"""Step 2's auto-tuner on the twin (ADR-009, step 2, gate 1): `new_pid = tuner(old_pid, charge_data)`, a window of
throws at a time, by the owner's ranking: the final weight's spread first, then the time.

The tuner holds, in RAM, the gains it last kept (the base) and that window's spread and median time; each window it
tries one move on the fine tube's gains (kp up, kp down, kd up, kd down, each by a factor), throws a window on it, and:
- **keeps it** when its spread (the half-width of its middle 68%, which one clump does not move) is narrower than the
  base's beyond the window's noise (by MARGIN), or within that noise and its median time shorter by TIME_SHARE (the
  base's spread is then the lower of the two, so a run of "as narrow, quicker" cannot walk the spread up); every window
  thrown on the base is pooled into its figures;
- **puts the base back** otherwise, and tries the next move.
A kept move is tried again; when every move in turn is put back, the tuner rests for REST_WINDOWS windows on the base.
Everything is a ratio (gains by factors, spreads and times against each other), so it is the same in grams and grains.

Each case is a long session (the tubes keep their state, the RAM its carried weight a turn), from a start's gains.

    venv\\Scripts\\python tools\\charge_sim\\step2_tuner.py [--rigs ...] [--throws 400] [--window 20]
"""

import argparse
import dataclasses
import statistics

import rigs
import step2
from legacy_pid import AR2209_PICOW_168, LegacyPid
from throw import Session
from workers import Workers

FACTOR = {"kp": 1.25, "kd": 2.0}
MARGIN = 0.25               # a window's spread differs from the base's only past this share (its noise at 20 throws)
POOL_THROWS = 60
TIME_SHARE = 0.05
REST_WINDOWS = 3
MOVES = (("kp", 1), ("kp", -1), ("kd", 1), ("kd", -1))
STARTS = {"owner": {}, "kp/4": {"fine_kp": 0.2}, "kp x4": {"fine_kp": 3.2}, "kd 0": {"fine_kd": 0.0}}


def band(errors: list) -> float:
    """Half the width that holds the middle 68% of the final weights: a window's spread that one clump does not move."""
    ordered = sorted(errors)
    return (ordered[int(0.84 * (len(ordered) - 1))] - ordered[int(0.16 * (len(ordered) - 1))]) / 2.0


class Tuner:
    """new_pid = tuner(old_pid, charge_data), a window at a time. Holds only RAM: the base's gains, and the throws
    thrown on them since they were kept (every window on them pooled, the latest POOL_THROWS)."""

    def __init__(self):
        self.base = None            # the profile kept
        self.pool = ([], [])        # its throws: final weights less the target, times
        self.reference = None       # its spread to beat
        self.move = 0
        self.rejected = 0
        self.resting = 0

    def keep(self, profile, errors: list, times: list, reference: float) -> None:
        self.base, self.pool, self.reference = profile, (list(errors), list(times)), reference

    def __call__(self, profile, errors: list, times: list):
        """The profile's next gains from the window just thrown on `profile`, and what was done, in words."""
        spread, time = band(errors), statistics.median(times)
        if self.base is None:
            self.keep(profile, errors, times, spread)
            return self.trial(), "first window: the base"
        if profile is self.base:
            self.pool = ((self.pool[0] + errors)[-POOL_THROWS:], (self.pool[1] + times)[-POOL_THROWS:])
            self.reference = band(self.pool[0])
            if self.resting > 0:
                self.resting -= 1
                return self.base, "resting"
            return self.trial(), "the base again"
        base_time = statistics.median(self.pool[1])
        narrower = spread < self.reference * (1.0 - MARGIN)
        as_narrow_quicker = spread <= self.reference * (1.0 + MARGIN) and time < base_time * (1.0 - TIME_SHARE)
        if narrower or as_narrow_quicker:
            self.keep(profile, errors, times, spread if narrower else min(spread, self.reference))
            self.rejected = 0
            return self.trial(), "kept: " + ("narrower" if narrower else "as narrow, quicker")
        self.rejected += 1
        self.move = (self.move + 1) % len(MOVES)
        if self.rejected >= len(MOVES):
            self.rejected = 0
            self.resting = REST_WINDOWS
            return self.base, "every move put back: resting on the base"
        return self.base, "put back"

    def trial(self):
        base = self.base
        gain, direction = MOVES[self.move]
        name = f"fine_{gain}"
        value = getattr(base, name)
        if gain == "kd" and value == 0.0:
            value = 1.0 if direction > 0 else 0.0
        else:
            value = value * FACTOR[gain] ** direction
        return dataclasses.replace(base, **{name: value})


def run_case(job: tuple) -> list:
    """One long session from a start's gains: [(window, kp, kd, spread, mean, time, note), ...] and the throws."""
    rig_name, start, throws, window, records, seed, tune = job
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(AR2209_PICOW_168, **STARTS[start])
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last")
    tuner = Tuner()
    windows, all_throws = [], []
    errors, times = [], []
    for n in range(throws):
        controller.profile = profile
        result = session.throw(controller, 10.0, profile.fine_stop_threshold)
        errors.append(result.true_error)
        times.append(result.elapsed_s)
        all_throws.append((result.true_error, result.elapsed_s))
        if len(errors) == window:
            thrown_on = profile
            note = ""
            if tune:
                profile, note = tuner(profile, errors, times)
            windows.append((len(windows), thrown_on.fine_kp, thrown_on.fine_kd, statistics.pstdev(errors),
                            statistics.fmean(errors), statistics.median(times), note))
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
    return (f"sd {statistics.pstdev(errors):.3f}  mean {statistics.fmean(errors):+.3f}  good {good * 100:4.1f}%  "
            f"time {times[len(times) // 2]:4.1f} s")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(step2.DEFAULT_RIGS))
    parser.add_argument("--starts", nargs="+", default=list(STARTS))
    parser.add_argument("--throws", type=int, default=400)
    parser.add_argument("--window", type=int, default=20)
    parser.add_argument("--seeds", type=int, default=2)
    parser.add_argument("--windows", action="store_true", help="print each window")
    args = parser.parse_args()
    tail = args.throws // 4
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        for rig_name in args.rigs:
            today = [t for batch in pool.map(today_case, [(rig_name, args.throws, 7000 + s) for s in range(args.seeds)])
                     for t in batch[-tail:]]
            print(f"{rig_name}: the last {tail} throws of {args.throws}, {args.seeds} sessions each")
            print(f"  {'today, the owner gains':30s} {tail_summary(today)}")
            jobs = [(rig_name, start, args.throws, args.window, made[rig_name], 7000 + s, tune)
                    for start in args.starts for tune in (False, True) for s in range(args.seeds)]
            results = pool.map(run_case, jobs)
            for i in range(0, len(jobs), args.seeds):
                _, start, _, _, _, _, tune = jobs[i]
                label = f"step 2 from {start}, " + ("tuned" if tune else "fixed")
                last = [t for windows, throws in results[i:i + args.seeds] for t in throws[-tail:]]
                ends = ", ".join(f"kp {w[-1][1]:.2f} kd {w[-1][2]:.1f}" for w, _ in results[i:i + args.seeds])
                print(f"  {label:30s} {tail_summary(last)}   (its last window's gains: {ends})")
                if args.windows and tune:
                    for w in results[i][0]:
                        print(f"      window {w[0]:2d} kp {w[1]:.3f} kd {w[2]:6.1f}  sd {w[3]:.3f} mean {w[4]:+.3f} "
                              f"time {w[5]:4.1f} s  {w[6]}")


if __name__ == "__main__":
    main()
