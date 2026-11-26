"""Step 2's auto-tuner on the twin (ADR-009, step 2, gate 1): `new_pid = tuner(old_pid, charge_data)`, a window of
throws at a time, on the owner's objective: the time per good charge (2026-11-25).

A window's cost is its median time to the stop over its share of throws within the tolerance (the profile's fine
threshold): what a good charge costs when every bad one is thrown again. The tuner holds, in RAM, the gains it last kept
(the base) and the throws thrown on them (the latest POOL_THROWS); each window it tries the base's fine kp moved by a
factor, one way (the first trial the way the first window misses: over on average, slower; else quicker; 2026-11-26),
and:
- **keeps it** when its cost is lower than the base's by more than Z times the two windows' noise (each a share's:
  sqrt((1 - good) / (good x throws))), and tries the same way again;
- **puts the base back** otherwise, and tries the other way at once; when both ways are put back, the factor's
  logarithm is halved (down to LEAST_FACTOR) and the tuner rests REST_WINDOWS windows on the base, pooling them.
kd stays as the user has it: on the forecast the D term only scales kp (progress, 2026-11-25, 18:51). Everything is a
ratio (kp by factors, costs against each other), so it is the same in grams and grains.

Each case is a long session (the tubes keep their state, the RAM its carried weight a turn), from a start's gains.

    venv\\Scripts\\python tools\\charge_sim\\step2_tuner.py [--rigs ...] [--throws 400] [--window 25]
"""

import argparse
import dataclasses
import math
import statistics

import rigs
import step2
from legacy_pid import AR2209_PICOW_168, LegacyPid
from throw import Session
from workers import Workers

WINDOW = 25
FIRST_FACTOR = 1.5
LEAST_FACTOR = 1.1
Z = 0.0                     # the rule gate 1 passed with (19:52): a lower cost is enough; z > 0 asks for a margin of its noise
POOL_THROWS = 75
REST_WINDOWS = 2
STARTS = {"owner": {}, "kp/4": {"fine_kp": 0.2}, "kp x4": {"fine_kp": 3.2}}


def counted(errors: list, tolerance: float) -> float:
    """The share of throws within the tolerance, counted."""
    return sum(abs(e) < tolerance for e in errors) / len(errors)


def erf_approx(x: float) -> float:
    """erf by Abramowitz and Stegun 7.1.26 (an error under 1.5e-7), as charge_tuner.c computes it: the firmware cannot
    take libm's erf on the Pico W (2026-11-26), and the two must decide alike."""
    sign = -1.0 if x < 0.0 else 1.0
    x = abs(x)
    t = 1.0 / (1.0 + 0.3275911 * x)
    poly = t * (0.254829592 + t * (-0.284496736 + t * (1.421413741 + t * (-1.453152027 + t * 1.061405429))))
    return sign * (1.0 - poly * math.exp(-x * x))


def normal_cdf(x: float, mu: float, sigma: float) -> float:
    return 0.5 * (1.0 + erf_approx((x - mu) / (sigma * math.sqrt(2.0))))


def modelled(errors: list, tolerance: float) -> float:
    """The share within the tolerance of a normal spread with the window's median and the half-width of its middle
    68%: the same share from every throw's weight, not only from which side of the tolerance it fell."""
    ordered = sorted(errors)
    centre = statistics.median(ordered)
    width = (ordered[int(0.84 * (len(ordered) - 1))] - ordered[int(0.16 * (len(ordered) - 1))]) / 2.0
    if width <= 0.0:
        return 1.0 if abs(centre) < tolerance else 0.0
    return normal_cdf(tolerance, centre, width) - normal_cdf(-tolerance, centre, width)


SHARES = {"counted": counted, "modelled": modelled}


def cost(errors: list, times: list, tolerance: float, share=counted) -> tuple:
    """(median time over the share within the tolerance, that share): seconds a good charge."""
    good = share(errors, tolerance)
    time = statistics.median(times)
    return (time / good if good > 0.0 else math.inf), good


def noise(good: float, throws: int) -> float:
    """A cost's relative noise from its share of good throws."""
    return math.sqrt((1.0 - good) / (good * throws)) if good > 0.0 else 1.0


AXES = {"kp": ("gain",), "kp+kd": ("gain", "braking")}
# The gains put right on the first window (2026-11-26: the owner, "If a scale reports gram, while the default parameter
# is grain, then I need a reliable way to tune down to support gram, with the auto tuner"). The steady gain kp / braking
# times step 1's weight a turn and its middle lag is a number with no unit: the bench's stored gains 1.3, the owner's
# 0.5; grains' gains on a scale in grams 0.08, in grains a gram's 20. Out of GAIN_RANGE the first trial brings it to
# GAIN_AIM; kept only if cheaper, as any trial. The integral term is the user's, never the tuner's
GAIN_RANGE = (0.2, 5.0)
GAIN_AIM = 1.0
# The coarse kp put right too (2026-11-26: the owner, "use and tune PID ... tune under the existing framework"): its
# gain kp x step 1's coarse weight a turn x the middle lag, a number with no unit, at most COARSE_GAIN_MOST. Above it the
# coarse tube's powder still in the air at its stop takes the throw past the target, and more so the larger the charge
# (step2_coarse_tuner_study.py: a coarse kp of 0.2, +14.5 at 30 gr; at the bound, sound to 30 gr). Only brought down,
# never up: tuned up, the coarse kp gained no time from the shipped gains
COARSE_GAIN_MOST = 0.4


def braking(kd: float, k: float) -> float:
    """1 + kd x k / 1000: how much the D term divides the fine tube's speed while it pours steadily on the forecast
    (the forecast rises by k a turn; the derivative is per millisecond, the original's). Dimensionless: the same in
    grams and grains. 1 is no D term; the stored kd of 10 to 15 is 1.01."""
    return 1.0 + kd * k / 1000.0


class Tuner:
    """new_pid = tuner(old_pid, charge_data), a window at a time, on the time per good charge. Holds only RAM.

    axes "kp+kd" (the owner, 2026-11-26 03:01: "consider Kd as well"): a compass search on two axes, the steady gain
    kp / braking (moved by kp, kd held) and the braking (moved by kd, kp with it so the steady gain holds). The D term
    acts by itself only on what the forecast does not hold (the coarse tube's powder landing): at an equal steady gain
    more braking is a slower, narrower throw (step2_kd_map.py). A trial put back is tried the other way, then on the
    other axis; with all four put back the step shrinks and the kept gains rest."""

    def __init__(self, tolerance: float, z: float = Z, share: str = "counted", blind: str = "stay", first: str = "miss",
                 axes: str = "kp"):
        """
        blind: what a trial is judged by when neither it nor the kept gains had a good throw (both cost infinity):
            "stay", never cheaper (as gate 1 built it); "error", the lower median miss is the cheaper.
        first: the first trial's way: "miss", the way the first window misses (over on average, a slower kp; the
            product, 2026-11-26); "up", always quicker (as gate 1 built it).
        axes: "kp", the fine kp alone (gate 1's); "kp+kd", the steady gain and the braking.
        """
        self.tolerance = tolerance
        self.blind = blind
        self.first = first
        self.axes = AXES[axes]
        self.z = z
        self.share = SHARES[share]
        self.base = None
        self.pool = ([], [])        # the base's throws: final weights less the target, times
        self.axis = 0
        self.direction = 1
        self.factor = FIRST_FACTOR
        self.tried = 0              # ways put back since the base was kept or the step changed
        self.resting = 0
        self.k = None               # the weight a turn the braking is reckoned with (the carried one)
        self.lag = None             # step 1's middle lag, s
        self.coarse_k = None        # step 1's coarse weight a turn, for the coarse kp put right
        self.putting_right = False  # the window being thrown is the gains put right (into range)

    @property
    def turned(self) -> bool:
        return self.tried % 2 == 1

    def keep(self, profile, errors: list, times: list) -> None:
        self.base, self.pool, self.tried = profile, (list(errors), list(times)), 0

    def missing_way(self, errors: list) -> int:
        """The way the current axis is tried first: the gain down and the braking up after throws over on average."""
        over = statistics.fmean(errors) > 0.0
        if self.axes[self.axis] == "gain":
            return -1 if over and self.first == "miss" else 1
        return 1 if over else -1

    def put_right(self):
        """The base's gains put right, or None when they need nothing: the steady gain into range by the weight a
        turn and the lag (GAIN_RANGE), and the coarse kp down to COARSE_GAIN_MOST by step 1's coarse weight a turn
        and the lag. The integral term is never touched (the owner, 2026-11-26: "The tuner should never
        intentionally touch Ki"): an integral the user set is met by kp and kd."""
        base = self.base
        kp = base.fine_kp
        if self.k and self.lag:
            hold = braking(base.fine_kd, self.k)
            gain = kp / hold * self.k * self.lag
            if not GAIN_RANGE[0] <= gain <= GAIN_RANGE[1]:
                kp = GAIN_AIM * hold / (self.k * self.lag)
        coarse_kp = base.coarse_kp
        if self.coarse_k and self.lag and coarse_kp * self.coarse_k * self.lag > COARSE_GAIN_MOST:
            coarse_kp = COARSE_GAIN_MOST / (self.coarse_k * self.lag)
        if kp == base.fine_kp and coarse_kp == base.coarse_kp:
            return None
        return dataclasses.replace(base, fine_kp=kp, coarse_kp=coarse_kp)

    def __call__(self, profile, errors: list, times: list, k: float = None, lag: float = None,
                 coarse_k: float = None):
        """The profile's next gains from the window just thrown on `profile`, and what was done, in words. k: the
        weight a turn the braking is reckoned with, needed with the axes "kp+kd"; lag: step 1's middle lag, for the
        gains put right; coarse_k: step 1's coarse weight a turn, for the coarse kp put right."""
        if k is not None and k > 0.0:
            self.k = k
        if lag is not None and lag > 0.0:
            self.lag = lag
        if coarse_k is not None and coarse_k > 0.0:
            self.coarse_k = coarse_k
        if self.base is None:
            self.keep(profile, errors, times)
            self.direction = self.missing_way(errors)
            right = self.put_right()
            if right is not None:
                self.putting_right = True
                return right, "first window: the base; the gains put right"
            return self.next_trial("first window: the base")
        if profile is self.base:
            self.pool = ((self.pool[0] + errors)[-POOL_THROWS:], (self.pool[1] + times)[-POOL_THROWS:])
            if self.resting > 0:
                self.resting -= 1
                return self.base, "resting"
            return self.next_trial("the base again")
        trial_cost, trial_good = cost(errors, times, self.tolerance, self.share)
        base_cost, base_good = cost(*self.pool, self.tolerance, self.share)
        spread = math.hypot(noise(trial_good, len(errors)), noise(base_good, len(self.pool[0])))
        if self.putting_right:
            self.putting_right = False
            if trial_cost < base_cost * (1.0 - self.z * spread):
                self.keep(profile, errors, times)
                self.factor = FIRST_FACTOR
                self.direction = self.missing_way(errors)   # the way the gains put right miss by, not the first's
                return self.next_trial(f"kept ({trial_cost:.1f} against {base_cost:.1f} s): the gains put right")
            return self.next_trial(f"put back ({trial_cost:.1f} against {base_cost:.1f} s): the gains as they were")
        if math.isinf(trial_cost) and math.isinf(base_cost) and self.blind == "error":
            trial_miss = statistics.median(abs(e) for e in errors)
            base_miss = statistics.median(abs(e) for e in self.pool[0])
            if trial_miss < base_miss:
                self.keep(profile, errors, times)
                self.factor = FIRST_FACTOR
                return self.next_trial(f"kept, none good (a median miss {trial_miss:.3f} against {base_miss:.3f})")
        elif trial_cost < base_cost * (1.0 - self.z * spread):
            self.keep(profile, errors, times)
            self.factor = FIRST_FACTOR
            return self.next_trial(f"kept ({trial_cost:.1f} against {base_cost:.1f} s)")
        said = f"put back ({trial_cost:.1f} against {base_cost:.1f} s)"
        return self.put_back(said)

    def next_trial(self, said: str):
        """A trial from the kept gains, or, when it would not move them, put back unthrown (as charge_tuner.c)."""
        trial = self.trial()
        if (trial.fine_kp, trial.fine_kd) != (self.base.fine_kp, self.base.fine_kd):
            return trial, said
        return self.put_back(f"{said}; no move")

    def put_back(self, said: str):
        """The trial no cheaper: the other way, then the next axis; all put back, the step shrinks and the base
        rests. A trial that would not move the gains (the braking at 1 asked lower) counts as put back unthrown."""
        while True:
            self.tried += 1
            if self.tried % 2 == 1:
                self.direction = -self.direction
                said_now = f"{said}: the other way"
            elif self.tried < 2 * len(self.axes):
                self.axis = (self.axis + 1) % len(self.axes)
                self.direction = self.missing_way(self.pool[0])
                said_now = f"{said}: the {self.axes[self.axis]}"
            else:
                self.tried = 0
                self.factor = max(LEAST_FACTOR, math.sqrt(self.factor))
                self.resting = REST_WINDOWS
                if len(self.axes) > 1:
                    self.axis = 0
                    self.direction = self.missing_way(self.pool[0])
                ways = "both ways" if len(self.axes) == 1 else "every way"
                return self.base, f"{said}: put back {ways}, resting, the step now x{self.factor:.2f}"
            trial = self.trial()
            if (trial.fine_kp, trial.fine_kd) != (self.base.fine_kp, self.base.fine_kd):
                return trial, said_now

    def trial(self):
        step = self.factor ** self.direction
        if self.axes[self.axis] == "gain":
            return dataclasses.replace(self.base, fine_kp=self.base.fine_kp * step)
        if not self.k:
            return self.base
        before = braking(self.base.fine_kd, self.k)
        after = max(1.0, before * step)
        if abs(after - before) < 0.01:
            return self.base
        return dataclasses.replace(self.base, fine_kp=self.base.fine_kp * after / before,
                                   fine_kd=(after - 1.0) * 1000.0 / self.k)


def run_case(job: tuple) -> list:
    """One long session from a start's gains: [(window, kp, kd, spread, mean, time, note), ...] and the throws."""
    rig_name, start, throws, window, records, seed, tune = job      # tune: None, or (z, share)
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(AR2209_PICOW_168, **STARTS[start])
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last")
    tuner = Tuner(profile.fine_stop_threshold, *tune, first="up") if tune else None    # gate 1's, as it ran
    windows, all_throws = [], []
    errors, times = [], []
    for n in range(throws):
        controller.profile = profile
        result = step2.throw_one(session, controller, 10.0, profile.fine_stop_threshold)
        errors.append(result.true_error)
        times.append(result.elapsed_s)
        all_throws.append((result.true_error, result.elapsed_s))
        if len(errors) == window:
            thrown_on = profile
            note = ""
            if tune:
                profile, note = tuner(profile, errors, times)
            windows.append((len(windows), thrown_on.fine_kp, cost(errors, times, profile.fine_stop_threshold)[0],
                            statistics.pstdev(errors), statistics.fmean(errors), statistics.median(times), note))
            errors, times = [], []
    return windows, all_throws


def today_case(job: tuple) -> list:
    rig_name, throws, seed = job
    rig = rigs.get_rig(rig_name)
    session = Session(rig, seed)
    controller = LegacyPid(AR2209_PICOW_168)
    return [(r.true_error, r.elapsed_s) for r in
            (session.throw(controller, 10.0, AR2209_PICOW_168.fine_stop_threshold) for _ in range(throws))]


def tail_summary(throws: list) -> str:
    errors = [e for e, _ in throws]
    times = sorted(t for _, t in throws)
    good = sum(abs(e) < AR2209_PICOW_168.fine_stop_threshold for e in errors) / len(errors)
    median = times[len(times) // 2]
    per_good = median / good if good > 0 else float("inf")
    return (f"{per_good:5.1f} s a good charge  (sd {statistics.pstdev(errors):.3f}  mean {statistics.fmean(errors):+.3f}"
            f"  good {good * 100:4.1f}%  time {median:4.1f} s)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(step2.DEFAULT_RIGS))
    parser.add_argument("--starts", nargs="+", default=list(STARTS))
    parser.add_argument("--throws", type=int, default=400)
    parser.add_argument("--window", type=int, default=WINDOW)
    parser.add_argument("--seeds", type=int, default=2)
    parser.add_argument("--windows", action="store_true", help="print each window")
    parser.add_argument("--tuners", nargs="+", default=[f"{Z}:counted"], help="the tuners tried, each z:share")
    parser.add_argument("--no-fixed", action="store_true", help="leave out the untuned cases")
    args = parser.parse_args()
    tail = args.throws // 4
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        for rig_name in args.rigs:
            today = [t for batch in pool.map(today_case, [(rig_name, args.throws, 7000 + s) for s in range(args.seeds)])
                     for t in batch[-tail:]]
            print(f"{rig_name}: the last {tail} throws of {args.throws}, {args.seeds} sessions each")
            print(f"  {'today, the owner gains':30s} {tail_summary(today)}")
            tuners = [None] if not args.no_fixed else []
            tuners += [(float(z), share) for z, share in (item.split(":") for item in args.tuners)]
            jobs = [(rig_name, start, args.throws, args.window, made[rig_name], 7000 + s, tune)
                    for start in args.starts for tune in tuners for s in range(args.seeds)]
            results = pool.map(run_case, jobs)
            for i in range(0, len(jobs), args.seeds):
                _, start, _, _, _, _, tune = jobs[i]
                label = f"step 2 from {start}, " + (f"tuned z {tune[0]:g} {tune[1]}" if tune else "fixed")
                last = [t for windows, throws in results[i:i + args.seeds] for t in throws[-tail:]]
                ends = ", ".join(f"{w[-1][1]:.2f}" for w, _ in results[i:i + args.seeds])
                print(f"  {label:38s} {tail_summary(last)}   (kp at the end: {ends})")
                if args.windows and tune:
                    for w in results[i][0]:
                        print(f"      window {w[0]:2d} kp {w[1]:.3f}  {w[2]:6.1f} s a good charge  sd {w[3]:.3f} "
                              f"mean {w[4]:+.3f} time {w[5]:4.1f} s  {w[6]}")


if __name__ == "__main__":
    main()
