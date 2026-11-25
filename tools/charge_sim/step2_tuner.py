"""Step 2's auto-tuner on the twin (ADR-009, step 2, gate 1): `new_pid = tuner(old_pid, charge_data)`, a window of
throws at a time, on the owner's objective: the time per good charge (2026-11-25).

A window's cost is its median time to the stop over its share of throws within the tolerance (the profile's fine
threshold): what a good charge costs when every bad one is thrown again. The tuner holds, in RAM, the gains it last kept
(the base) and the throws thrown on them (the latest POOL_THROWS); each window it tries the base's fine kp moved by a
factor, one way, and:
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
Z = 1.5
POOL_THROWS = 75
REST_WINDOWS = 2
STARTS = {"owner": {}, "kp/4": {"fine_kp": 0.2}, "kp x4": {"fine_kp": 3.2}}


def counted(errors: list, tolerance: float) -> float:
    """The share of throws within the tolerance, counted."""
    return sum(abs(e) < tolerance for e in errors) / len(errors)


def modelled(errors: list, tolerance: float) -> float:
    """The share within the tolerance of a normal spread with the window's median and the half-width of its middle
    68%: the same share from every throw's weight, not only from which side of the tolerance it fell."""
    ordered = sorted(errors)
    centre = statistics.median(ordered)
    width = (ordered[int(0.84 * (len(ordered) - 1))] - ordered[int(0.16 * (len(ordered) - 1))]) / 2.0
    if width <= 0.0:
        return 1.0 if abs(centre) < tolerance else 0.0
    normal = statistics.NormalDist(centre, width)
    return normal.cdf(tolerance) - normal.cdf(-tolerance)


SHARES = {"counted": counted, "modelled": modelled}


def cost(errors: list, times: list, tolerance: float, share=counted) -> tuple:
    """(median time over the share within the tolerance, that share): seconds a good charge."""
    good = share(errors, tolerance)
    time = statistics.median(times)
    return (time / good if good > 0.0 else math.inf), good


def noise(good: float, throws: int) -> float:
    """A cost's relative noise from its share of good throws."""
    return math.sqrt((1.0 - good) / (good * throws)) if good > 0.0 else 1.0


class Tuner:
    """new_pid = tuner(old_pid, charge_data), a window at a time, on the time per good charge. Holds only RAM."""

    def __init__(self, tolerance: float, z: float = Z, share: str = "counted"):
        self.tolerance = tolerance
        self.z = z
        self.share = SHARES[share]
        self.base = None
        self.pool = ([], [])        # the base's throws: final weights less the target, times
        self.direction = 1
        self.factor = FIRST_FACTOR
        self.turned = False         # the other way already tried from this base
        self.resting = 0

    def keep(self, profile, errors: list, times: list) -> None:
        self.base, self.pool, self.turned = profile, (list(errors), list(times)), False

    def __call__(self, profile, errors: list, times: list):
        """The profile's next gains from the window just thrown on `profile`, and what was done, in words."""
        if self.base is None:
            self.keep(profile, errors, times)
            return self.trial(), "first window: the base"
        if profile is self.base:
            self.pool = ((self.pool[0] + errors)[-POOL_THROWS:], (self.pool[1] + times)[-POOL_THROWS:])
            if self.resting > 0:
                self.resting -= 1
                return self.base, "resting"
            return self.trial(), "the base again"
        trial_cost, trial_good = cost(errors, times, self.tolerance, self.share)
        base_cost, base_good = cost(*self.pool, self.tolerance, self.share)
        spread = math.hypot(noise(trial_good, len(errors)), noise(base_good, len(self.pool[0])))
        if trial_cost < base_cost * (1.0 - self.z * spread):
            self.keep(profile, errors, times)
            self.factor = FIRST_FACTOR
            return self.trial(), f"kept ({trial_cost:.1f} against {base_cost:.1f} s)"
        if not self.turned:
            self.turned = True
            self.direction = -self.direction
            return self.trial(), f"put back ({trial_cost:.1f} against {base_cost:.1f} s): the other way"
        self.turned = False
        self.factor = max(LEAST_FACTOR, math.sqrt(self.factor))
        self.resting = REST_WINDOWS
        return self.base, f"put back both ways: resting, the step now x{self.factor:.2f}"

    def trial(self):
        return dataclasses.replace(self.base, fine_kp=self.base.fine_kp * self.factor ** self.direction)


def run_case(job: tuple) -> list:
    """One long session from a start's gains: [(window, kp, kd, spread, mean, time, note), ...] and the throws."""
    rig_name, start, throws, window, records, seed, tune = job      # tune: None, or (z, share)
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(AR2209_PICOW_168, **STARTS[start])
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last")
    tuner = Tuner(profile.fine_stop_threshold, *tune) if tune else None
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
