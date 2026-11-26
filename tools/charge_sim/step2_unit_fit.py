"""Does the twin throw as the bench's unit threw? (ADR-009, 2026-11-26, the bench run on 2.0.3-252.)

The unit (the Pico 2 W FA0C, the A&D FX-300i, AR2209) threw 59 real throws of 10 gr on step 2 as it ships, the tuner on,
from its stored gains: 25 on fine kp 2, 25 on kp 1.33 (the tuner's trial, kept), 9 on kp 0.89 (its next trial).
Here each rig throws the same: the unit's own step 1 records and profile, the same gains in the same order, judged as
the unit judges, by the reading at its verdict. Nothing is tuned: the gains are the ones the unit's tuner gave, to see
whether the twin reproduces what they did.

    venv\\Scripts\\python tools\\charge_sim\\step2_unit_fit.py [--rigs ...] [--seeds 8]
"""

import argparse
import dataclasses
import statistics

import rigs
import step2
from legacy_pid import Profile
from throw import Session
from workers import Workers

# The unit's settings at the run (logs/adr009/bench_1126n/before_flash_252): AR2209's profile and the scale's records
UNIT_PROFILE = Profile(coarse_kp=0.02, coarse_ki=0.0, coarse_kd=0.3, coarse_min_rps=0.16, coarse_max_rps=5.0,
                       fine_kp=2.0, fine_ki=0.0, fine_kd=10.0, fine_min_rps=0.02, fine_max_rps=5.0)
UNIT_RECORDS = step2.Records(settle_s=0.550, lag=((0.205, 0.9713), (0.4, 0.7840), (0.8333, 0.6213), (2.76, 0.5128),
                                                  (10.26, 0.5371)),
                             fine_least_rps=0.02, coarse_least_rps=0.16, fine_gr_turn=0.8333, coarse_gr_turn=10.84)
# The gains the unit's tuner threw, in order (the windows' kp; kd 10 throughout)
SCHEDULE = ((2.0, 25), (1.3333, 25), (0.8889, 9))
# What the unit threw, by window (step2_windows.py on the run's logs): good, over, under, mean, sd, median time
UNIT = ((0.36, 0.64, 0.00, +0.088, 0.120, 3.72), (0.44, 0.48, 0.08, +0.037, 0.050, 5.03),
        (0.67, 0.33, 0.00, +0.011, 0.023, 6.51))
TARGET = 10.0
TOLERANCE = 0.03


def session(job: tuple) -> list:
    """One session of the schedule on a rig: each throw's (window, reading's error, time)."""
    rig_name, seed = job
    rig = rigs.get_rig(rig_name)
    profile = UNIT_PROFILE
    controller = step2.Step2(profile, UNIT_RECORDS, rig.acceleration_rps2, before="last", seed=True, hold="none",
                             cut="threshold")
    run = Session(rig, seed)
    out = []
    for window, (kp, throws) in enumerate(SCHEDULE):
        controller.profile = dataclasses.replace(profile, fine_kp=kp)
        for _ in range(throws):
            result = step2.throw_one(run, controller, TARGET, TOLERANCE)
            out.append((window, result.error, result.elapsed_s))
    return out


def line(errors: list, times: list) -> tuple:
    n = len(errors)
    good = sum(abs(e) < TOLERANCE - 1e-9 for e in errors) / n
    over = sum(e >= TOLERANCE - 1e-9 for e in errors) / n
    return (good, over, 1.0 - good - over, statistics.fmean(errors), statistics.pstdev(errors),
            statistics.median(times))


def show(name: str, values: tuple) -> str:
    good, over, under, mean, sd, time = values
    return (f"{name:10s} good {good:4.0%} over {over:4.0%} under {under:4.0%} mean {mean:+.3f} sd {sd:.3f} "
            f"time {time:4.1f} s")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(step2.DEFAULT_RIGS))
    parser.add_argument("--seeds", type=int, default=8)
    args = parser.parse_args()
    jobs = [(rig, seed) for rig in args.rigs for seed in range(args.seeds)]
    results = Workers().map(session, jobs)
    for rig in args.rigs:
        throws = [t for (name, _), out in zip(jobs, results) if name == rig for t in out]
        print(f"{rig}, {args.seeds} sessions of the unit's schedule:")
        for window, (kp, _) in enumerate(SCHEDULE):
            errors = [e for w, e, _ in throws if w == window]
            times = [t for w, _, t in throws if w == window]
            print(f"  window {window + 1}, kp {kp:.2f}")
            print("    " + show("unit", UNIT[window]))
            print("    " + show("twin", line(errors, times)))


if __name__ == "__main__":
    main()
