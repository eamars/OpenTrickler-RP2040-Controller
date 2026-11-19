"""The candidate controller: flow by the weight still to go, then a finish in nudges on settled readings.

Its invariant: no tube feeds into what the scale cannot yet show. Each tube is asked for a flow the weight still
to go can absorb within the scale's delay, through that tube's measured flow map; a tube whose allowed flow falls
under the least it delivers regularly stops for good. The last kernels are added by small turns of the fine tube,
each judged on a settled reading. Nothing is tuned by hand: the calibration gives the maps and the delays, the
powder's entry gives the kernel's mass, the user gives the tolerance.
"""

from collections import deque
from dataclasses import dataclass

from calibrate import Calibration

GAIN_SHARE = 0.37           # a proportional loop round an integrator with a delay does not overshoot under 1/e
NUDGE_RPS = 0.5
MAX_NUDGES = 60
NUDGE_MEMORY_TURNS = 0.5    # the turns of nudging the yield is averaged over
RETRACT_TURNS = 0.05        # a reverse turn that takes back what hangs at the fine tube's lip
LEAST_NUDGE_KERNELS = 0.15  # no nudge is worth making for less
MOST_NUDGE_KERNELS = 3.0    # nor is more asked of one, however far short the weight is
BACKOFF_TURNS = 0.25
BACKOFF_RPS = 1.0


@dataclass(frozen=True)
class DoseSettings:
    """What is not measured by the calibration.

    kernel_gr: the powder's kernel, from its entry.
    tolerance_gr: a settled reading no more than this under the target ends the throw.
    fine_margin_kernels: continuous feeding by the fine tube ends this many kernels short.
    coarse_margin_gr: the coarse tube aims this far short.
    coarse_least_rps, fine_least_rps: the least speed each tube delivers regularly at.
    nudge_kernels: kernels a single nudge aims to release, on average.
    commit: count the powder the fine tube has pushed to its lip and the scale has not shown as delivered.
    """

    kernel_gr: float
    tolerance_gr: float = 0.02
    fine_margin_kernels: float = 3.0
    coarse_margin_gr: float = 1.0
    coarse_least_rps: float = 0.2
    fine_least_rps: float = 0.1
    nudge_kernels: float = 0.4
    commit: bool = False


class FlowMap:
    """A tube's flow at each speed, read both ways."""

    def __init__(self, points: list):
        speeds = [speed for speed, _ in points]
        flows = []
        for _, flow in points:
            flows.append(max(flow, flows[-1] if flows else 0.0))
        # the highest useful speed: the first that gives nearly all the tube can
        top = next(index for index, flow in enumerate(flows) if flow >= 0.95 * flows[-1])
        self.speeds = speeds[:top + 1]
        self.flows = flows[:top + 1]

    def flow_at(self, speed: float) -> float:
        """Flow at a speed, by straight lines between the measured points and through zero."""
        speeds, flows = self.speeds, self.flows
        if speed <= speeds[0]:
            return flows[0] * speed / speeds[0]
        for index in range(1, len(speeds)):
            if speed <= speeds[index]:
                share = (speed - speeds[index - 1]) / (speeds[index] - speeds[index - 1])
                return flows[index - 1] + share * (flows[index] - flows[index - 1])
        return flows[-1]

    def speed_for(self, flow: float) -> float:
        """Speed that gives a flow, held to the highest useful speed."""
        speeds, flows = self.speeds, self.flows
        if flow <= flows[0]:
            return speeds[0] * flow / flows[0]
        for index in range(1, len(speeds)):
            if flow <= flows[index]:
                if flows[index] == flows[index - 1]:
                    return speeds[index - 1]
                share = (flow - flows[index - 1]) / (flows[index] - flows[index - 1])
                return speeds[index - 1] + share * (speeds[index] - speeds[index - 1])
        return speeds[-1]


class FlowDose:
    """The candidate. The harness reads coarse_rps, fine_rps and done after each call."""

    name = "flow_dose"

    def __init__(self, calibration: Calibration, settings: DoseSettings):
        self.settings = settings
        self.coarse_map = FlowMap(calibration.coarse_map)
        self.fine_map = FlowMap(calibration.fine_map)
        self.coarse_delay = calibration.coarse_delay_s
        self.fine_delay = calibration.fine_delay_s
        self.digit = calibration.digit_gr
        # A loop that acts once a reading is blind for the reading's interval on top of the scale's delay
        self.coarse_blind = calibration.coarse_delay_s + calibration.interval_s
        self.fine_blind = calibration.fine_delay_s + calibration.interval_s
        self.settle_s = max(2.5 * calibration.fine_delay_s, 3.0 * calibration.interval_s)
        # What a nudge gives per turn, kept as two sums so one reading's rounding cannot swing it: the calibration's
        # nudges count as the first of them, and each throw's are added with the older ones fading
        slow = self.fine_map.flows[0] / self.fine_map.speeds[0]
        self.least_gr_per_turn = 0.3 * slow     # a run of empty nudges must not talk the yield down to nothing
        self.nudge_turned = NUDGE_MEMORY_TURNS
        self.nudge_gained = NUDGE_MEMORY_TURNS * max(calibration.nudge_gr_per_turn, 0.3 * slow)
        self.trim = 1.0     # what the fine tube gives in a throw against its map, learned throw to throw
        self.retracts = 0

    def start(self, target_gr: float, now_ms: float) -> None:
        """Begin a throw."""
        self.target = target_gr
        self.coarse_rps = 0.0
        self.fine_rps = 0.0
        self.done = False
        self.coarse_on = True
        self.fine_on = True
        self.readings = deque()         # (seconds, weight), the last second of them
        self.still_since = now_ms / 1000.0
        self.backoff_until = None
        self.nudge_until = None
        self.nudge_from = None          # the settled weight a nudge started from, and its turn
        self.nudge_turns = 0.0
        self.empty_nudges = 0
        self.nudges = 0
        # The fine tube's account once it feeds alone: what its map says it pushed out, what the scale has shown
        # of it, and the difference, which is still on its way: in the air, in the scale's filter, or at the lip
        self.owed = 0.0
        self.fed_expected = 0.0
        self.fed_from = None
        self.last_tick = now_ms / 1000.0
        self.last_weight = None

    def on_tick(self, now_ms: float) -> None:
        """Keep the fine tube's account, and end a back-off or a nudge on time."""
        now = now_ms / 1000.0
        elapsed = now - self.last_tick
        self.last_tick = now
        if self.fine_on and not self.coarse_on and self.backoff_until is None and self.fine_rps > 0.0:
            pushed = self.trim * self.fine_map.flow_at(self.fine_rps) * elapsed
            self.owed += pushed
            self.fed_expected += pushed
        if self.backoff_until is not None and now >= self.backoff_until:
            self.coarse_rps = 0.0
            self.backoff_until = None
            self.still_since = now
        if self.nudge_until is not None and now >= self.nudge_until:
            self.fine_rps = 0.0
            self.nudge_until = None
            self.still_since = now

    def on_reading(self, now_ms: float, weight: float) -> None:
        """One reading: feed by the weight still to go, or judge a settled reading and nudge."""
        now = now_ms / 1000.0
        readings = self.readings
        readings.append((now, weight))
        while now - readings[0][0] > 1.0:
            readings.popleft()

        if not self.coarse_on and self.backoff_until is None:
            if self.fed_from is None:
                self.fed_from = weight
            elif self.fine_on and weight > self.last_weight:
                self.owed = max(0.0, self.owed - (weight - self.last_weight))
        self.last_weight = weight

        if self.coarse_on or self.fine_on:
            self._feed(now, weight)
        elif self.backoff_until is None and self.nudge_until is None and self._settled(now):
            self._judge(now, self._settled_weight(now))

    def _feed(self, now: float, weight: float) -> None:
        """Continuous feeding: each tube's flow in proportion to the weight it may still add."""
        settings = self.settings

        if self.coarse_on:
            room = self.target - weight - settings.coarse_margin_gr
            speed = self.coarse_map.speed_for(max(0.0, GAIN_SHARE / self.coarse_blind * room))
            if speed < settings.coarse_least_rps:
                self.coarse_on = False
                self.coarse_rps = -BACKOFF_RPS
                self.backoff_until = now + BACKOFF_TURNS / BACKOFF_RPS
            else:
                self.coarse_rps = speed

        # With the account kept, the weight acted on is the committed one: the reading plus what is on its way.
        # That takes the scale's delay out of the loop, so the gain need not be held to a third of it
        committed = weight + self.owed if settings.commit else weight
        room = self.target - committed - settings.fine_margin_kernels * settings.kernel_gr
        gain = (1.0 if settings.commit and not self.coarse_on else GAIN_SHARE) / self.fine_blind
        speed = self.fine_map.speed_for(max(0.0, gain * room) / self.trim)
        if speed < settings.fine_least_rps and not self.coarse_on:
            self.fine_on = False
            self.fine_rps = 0.0
            self.still_since = now
        else:
            self.fine_rps = max(speed, settings.fine_least_rps)

    def _settled(self, now: float) -> bool:
        """Nothing has turned for the settle time, and the readings in it agree within a digit."""
        if now - self.still_since < self.settle_s:
            return False
        recent = [w for t, w in self.readings if t >= now - self.settle_s]
        return len(recent) >= 2 and max(recent) - min(recent) <= 1.5 * self.digit

    def _settled_weight(self, now: float) -> float:
        """The settled weight: the mean of the readings in the settle time, finer than one reading's digit."""
        recent = [w for t, w in self.readings if t >= now - self.settle_s]
        return sum(recent) / len(recent)

    def _judge(self, now: float, weight: float) -> None:
        """A settled weight: learn what the last nudge gave, then stop or nudge again."""
        settings = self.settings
        kernel = settings.kernel_gr
        if self.nudge_from is None and self.fed_expected > 0.0:
            # The first settled weight after the continuous feed: what the fine tube really gave against its map
            ratio = max(0.0, weight - self.fed_from) / self.fed_expected
            self.trim = min(2.0, max(0.2, self.trim * (1.0 + 0.3 * (ratio - 1.0))))
            self.fed_expected = 0.0
            self.owed = 0.0
        if self.nudge_from is not None:
            gained = max(0.0, weight - self.nudge_from)
            self.owed = max(0.0, self.owed - gained)
            keep = 1.0 - self.nudge_turns / NUDGE_MEMORY_TURNS if self.nudge_turns < NUDGE_MEMORY_TURNS else 0.0
            self.nudge_turned = self.nudge_turned * keep + self.nudge_turns
            self.nudge_gained = self.nudge_gained * keep + gained
            self.empty_nudges = 0 if gained > 0.5 * kernel else self.empty_nudges + 1

        short = self.target - weight
        if short <= settings.tolerance_gr + 1e-9 or self.nudges >= MAX_NUDGES:
            self.done = True
            return

        wanted = settings.nudge_kernels
        if short > 4.0 * kernel:
            wanted = min(MOST_NUDGE_KERNELS, 0.7 * (short - 2.0 * kernel) / kernel)
        if settings.commit:
            # Never push to the lip more than the band can take: what hangs there falls together
            wanted = min(wanted, (short + settings.tolerance_gr - self.owed) / kernel)
            if wanted < LEAST_NUDGE_KERNELS:
                self.retracts += 1
                self.owed = 0.0
                self.nudge_from = None
                self.fine_rps = -NUDGE_RPS
                self.nudge_until = now + RETRACT_TURNS / NUDGE_RPS
                return
            self.owed += wanted * kernel
        turns = wanted * kernel / max(self.least_gr_per_turn, self.nudge_gained / self.nudge_turned)
        self.nudge_from = weight
        self.nudge_turns = turns
        self.nudges += 1
        self.fine_rps = NUDGE_RPS
        self.nudge_until = now + turns / NUDGE_RPS
