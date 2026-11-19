"""The plant of a throw: two rotary tubes dropping kernels into the cup.

Weights are in grains, times in seconds, speeds in trickler rev/s as the firmware takes them (it divides by the
belt ratio itself). The model's shape follows the tubes' geometry and the flow research (README.md); its numbers
are priors until open-loop recordings on a real unit replace them.
"""

import heapq
import math
import random
from dataclasses import dataclass

GRAVITY_M_S2 = 9.80665
GR_PER_G = 15.4323584


@dataclass(frozen=True)
class Powder:
    """What the plant needs to know of a powder.

    kernel_gr: mean mass of one kernel.
    kernel_cv: spread of a kernel's mass, as a share of the mean.
    clump_mean: mean kernels that leave the tube together at speed; 1 is one at a time.
    regularity: order of the gamma-distributed gaps between releases. 1 is a Poisson drip, higher is steadier.
    flow_scale: this powder's flow against the reference powder's, for both tubes (bulk density, how it packs).
    """

    name: str
    kernel_gr: float
    kernel_cv: float
    clump_mean: float
    regularity: float
    flow_scale: float = 1.0


@dataclass(frozen=True)
class Tube:
    """One rotary tube with the reference powder in it.

    out_per_rev: share of the powder held in the tube that leaves per turn (outflow = out_per_rev x speed x held).
    in_per_rev: share of the tube's free room that the inlet fills per turn.
    held_max_gr: what the tube holds when the inlet no longer adds to it.
    max_inflow_gr_s: ceiling of the inlet (inf for none): the small tube fills through one window a turn.
    critical_rps: speed at which the bore's wall holds the powder by centrifugal force.
    centrifuge_power: how sharply the flow per turn falls toward the critical speed.
    ripple: once-a-turn variation of the flow, 0 to 1 (the window, a seam, eccentricity).
    slump_deg: turn of the tube between avalanches of the bed at low speed.
    roll_rps: speed above which the avalanches merge into a steady stream.
    start_turns: forward turns that bring a retreated front back to the lip.
    retreat_turns: reverse turns that take the front fully back.
    trail_kernels: mean kernels that fall after a stop from trail_ref_rps or faster.
    trail_tau_s: time constant of those late kernels.
    drop_mm: fall from the lip to the cup.
    roll_tau_s: time the bed takes to settle into its speed's way of flowing. A tube that has just started, or is
        nudged, goes on avalanching for about this long whatever its speed; 0 for at once.
    trail_full_turns: a tube that turned less than this since its start has brought fewer kernels to the lip, and
        fewer fall after its stop.
    """

    name: str
    out_per_rev: float
    in_per_rev: float
    held_max_gr: float
    max_inflow_gr_s: float
    critical_rps: float
    centrifuge_power: float
    ripple: float
    slump_deg: float
    roll_rps: float
    start_turns: float
    retreat_turns: float
    trail_kernels: float
    trail_ref_rps: float
    trail_tau_s: float
    drop_mm: float
    roll_tau_s: float = 0.0
    trail_full_turns: float = 0.25

    @property
    def held_steady_gr(self) -> float:
        """What the tube holds at a steady slow speed, where the inlet keeps up."""
        return self.held_max_gr * self.in_per_rev / (self.in_per_rev + self.out_per_rev)

    @property
    def gr_per_rev(self) -> float:
        """Powder delivered per turn at a steady slow speed."""
        return self.out_per_rev * self.held_steady_gr


class TubeState:
    """A tube turning: its ramped speed, the powder it holds, where its front is, and the kernels it releases."""

    def __init__(self, tube: Tube, powder: Powder, rng: random.Random, acceleration_rps2: float, max_speed_rps: float):
        self.tube = tube
        self.powder = powder
        self.rng = rng
        self.acceleration = acceleration_rps2
        self.max_speed = max_speed_rps
        self.target = 0.0
        self.speed = 0.0
        self.rolling = 0.0          # the speed the bed flows as if at: the speed itself, trailing by roll_tau_s
        self.angle = rng.random()
        self.front = 1.0            # 1: powder at the lip; 0: fully retreated
        self.held = tube.held_steady_gr * powder.flow_scale
        self.budget = 0.0           # powder brought to the lip and not yet released
        self.clump = 0.0            # the next release, and the budget that lets it go
        self.threshold = 0.0
        self.fall_s = math.sqrt(2.0 * tube.drop_mm / 1000.0 / GRAVITY_M_S2)
        self.land_speed = GRAVITY_M_S2 * self.fall_s
        self.speed_before_stop = 0.0
        self.released_gr = 0.0
        self.turns = 0.0
        self.turns_since_start = 0.0
        self._next_release()

    def _kernel(self) -> float:
        """Mass of one kernel, never under a fifth of the mean."""
        mass = self.rng.gauss(self.powder.kernel_gr, self.powder.kernel_gr * self.powder.kernel_cv)
        return max(mass, 0.2 * self.powder.kernel_gr)

    def _next_release(self) -> None:
        """Draw the next release: its kernels, and the powder the bed must bring to the lip before it goes.

        At speed a release is the powder's own clump. Turning slowly the bed avalanches, and a release is what
        one avalanche carries over the lip when that is more.
        """
        powder = self.powder
        tube = self.tube
        slumping = 1.0 / (1.0 + (self.rolling / tube.roll_rps) ** 2)
        avalanche = tube.out_per_rev * self.held * tube.slump_deg / 360.0 * slumping
        mean_kernels = max(powder.clump_mean, avalanche / powder.kernel_gr)
        count = 1
        extra = mean_kernels - 1.0
        if extra > 0.0:
            stay = extra / (1.0 + extra)
            while self.rng.random() < stay:
                count += 1
        self.clump = sum(self._kernel() for _ in range(count))
        order = powder.regularity
        self.threshold = self.clump * self.rng.gammavariate(order, 1.0 / order)

    def set_speed(self, speed_rps: float) -> None:
        """Ask for a speed, as motor_set_speed does: held to the motor's limit either way."""
        speed_rps = max(-self.max_speed, min(self.max_speed, speed_rps))
        if speed_rps <= 0.0 < self.target:
            self.speed_before_stop = self.speed
        self.target = speed_rps

    def mean_flow(self) -> float:
        """Mean discharge in gr/s at the present speed, front, angle and what the tube holds."""
        speed = self.speed
        if speed <= 0.0:
            return 0.0
        tube = self.tube
        flow = tube.out_per_rev * speed * self.held
        flow *= max(0.0, 1.0 - (speed / tube.critical_rps) ** tube.centrifuge_power)
        flow *= self.front * self.front
        flow *= 1.0 + tube.ripple * math.cos(2.0 * math.pi * self.angle)
        return flow

    def step(self, now_s: float, dt_s: float, landings: list) -> None:
        """Advance one time step, pushing (landing time, mass, landing speed) for each release onto the heap."""
        change = self.acceleration * dt_s
        was_moving = self.speed > 0.0
        if self.speed < self.target:
            self.speed = min(self.target, self.speed + change)
        elif self.speed > self.target:
            self.speed = max(self.target, self.speed - change)
        if self.tube.roll_tau_s > 0.0:
            self.rolling += (abs(self.speed) - self.rolling) * dt_s / self.tube.roll_tau_s
        else:
            self.rolling = abs(self.speed)
        turned = self.speed * dt_s
        self.angle = (self.angle + turned) % 1.0
        self.turns += abs(turned)

        if turned > 0.0:
            tube = self.tube
            scale = self.powder.flow_scale
            self.front = min(1.0, self.front + turned / tube.start_turns)
            inflow = tube.in_per_rev * self.speed * max(0.0, tube.held_max_gr * scale - self.held)
            self.held += min(inflow, tube.max_inflow_gr_s * scale) * dt_s
            self.budget += self.mean_flow() * dt_s
            while self.budget >= self.threshold:
                self.budget -= self.clump
                self.held = max(0.0, self.held - self.clump)
                self._release(now_s, self.clump, landings)
                self._next_release()
        elif turned < 0.0:
            self.front = max(0.0, self.front + turned / self.tube.retreat_turns)
            self.budget = min(self.budget, 0.0)

        if self.speed > 0.0:
            self.turns_since_start = self.turns_since_start + turned if was_moving else turned
        if was_moving and self.speed <= 0.0:
            self._trail(now_s, landings)

    def _release(self, now_s: float, mass: float, landings: list) -> None:
        """Send one clump on its way to the cup."""
        self.released_gr += mass
        fall = self.fall_s * self.rng.uniform(0.95, 1.05)
        heapq.heappush(landings, (now_s + fall, mass, self.land_speed))

    def _trail(self, now_s: float, landings: list) -> None:
        """Kernels left at the lip that fall after the tube stops; fewer from a slow stop or a retreated front.

        What falls is owed: the next turn first brings as much back to the lip before anything leaves it.
        """
        tube = self.tube
        share = min(1.0, self.speed_before_stop / tube.trail_ref_rps) * self.front
        share *= min(1.0, self.turns_since_start / tube.trail_full_turns)
        mean = tube.trail_kernels * share
        count = 0
        # Poisson by multiplication: the means here are a few kernels at most
        limit = math.exp(-mean)
        product = self.rng.random()
        while product > limit:
            count += 1
            product *= self.rng.random()
        for _ in range(count):
            delay = self.rng.expovariate(1.0 / tube.trail_tau_s)
            kernel = self._kernel()
            self.held = max(0.0, self.held - kernel)
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
