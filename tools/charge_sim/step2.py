"""Step 2's charge on the twin (ADR-009, step 2, gate 1): the predictor serving the fine tube alone, below the coarse
tube's threshold, against today's loop with the same profile.

- **The coarse tube** is today's: its PID on the reading's error to the target less its threshold, stopped for good
  once the reading reaches it, then its back-off (legacy_pid.py's port of charge_mode.cpp).
- **The fine tube's forecast:** the reading plus the fine tube's turns turned out and not yet shown (10 ms parcels of
  its speed at the motor's ramp, each shown after step 1's lag at its flow) times its weight a turn. The weight a turn
  is the throw's own, the slope of the reading on the fine turns shown since the coarse tube's powder went quiet (the
  coarse tube's stop, its lag and step 1's settle time); before it is known, `before` says what is taken: nothing
  (the reading alone), or the last throw's (RAM, nothing stored; the first throw of a start has none).
- **The fine tube's PID** is the profile's gains on the forecast's error, at step 1's least speed and the profile's most,
  within its envelope: the integral held while the speed is held at a limit, and the speed held under what puts no
  more powder in the air than the room left (its least speed plus the room over the weight a turn and the lag). It
  stops the moment the forecast reaches the target (checked each millisecond, not only on a reading). No top-up.
- **Step 1's records** (the settle time, the lag by flow, each tube's least speed) are made by `calibration.py`'s
  parts on the same rig, a fresh bench a session.

Judged on the final weight (all landed) less the target: its spread first, then the time to the stop.

    venv\\Scripts\\python tools\\charge_sim\\step2.py [--rigs ...] [--targets 10] [--throws 300]
"""

import argparse
import collections
import dataclasses
import statistics
from dataclasses import dataclass

import numpy

import calibration
import rigs
from bench_twin import TwinBench
from legacy_pid import AR2209_PICOW_168, LegacyPid, Profile
from throw import Session
from workers import Workers

THROWS_PER_SESSION = 25
PARCEL_S = 0.01
KNOWN_TURNS = 0.15          # fine turns shown since the quiet before the throw's own weight a turn is taken
KNOWN_ROWS = 5
REST_TURNS = 0.2            # fine turns turned out after the quiet for a throw's own, at rest, to be carried
FIT_ROUNDS = 4              # the weight a turn and its lags solved together
CARRY_THROWS = 5            # the throws whose own weights a turn are carried (their mean)
DEFAULT_RIGS = ("pico2w_ar2209_night", "pico2w_ar2209_f2", "pico2w_bm2", "pico2w_ar2209_night+fine_by_speed")


@dataclass(frozen=True)
class Records:
    """Step 1's records of a unit and a powder, as the firmware keeps them."""

    settle_s: float
    lag: tuple                  # ((flow gr/s, lag s), ...) by rising flow
    fine_least_rps: float
    coarse_least_rps: float
    fine_gr_turn: float = None  # the powder part's own fine weight a turn where it streams (the prime's); None: not kept


def records_of(rig, seed: int) -> Records:
    """Step 1's three parts, each on a fresh bench of the rig, as calibration.py's main runs them."""
    least = {}
    stream = {}
    for tube, limits in (("fine", (0.01, 5.0)), ("coarse", (0.08, 5.0))):
        report = calibration.speeds_part(TwinBench(rig, seed), calibration.Cup(), tube, *limits)
        least[tube] = report.records.get("least_rps", limits[0])
        stream[tube] = report.records.get("stream_gr_turn")
    report = calibration.scale_part(TwinBench(rig, seed), calibration.Cup())
    return Records(settle_s=report.records.get("settle_s", 1.0), lag=tuple(report.records.get("lag", ((1.0, 0.8),))),
                   fine_least_rps=least["fine"], coarse_least_rps=least["coarse"],
                   fine_gr_turn=stream["fine"] if stream["fine"] and stream["fine"] > 0.0 else None)


def lag_at(lag: tuple, flow: float) -> float:
    """Step 1's lag table read at a flow: straight between its points, the end's value beyond them."""
    if flow <= lag[0][0]:
        return lag[0][1]
    for (x0, y0), (x1, y1) in zip(lag, lag[1:]):
        if flow <= x1:
            return y0 + (y1 - y0) * (flow - x0) / (x1 - x0)
    return lag[-1][1]


class Step2(LegacyPid):
    """Step 2's charge: today's coarse tube, the fine tube's PID on its forecast. The harness reads coarse_rps,
    fine_rps and done after each call."""

    name = "step2"

    def __init__(self, profile: Profile, records: Records, acceleration_rps2: float, before: str = "none",
                 envelope: bool = True, seed: bool = False, hold: str = "none", cut: str = "forecast"):
        """
        seed: with nothing carried, the forecast takes step 1's own weight a turn (Records.fine_gr_turn) until the
            throws' own replace it (2026-11-26: without it a fast profile never teaches one).
        hold: "none"; "landed", the fine tube at its least and no stop until the coarse tube's powder has landed (its
            stop, the back-off and the slowest lag); "quiet", until the coarse tube's quiet (and the settle time).
        cut: what stops the fine tube (the owner, 2026-11-26: "run the simulation without the predictor (perhaps just
            don't cut the throw)? So we basically push all potential failure mode to overcharge"): "forecast", the
            moment the forecast reaches the target (as built); "reading", only once the reading itself does, the
            forecast still driving the PID and its envelope, so the fine tube creeps at its least between; "none",
            no forecast: the PID on the reading, stopped once the reading reaches the target.
        """
        super().__init__(profile)
        self.records = records
        self.acceleration = acceleration_rps2
        self.before = before
        self.cut = cut
        self.envelope = envelope and cut != "none"
        self.seed = seed
        self.hold = hold
        self.history = []               # the last throws' own weights a turn (RAM, nothing stored)
        self.lag_flows = [flow for flow, _ in records.lag]
        self.lag_seconds = [lag for _, lag in records.lag]

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.fine_speed = 0.0           # the fine motor's speed at its ramp
        self.parcel = 0.0               # the parcel being turned out, its turns
        self.parcel_start = now_ms / 1000.0
        self.turned = []                # every parcel of the throw: (turned out at s, turns, speed)
        self.air = collections.deque()  # (due s, turns): turned out, not yet shown, at the weight a turn now
        self.unseen = 0.0
        self.last_tick = now_ms / 1000.0
        self.reading = 0.0
        self.quiet_at = None            # when the coarse tube's powder is quiet
        self.rows = []                  # (time s, reading) since the quiet
        self.k = self.carried() if self.before == "last" else None
        if self.before.startswith("given="):
            self.k = float(self.before[6:])
        self.own = None                 # the throw's own weight a turn, and the turns it is taken over
        self.held = False
        self.fine_last_forecast_error = None

    def carried(self):
        """The median of the last throws' own weights a turn: one clump's throw moves it not at all. With none yet,
        step 1's own where it is kept and seeding is on."""
        if self.history:
            return statistics.median(self.history)
        return self.records.fine_gr_turn if self.seed else None

    def holding(self, now_s: float) -> bool:
        """The fine tube held at its least, with no stop, while the coarse tube's powder may still be in the air."""
        if self.hold == "none" or self.coarse_moves:
            return False
        if self.quiet_at is None:
            return True
        until = self.quiet_at - (self.records.settle_s if self.hold == "landed" else 0.0)
        return now_s < until

    def lags(self, flow):
        return numpy.interp(flow, self.lag_flows, self.lag_seconds)

    def fit(self):
        """The throw's own weight a turn: the slope of the readings since the quiet on the fine turns shown, each
        parcel shown after the lag at its own flow, and so at the weight a turn being found: solved by repeating
        from the table's middle lag (None until KNOWN_TURNS are shown in KNOWN_ROWS readings)."""
        if len(self.rows) < KNOWN_ROWS:
            return None
        at = numpy.array([p[0] for p in self.turned])
        turns = numpy.array([p[1] for p in self.turned])
        speed = numpy.array([p[2] for p in self.turned])
        times = numpy.array([r[0] for r in self.rows])
        weights = numpy.array([r[1] for r in self.rows])
        due = at + float(numpy.median(self.lag_seconds))
        k = None
        for _ in range(FIT_ROUNDS):
            order = numpy.argsort(due)
            total = numpy.concatenate([[0.0], numpy.cumsum(turns[order])])
            shown = total[numpy.searchsorted(due[order], times, side="right")]
            x = shown - shown[0]
            if x[-1] < KNOWN_TURNS:
                return None
            k = float(numpy.polyfit(x, weights, 1)[0])
            if k <= 0.0:
                return None
            due = at + self.lags(speed * k)
        return k, float(x[-1])

    def choose(self) -> None:
        """The weight a turn the forecast takes now: the carried one where there is one (`before` "last"), else the
        throw's own once known; the air's parcels made due again at it."""
        own = self.fit() if self.rows else None
        self.own = own
        carried = self.carried() if self.before == "last" else None
        k = carried
        if carried is None and own is not None:
            k = own[0]
        if self.before.startswith("given="):
            k = float(self.before[6:])     # an oracle for the study: the rig's own figure, never on a unit
        self.k = k
        now = self.last_tick
        self.air = collections.deque()
        self.unseen = 0.0
        newest = max(self.lag_seconds) + PARCEL_S
        for turned_at, turns, speed in self.turned:
            if turned_at < now - newest:
                continue
            due = turned_at + float(self.lags(speed * (k or 0.0)))
            if due > now:
                self.air.append((due, turns))
                self.unseen += turns

    def forecast(self) -> float:
        if self.cut == "none":
            return self.reading
        return self.reading + (self.k * self.unseen if self.k is not None else 0.0)

    def reached(self) -> bool:
        """The fine tube's stop: the forecast at the target, or with `cut` "reading", the reading itself."""
        return (self.reading if self.cut == "reading" else self.forecast()) >= self.target

    def on_tick(self, now_ms: float) -> None:
        if self.done:
            return
        now = now_ms / 1000.0
        dt = now - self.last_tick
        self.last_tick = now
        change = self.acceleration * dt
        v = self.fine_speed
        v = min(self.fine_rps, v + change) if v < self.fine_rps else max(self.fine_rps, v - change)
        self.fine_speed = v
        if v > 0.0:
            self.parcel += v * dt
        if now - self.parcel_start >= PARCEL_S:
            if self.parcel > 0.0:
                speed = self.parcel / (now - self.parcel_start)
                self.turned.append((self.parcel_start, self.parcel, speed))
                self.air.append((self.parcel_start + float(self.lags(speed * (self.k or 0.0))), self.parcel))
                self.unseen += self.parcel
            self.parcel = 0.0
            self.parcel_start = now
        while self.air and self.air[0][0] <= now:
            self.unseen -= self.air.popleft()[1]
        if self.quiet_at is None and not self.coarse_moves and self.backoff_until_ms is None:
            self.quiet_at = now + max(self.lag_seconds) + self.records.settle_s
        # the stop, the moment the forecast reaches the target, below the coarse tube's threshold
        if not self.coarse_moves and self.fine_rps > 0.0 and self.reached() and not self.holding(now):
            self._stop()

    def _stop(self) -> None:
        self.fine_rps = 0.0
        self.coarse_rps = 0.0
        self.done = True

    @property
    def verdict_after_s(self) -> float:
        """Step 2 reads its verdict once the slowest lag and the settle time have passed after the stop."""
        return max(self.lag_seconds) + self.records.settle_s

    def at_rest(self, reading: float) -> None:
        """The throw's own weight a turn in hindsight, from its verdict's reading: the fine tube's powder since the
        coarse tube's quiet over its turns not yet shown then, so only the quiet's moment rests on the lag table.
        Carried to the next throws (RAM): every throw with REST_TURNS of the fine tube alone renews the carry."""
        if not self.rows:
            return
        quiet, at_quiet = self.rows[0]
        k = self.k if self.k is not None else (self.own[0] if self.own is not None else 0.0)
        turned = sum(turns for _, turns, _ in self.turned) + self.parcel
        shown = sum(turns for at, turns, speed in self.turned if at + float(self.lags(speed * k)) <= quiet)
        alone = turned - shown
        if alone >= REST_TURNS and reading > at_quiet:
            self.history = (self.history + [(reading - at_quiet) / alone])[-CARRY_THROWS:]

    def on_reading(self, now_ms: float, weight: float) -> None:
        if self.done:
            return
        profile = self.profile
        self.reading = weight
        if self.backoff_until_ms is not None and now_ms >= self.backoff_until_ms:
            self.coarse_rps = 0.0
            self.backoff_until_ms = None
        if self.quiet_at is not None and now_ms / 1000.0 >= self.quiet_at:
            self.rows.append((now_ms / 1000.0, weight))
        self.choose()
        # the coarse tube: today's
        coarse_error = self.coarse_target - weight
        if coarse_error <= 0.0 and self.coarse_moves:
            self.coarse_moves = False
            self.coarse_rps = 0.0
            self._backoff(now_ms)
        elapsed_ms = now_ms - self.last_sample_ms
        if self.coarse_moves and self.backoff_until_ms is None:
            self.coarse_integral += coarse_error
            derivative = self._derivative(coarse_error, self.coarse_last_error, elapsed_ms)
            speed = (profile.coarse_kp * coarse_error + profile.coarse_ki * self.coarse_integral
                     + profile.coarse_kd * derivative)
            self.coarse_rps = max(profile.coarse_min_rps, min(speed, profile.coarse_max_rps))
        # the fine tube: the profile's PID on the forecast's error
        error = self.target - self.forecast()
        if self.holding(now_ms / 1000.0):
            # The coarse tube's powder may still be in the air: the fine tube at its least, the PID's state kept
            self.fine_rps = self.records.fine_least_rps
            self.last_sample_ms = now_ms
            self.fine_last_forecast_error = error
            self.coarse_last_error = coarse_error
            self.first_sample = False
            return
        if not self.coarse_moves and self.reached():
            self._stop()
            return
        if not (self.envelope and self.held):
            self.fine_integral += error
        last = self.fine_last_forecast_error if self.fine_last_forecast_error is not None else error
        derivative = self._derivative(error, last, elapsed_ms)
        asked = profile.fine_kp * error + profile.fine_ki * self.fine_integral + profile.fine_kd * derivative
        least, top = self.records.fine_least_rps, profile.fine_max_rps
        if self.envelope and self.k is not None and self.k > 0.0:
            lag = float(self.lags(self.fine_speed * self.k))
            top = min(top, least + max(0.0, error) / (self.k * lag))
        self.fine_rps = max(least, min(asked, top))
        self.held = not (least < asked < top)
        self.last_sample_ms = now_ms
        self.fine_last_forecast_error = error
        self.coarse_last_error = coarse_error
        self.first_sample = False


def rig_records(rig_name: str) -> Records:
    """A rig's step 1 records: one calibration, as a unit is calibrated once and then throws."""
    return records_of(rigs.get_rig(rig_name), 0)


def run_session(job: tuple) -> list:
    """One session of throws on a rig: (rig name, controller, target, profile, records, seed) -> [(error gr, time s,
    verdict), ...]."""
    rig_name, controller, target, profile, records, seed = job
    rig = rigs.get_rig(rig_name)
    session = Session(rig, seed)
    name, _, gains = controller.partition("@")
    changes = {key: float(value) for key, value in (item.split("=") for item in gains.split(","))} if gains else {}
    if "least" in changes:
        records = dataclasses.replace(records, fine_least_rps=changes.pop("least"))
    if changes:
        profile = dataclasses.replace(profile, **{f"fine_{key}": value for key, value in changes.items()})
    if name == "today":
        made = LegacyPid(profile)
    else:
        _, before = name.split(":", 1)
        made = Step2(profile, records, rig.acceleration_rps2, before=before)
    out = []
    for _ in range(THROWS_PER_SESSION):
        result = throw_one(session, made, target, profile.fine_stop_threshold)
        out.append((result.true_error, result.elapsed_s, result.verdict))
    return out


def throw_one(session, controller, target: float, tolerance: float):
    """One throw: step 2's with its own verdict wait, its weight a turn then kept from the verdict's reading."""
    if not isinstance(controller, Step2):
        return session.throw(controller, target, tolerance)
    result = session.throw(controller, target, tolerance, verdict_after_s=controller.verdict_after_s)
    controller.at_rest(result.reading)
    return result


def summary(results: list, tolerance: float) -> str:
    errors = [e for e, _, v in results if v != "timeout"]
    times = sorted(t for _, t, v in results if v != "timeout")
    good = sum(abs(e) < tolerance for e in errors) / len(results)
    over = sum(e >= tolerance for e in errors) / len(results)
    under = sum(e <= -tolerance for e in errors) / len(results)
    return (f"sd {statistics.pstdev(errors):.3f}  mean {statistics.fmean(errors):+.3f}  good {good * 100:5.1f}%  "
            f"over {over * 100:4.1f}%  under {under * 100:4.1f}%  time {times[len(times) // 2]:5.1f} s "
            f"(95% {times[int(0.95 * (len(times) - 1))]:5.1f})  timeouts {len(results) - len(errors)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(DEFAULT_RIGS))
    parser.add_argument("--targets", nargs="+", type=float, default=[10.0])
    parser.add_argument("--throws", type=int, default=300)
    parser.add_argument("--controllers", nargs="+", default=["today", "step2:last"],
                        help="today or step2:none|last, each with the fine gains (or step 1's least) changed after @: "
                             "step2:last@kp=1.6,kd=0,least=0.05")
    parser.add_argument("--threshold", type=float, default=None, help="the coarse stop threshold (the profile's: 5)")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    profile = AR2209_PICOW_168
    if args.threshold is not None:
        profile = dataclasses.replace(profile, coarse_stop_threshold=args.threshold)
    sessions = max(1, args.throws // THROWS_PER_SESSION)
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(rig_records, list(args.rigs))))
        for rig_name in args.rigs:
            records = made[rig_name]
            print(f"{rig_name}: step 1's records: settle {records.settle_s:.2f} s, lag "
                  + ", ".join(f"{lag:.2f} s at {flow:.2f} gr/s" for flow, lag in records.lag)
                  + f"; least fine {records.fine_least_rps:.3f}, coarse {records.coarse_least_rps:.3f} rev/s")
            for target in args.targets:
                print(f"{rig_name}, {target:g} gr, the owner's Pico W AR2209 gains, coarse threshold "
                      f"{profile.coarse_stop_threshold:g} gr, judged at {profile.fine_stop_threshold:g} gr:")
                for controller in args.controllers:
                    jobs = [(rig_name, controller, target, profile, records, args.seed * 100003 + i)
                            for i in range(sessions)]
                    results = [r for batch in pool.map(run_session, jobs) for r in batch]
                    print(f"  {controller:24s} {summary(results, profile.fine_stop_threshold)}")


if __name__ == "__main__":
    main()
