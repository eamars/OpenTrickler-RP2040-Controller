"""The calibration a unit would run for a powder: each tube at a few speeds with a stop after each, then nudges.

It is written as a controller, so the same harness runs it against the simulated plant; on a unit the same steps
give the same numbers. What it finds:
- per tube, the flow at each speed (the flow map);
- the scale's effective delay: seconds of flow that show only after a stop (reading lag, fall and late kernels);
- the scale's settle time after a stop;
- what a small turn of the fine tube releases (the nudge test).
"""

from collections import deque
from dataclasses import dataclass, field

COARSE_SPEEDS = (0.2, 0.5, 1.0, 2.0, 3.5, 5.0)
FINE_SPEEDS = (0.1, 0.2, 0.5, 1.0, 2.0, 5.0)
RUN_S = 4.0                 # a step runs this long,
RUN_GR = 20.0               # or until this much has fallen
SETTLE_STEADY_S = 0.6       # settled: this long within a digit
SETTLE_LIMIT_S = 5.0
NUDGE_TURNS = 0.02
NUDGE_RPS = 0.5
NUDGES = 30


@dataclass
class Calibration:
    """What the calibration found. Flows in gr/s, speeds in rev/s, times in seconds, weights in grains."""

    coarse_map: list = field(default_factory=list)      # (speed, flow), by rising speed
    fine_map: list = field(default_factory=list)
    coarse_delay_s: float = 0.0     # seconds of a tube's flow that show only after its stop: the scale's lag,
    fine_delay_s: float = 0.0       # the fall, and the kernels that leave late
    settle_s: float = 0.0           # longest time a stop took to settle
    digit_gr: float = 0.0           # the smallest step seen between readings
    interval_s: float = 0.0         # the usual time between readings
    nudge_gr_per_turn: float = 0.0  # what the fine tube gives per turn in small moves
    nudge_gains: list = field(default_factory=list)


class CalibrationRun:
    """Runs the steps one after another. The harness drives it as it drives a charge's controller."""

    name = "calibration"

    def __init__(self):
        self.result = Calibration()

    def start(self, target_gr: float, now_ms: float) -> None:
        """Begin; the target is not used."""
        self.coarse_rps = 0.0
        self.fine_rps = 0.0
        self.done = False
        self.steps = ([("coarse", speed) for speed in COARSE_SPEEDS] + [("fine", speed) for speed in FINE_SPEEDS]
                      + [("nudge", NUDGE_RPS)] * NUDGES)
        self.index = -1
        self.readings = deque()         # (seconds, weight) of the present step's run
        self.recent = deque()           # readings of the last SETTLE_STEADY_S, for the settle test
        self.late = {"coarse": [0.0, 0.0], "fine": [0.0, 0.0]}     # per tube: weight shown late, flow at the stops
        self.run_s = 0.0
        self.state = "settle"
        self.state_since = 0.0
        self.weight = 0.0
        self.step_start_weight = 0.0
        self.stop_weight = 0.0
        self.stop_flow = 0.0
        self.last_weight = None
        self.last_reading_s = None
        self.intervals = []

    def on_tick(self, now_ms: float) -> None:
        """End a nudge's move on time; a run's end is taken on a reading."""
        now = now_ms / 1000.0
        if self.state == "run" and self.steps[self.index][0] == "nudge":
            if now - self.state_since >= NUDGE_TURNS / NUDGE_RPS:
                self._stop(now)

    def on_reading(self, now_ms: float, weight: float) -> None:
        """Follow the step in hand: run it, stop it, wait for the scale, note what it showed."""
        now = now_ms / 1000.0
        if self.last_weight is not None and weight != self.last_weight:
            step = abs(weight - self.last_weight)
            if self.result.digit_gr == 0.0 or step < self.result.digit_gr:
                self.result.digit_gr = step
        self.last_weight = weight
        if self.last_reading_s is not None:
            self.intervals.append(now - self.last_reading_s)
        self.last_reading_s = now
        self.weight = weight
        self.recent.append((now, weight))
        while now - self.recent[0][0] > SETTLE_STEADY_S:
            self.recent.popleft()

        if self.state == "run":
            self.readings.append((now, weight))
            kind = self.steps[self.index][0]
            if kind != "nudge" and (now - self.state_since >= RUN_S or weight - self.step_start_weight >= RUN_GR):
                self._stop(now)
        elif self.state == "settle":
            spread = max(w for _, w in self.recent) - min(w for _, w in self.recent)
            steady = now - self.state_since >= SETTLE_STEADY_S and spread <= 1.5 * self.result.digit_gr
            if steady or now - self.state_since >= SETTLE_LIMIT_S:
                self._note(now)
                self._next(now)

    def _stop(self, now: float) -> None:
        """Stop the tube in hand and keep what the scale showed at that moment."""
        self.run_s = now - self.state_since
        self.stop_weight = self.weight
        self.coarse_rps = 0.0
        self.fine_rps = 0.0
        self.state = "settle"
        self.state_since = now

    def _note(self, now: float) -> None:
        """A step has settled: write down what it gave."""
        if self.index < 0:
            return
        kind, speed = self.steps[self.index]
        if kind == "nudge":
            self.result.nudge_gains.append(self.weight - self.step_start_weight)
            return
        # The flow is what the step added over the time it ran: it needs no reading during the run, so a scale
        # that reports twice a second measures it as well as a fast one
        flow = (self.weight - self.step_start_weight) / self.run_s
        (self.result.coarse_map if kind == "coarse" else self.result.fine_map).append((speed, flow))
        self.late[kind][0] += self.weight - self.stop_weight
        self.late[kind][1] += flow
        self.result.settle_s = max(self.result.settle_s, now - self.state_since - SETTLE_STEADY_S)

    def _next(self, now: float) -> None:
        """Start the next step, or finish."""
        self.index += 1
        if self.index >= len(self.steps):
            self._finish()
            return
        kind, speed = self.steps[self.index]
        self.readings.clear()
        self.step_start_weight = self.weight
        self.state = "run"
        self.state_since = now
        if kind == "coarse":
            self.coarse_rps = speed
        else:
            self.fine_rps = speed

    def _finish(self) -> None:
        """Sum up and end."""
        result = self.result
        result.coarse_delay_s = self.late["coarse"][0] / self.late["coarse"][1]
        result.fine_delay_s = self.late["fine"][0] / self.late["fine"][1]
        result.interval_s = sorted(self.intervals)[len(self.intervals) // 2]
        result.nudge_gr_per_turn = sum(result.nudge_gains) / len(result.nudge_gains) / NUDGE_TURNS
        self.done = True


def calibrate(session) -> Calibration:
    """Run the calibration on a session's rig and give what it found."""
    run = CalibrationRun()
    session.throw(run, 0.0, 0.0, timeout_s=600.0)
    return run.result
