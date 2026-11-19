"""A scale as the charge loop meets it: late, filtered, rounded, and reported at its own rhythm.

One chain for every make, a parameter set per make and setting:
pan load -> dead time -> sensor lag -> a filter with two speeds -> zero tracking -> rounding -> a report every
interval -> the link and the driver's poll. Weights in grains, times in seconds.

Only the A&D's report interval, readability and link figures are measured or documented; every time constant is an
estimate to be replaced by the bench's step and ramp tests (README.md).
"""

import random
from collections import deque
from dataclasses import dataclass


@dataclass(frozen=True)
class ScaleModel:
    """One make at one setting.

    digit_gr: readability, in grains.
    report_s, report_jitter_s: time between reports and its spread.
    dead_s: time before the pan's load shows at all.
    sensor_tau_s: the sensor's own lag.
    open_tau_s, closed_tau_s: the filter's lag while the weight moves, and while it nearly rests.
    open_above_gr: the filter opens when its input and output differ by more than this.
    noise_gr: sd of the noise before rounding.
    link_min_s, link_max_s: frame time and the driver's poll, uniform between the two.
    zero_track_gr, zero_track_gr_s: window around zero where tracking acts, and its rate (0 for off).
    miss_share: share of reports that never arrive.
    draught_per_min, draught_gr, draught_tau_s: gusts a minute, their size (sd) and how long each lasts.
    """

    name: str
    digit_gr: float
    report_s: float
    report_jitter_s: float
    dead_s: float
    sensor_tau_s: float
    open_tau_s: float
    closed_tau_s: float
    open_above_gr: float
    noise_gr: float
    link_min_s: float
    link_max_s: float
    zero_track_gr: float = 0.0
    zero_track_gr_s: float = 0.0
    miss_share: float = 0.0
    draught_per_min: float = 0.0
    draught_gr: float = 0.0
    draught_tau_s: float = 1.0


class ScaleState:
    """A scale running: feed it the pan's load each step, take the readings the driver would publish."""

    def __init__(self, model: ScaleModel, rng: random.Random, dt_s: float):
        self.model = model
        self.rng = rng
        self.dt = dt_s
        self.delay = deque([0.0] * max(1, round(model.dead_s / dt_s)))
        self.sensor = 0.0
        self.filtered = 0.0
        self.zero = 0.0
        self.gust = 0.0
        self.gust_left_s = 0.0
        self.next_report_s = rng.uniform(0.0, model.report_s)
        self.pending: deque = deque()   # (arrival time, value) on their way through the link

    def step(self, now_s: float, pan_gr: float):
        """Advance one step. Returns a reading (grains) when one reaches the charge loop, else None."""
        model = self.model
        dt = self.dt

        if model.draught_per_min > 0.0:
            if self.gust_left_s > 0.0:
                self.gust_left_s -= dt
            else:
                self.gust *= 1.0 - dt / model.draught_tau_s
                if self.rng.random() < model.draught_per_min / 60.0 * dt:
                    self.gust = self.rng.gauss(0.0, model.draught_gr)
                    self.gust_left_s = self.rng.expovariate(1.0 / model.draught_tau_s)
            pan_gr += self.gust

        self.delay.append(pan_gr)
        delayed = self.delay.popleft()
        self.sensor += (delayed - self.sensor) * dt / model.sensor_tau_s
        gap = self.sensor - self.filtered
        tau = model.open_tau_s if abs(gap) > model.open_above_gr else model.closed_tau_s
        self.filtered += gap * dt / tau

        shown = self.filtered - self.zero
        if model.zero_track_gr_s > 0.0 and abs(shown) <= model.zero_track_gr:
            step = model.zero_track_gr_s * dt
            self.zero += max(-step, min(step, shown))

        if now_s >= self.next_report_s:
            self.next_report_s += model.report_s + self.rng.uniform(-model.report_jitter_s, model.report_jitter_s)
            if self.rng.random() >= model.miss_share:
                value = shown + self.rng.gauss(0.0, model.noise_gr)
                value = round(value / model.digit_gr) * model.digit_gr
                self.pending.append((now_s + self.rng.uniform(model.link_min_s, model.link_max_s), value))

        if self.pending and self.pending[0][0] <= now_s:
            return self.pending.popleft()[1]
        return None


# The bench's A&D FX-300i in grains at FAST: 20.9 reports a second measured (ADR-013), 0.02 gr a digit and the link
# from the manual and the driver. Lags are estimates: the reading trails a fine trickle by about 0.35 s.
AND_FXI_FAST = ScaleModel(
    name="A&D FX-i, FAST", digit_gr=0.02, report_s=0.0478, report_jitter_s=0.002, dead_s=0.10, sensor_tau_s=0.03,
    open_tau_s=0.05, closed_tau_s=0.20, open_above_gr=0.10, noise_gr=0.005, link_min_s=0.009, link_max_s=0.029)

# The same balance at SLOW, which also drops it to 5 reports a second.
AND_FXI_SLOW = ScaleModel(
    name="A&D FX-i, SLOW", digit_gr=0.02, report_s=0.200, report_jitter_s=0.002, dead_s=0.10, sensor_tau_s=0.03,
    open_tau_s=0.12, closed_tau_s=0.80, open_above_gr=0.10, noise_gr=0.003, link_min_s=0.009, link_max_s=0.029)

# A G&G JJ223BF at the project's C1 = 1, C2 = 1, asked every 250 ms by a driver that reads the answer one ask late.
GNG_JJB = ScaleModel(
    name="G&G JJ-B, C1=1 C2=1", digit_gr=0.0154, report_s=0.250, report_jitter_s=0.005, dead_s=0.20,
    sensor_tau_s=0.03, open_tau_s=0.15, closed_tau_s=0.50, open_above_gr=0.08, noise_gr=0.010,
    link_min_s=0.235, link_max_s=0.250)

# A slow strain-gauge scale: 2 reports a second and a long filter (the US Solid's fastest stream, as a worst case).
SLOW_STRAIN_GAUGE = ScaleModel(
    name="Slow strain gauge, 2/s", digit_gr=0.0154, report_s=0.500, report_jitter_s=0.010, dead_s=0.20,
    sensor_tau_s=0.05, open_tau_s=0.30, closed_tau_s=1.20, open_above_gr=0.08, noise_gr=0.010,
    link_min_s=0.016, link_max_s=0.036)

SCALES = {"and_fast": AND_FXI_FAST, "and_slow": AND_FXI_SLOW, "gng": GNG_JJB, "slow": SLOW_STRAIN_GAUGE}
