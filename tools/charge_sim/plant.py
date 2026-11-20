"""The plant of a throw: two rotary tubes dropping kernels into the cup.

Weights are in grains, times in seconds, speeds in trickler rev/s as the firmware takes them (it divides by the
belt ratio itself). A tube is described by what a calibration measures of it with a powder in it: its flow a turn
at each speed, the size of its releases at each speed, what falls after a stop from each speed. Nothing here is a
figure of a tube's geometry that a unit could not measure through its scale. `build_rig.py` makes the tubes from a
bench's recordings; `records/` holds them.
"""

import heapq
import math
import random
from dataclasses import dataclass

GRAVITY_M_S2 = 9.80665
GR_PER_G = 15.4323584


def table_value(table: tuple, x: float) -> float:
    """
    A measured table read at x.

    Args:
        table: ((x, y), ...) by rising x, every x above 0.
        x: where to read it.

    Returns:
        y, straight between two points on a log scale of x, and the end point's outside them: nothing was
        measured there.
    """
    if x <= table[0][0]:
        return table[0][1]
    if x >= table[-1][0]:
        return table[-1][1]
    for (x0, y0), (x1, y1) in zip(table, table[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * math.log(x / x0) / math.log(x1 / x0)
    return table[-1][1]


@dataclass(frozen=True)
class Powder:
    """What the plant needs to know of a powder apart from any tube.

    kernel_gr: mean mass of one kernel.
    kernel_cv: spread of a kernel's mass, as a share of the mean.
    """

    name: str
    kernel_gr: float
    kernel_cv: float


@dataclass(frozen=True)
class Tube:
    """One rotary tube with one powder in it, as a calibration measures the two together.

    gr_per_turn: the flow map, ((speed, grains a turn), ...): one point when the flow follows the speed.
    burst_kernels: ((speed, mean kernels that leave together), ...): 1 is a kernel at a time. Slow turning lets
        the bed avalanche, so the low speeds hold the large figures.
    after_stop_kernels: ((speed before the stop, mean kernels that fall after it), ...).
    regularity: order of the gamma-distributed gaps between releases. 1 is a Poisson drip, higher is steadier.
    settle_s: time the bed takes to come out of its slow way of flowing. A tube that has just started, or is
        nudged, goes on releasing as its lowest speed does for about this long; 0 for at once. A tube that slows
        down takes up the slower speed's way at once: avalanches begin as soon as the turning no longer carries
        the bed.
    after_stop_tau_s: time constant of the kernels that fall after a stop.
    after_stop_full_turns: a tube that turned less than this since its start has brought fewer kernels to the
        lip, and fewer fall after its stop.
    ripple_turn: once-a-turn variation of the flow, 0 to 1 (the window, a seam, eccentricity).
    rifling_starts, ripple_rifling: the tube's rifling starts (5 on both of the project's tubes) and the depth of
        a variation of the flow once a start. `bench/fold_by_angle.py` measures both ripples on a unit.
    reverse_spill_gr: powder that falls when a tube that was turning forward is turned back (the charge's
        back-off of the coarse tube). Mean, in grains.
    reverse_spill_cv, reverse_spill_s: spread of that spill as a share of its mean, and the time it falls over.
    start_turns: forward turns that bring a retreated front back to the lip.
    retreat_turns: reverse turns that take the front fully back.
    drop_mm: fall from the lip to the cup.
    """

    name: str
    gr_per_turn: tuple
    burst_kernels: tuple
    after_stop_kernels: tuple
    regularity: float = 2.0
    settle_s: float = 0.0
    after_stop_tau_s: float = 0.2
    after_stop_full_turns: float = 0.1
    ripple_turn: float = 0.0
    rifling_starts: int = 5
    ripple_rifling: float = 0.0
    reverse_spill_gr: float = 0.0
    reverse_spill_cv: float = 0.3
    reverse_spill_s: float = 0.25
    start_turns: float = 0.2
    retreat_turns: float = 0.5
    drop_mm: float = 50.0


class TubeState:
    """A tube turning: its ramped speed, where its front is, and the kernels it releases."""

    def __init__(self, tube: Tube, powder: Powder, rng: random.Random, acceleration_rps2: float, max_speed_rps: float):
        self.tube = tube
        self.powder = powder
        self.rng = rng
        self.acceleration = acceleration_rps2
        self.max_speed = max_speed_rps
        self.target = 0.0
        self.speed = 0.0
        self.rolling = 0.0          # the speed the bed flows as if at: the lower of the speed and its trail
        self.trailing = 0.0         # the speed, trailing by settle_s
        self.angle = rng.random()
        self.front = 1.0            # 1: powder at the lip; 0: fully retreated
        self.budget = 0.0           # powder brought to the lip and not yet released
        self.clump = 0.0            # the next release, and the budget that lets it go
        self.threshold = 0.0
        self.fall_s = math.sqrt(2.0 * tube.drop_mm / 1000.0 / GRAVITY_M_S2)
        self.land_speed = GRAVITY_M_S2 * self.fall_s
        self.speed_before_stop = 0.0
        self.released_gr = 0.0
        self.turns = 0.0
        self.turns_since_start = 0.0
        self.forward_since_reverse = 0.0
        self.map_speed = -1.0       # the speed the flow map was last read at, and what it gave
        self.map_gr_per_turn = 0.0
        self._next_release()

    def _kernel(self) -> float:
        """Mass of one kernel, never under a fifth of the mean."""
        mass = self.rng.gauss(self.powder.kernel_gr, self.powder.kernel_gr * self.powder.kernel_cv)
        return max(mass, 0.2 * self.powder.kernel_gr)

    def _next_release(self) -> None:
        """Draw the next release: its kernels, and the powder the bed must bring to the lip before it goes."""
        mean_kernels = table_value(self.tube.burst_kernels, self.rolling)
        count = 1
        extra = mean_kernels - 1.0
        if extra > 0.0:
            stay = extra / (1.0 + extra)
            while self.rng.random() < stay:
                count += 1
        self.clump = sum(self._kernel() for _ in range(count))
        order = self.tube.regularity
        self.threshold = self.clump * self.rng.gammavariate(order, 1.0 / order)

    def set_speed(self, speed_rps: float) -> None:
        """Ask for a speed, as motor_set_speed does: held to the motor's limit either way."""
        speed_rps = max(-self.max_speed, min(self.max_speed, speed_rps))
        if speed_rps <= 0.0 < self.target:
            self.speed_before_stop = self.speed
        self.target = speed_rps

    def mean_flow(self) -> float:
        """Mean discharge in gr/s at the present speed, front and angle."""
        speed = self.speed
        if speed <= 0.0:
            return 0.0
        tube = self.tube
        if speed != self.map_speed:
            self.map_speed = speed
            self.map_gr_per_turn = table_value(tube.gr_per_turn, speed)
        flow = self.map_gr_per_turn * speed * self.front * self.front
        turn = 2.0 * math.pi * self.angle
        return flow * max(0.0, 1.0 + tube.ripple_turn * math.cos(turn)
                          + tube.ripple_rifling * math.cos(tube.rifling_starts * turn))

    def step(self, now_s: float, dt_s: float, landings: list) -> None:
        """Advance one time step, pushing (landing time, mass, landing speed) for each release onto the heap."""
        change = self.acceleration * dt_s
        was_moving = self.speed > 0.0
        if self.speed < self.target:
            self.speed = min(self.target, self.speed + change)
        elif self.speed > self.target:
            self.speed = max(self.target, self.speed - change)
        if self.tube.settle_s > 0.0:
            self.trailing += (abs(self.speed) - self.trailing) * dt_s / self.tube.settle_s
            self.rolling = min(self.trailing, abs(self.speed))
        else:
            self.rolling = abs(self.speed)
        turned = self.speed * dt_s
        self.angle = (self.angle + turned) % 1.0
        self.turns += abs(turned)

        if turned > 0.0:
            self.front = min(1.0, self.front + turned / self.tube.start_turns)
            self.budget += self.mean_flow() * dt_s
            while self.budget >= self.threshold:
                self.budget -= self.clump
                self._release(now_s, self.clump, landings)
                self._next_release()
        elif turned < 0.0:
            self.front = max(0.0, self.front + turned / self.tube.retreat_turns)
            self.budget = min(self.budget, 0.0)
            if self.forward_since_reverse > 0.0:
                self._spill(now_s, landings)
            self.forward_since_reverse = 0.0
        if turned > 0.0:
            self.forward_since_reverse += turned

        if self.speed > 0.0:
            self.turns_since_start = self.turns_since_start + turned if was_moving else turned
        if was_moving and self.speed <= 0.0:
            self._trail(now_s, landings)

    def _release(self, now_s: float, mass: float, landings: list) -> None:
        """Send one clump on its way to the cup."""
        self.released_gr += mass
        fall = self.fall_s * self.rng.uniform(0.95, 1.05)
        heapq.heappush(landings, (now_s + fall, mass, self.land_speed))

    def _spill(self, now_s: float, landings: list) -> None:
        """What a reverse turn shakes off the lip of a tube that was pouring: in ten parts over the spill's time."""
        tube = self.tube
        if tube.reverse_spill_gr <= 0.0:
            return
        mass = max(0.0, self.rng.gauss(tube.reverse_spill_gr, tube.reverse_spill_gr * tube.reverse_spill_cv))
        for _ in range(10):
            self._release(now_s + self.rng.uniform(0.0, tube.reverse_spill_s), mass / 10.0, landings)

    def _trail(self, now_s: float, landings: list) -> None:
        """Kernels left at the lip that fall after the tube stops; fewer from a retreated front or a short turn.

        What falls is owed: the next turn first brings as much back to the lip before anything leaves it.
        """
        tube = self.tube
        share = self.front * min(1.0, self.turns_since_start / tube.after_stop_full_turns)
        mean = table_value(tube.after_stop_kernels, self.speed_before_stop) * share
        # Often none and now and then many, as the bench's stops gave (0 to 10 digits): a geometric count
        count = 0
        while self.rng.random() < mean / (1.0 + mean):
            count += 1
        for _ in range(count):
            delay = self.rng.expovariate(1.0 / tube.after_stop_tau_s)
            kernel = self._kernel()
            self.budget -= kernel
            self._release(now_s + delay, kernel, landings)


class Cup:
    """The cup on the pan: what has landed, and the push of what is landing now."""

    def __init__(self):
        self.landings: list = []
        self.landed_gr = 0.0

    def step(self, now_s: float, dt_s: float) -> float:
        """Land what is due and give the pan's load in gr: the mass landed plus this step's impact as a mass."""
        impulse = 0.0
        landings = self.landings
        while landings and landings[0][0] <= now_s:
            _, mass, speed = heapq.heappop(landings)
            self.landed_gr += mass
            impulse += mass * speed
        return self.landed_gr + impulse / (GRAVITY_M_S2 * dt_s)

    def in_flight_gr(self) -> float:
        """Powder released and not yet landed."""
        return sum(item[1] for item in self.landings)
