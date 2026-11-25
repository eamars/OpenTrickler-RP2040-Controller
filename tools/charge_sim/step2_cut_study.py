"""ADR-009 step 2: what stops the fine tube, on the twin (the owner, 2026-11-26).

The owner: "Can you also run the simulation without the predictor (perhaps just don't cut the throw)? So we basically
push all potential failure mode to overcharge? And will it help improving the grouping?"

Three stops (step2.Step2's `cut`), each from fixed gains (no tuner), the seed on:
- forecast: the moment the forecast reaches the target (as built);
- reading: the forecast drives the approach (the PID and its envelope) but the throw is never cut early: the fine tube
  creeps at its least until the reading itself reaches the target;
- none: no forecast at all, the PID on the reading, stopped once the reading reaches the target;
- reading+aim: never cut early, the reading's stop short of the target by the median of the last throws' landing past
  their aim (step2.Step2's `aim`; the owner, 2026-11-26: "Yes try it, but don't stress yourself too hard");
- reading+whole: the same aim-off on the fine finish's whole target, the forecast's PID too.
`--tuner` runs the tuner as it ships (kp and kd, the share modelled) and judges the last throws of each session.

Each judged by its grouping (the final weight's SD, and its 5% to 95% range), its mean, its worst over, its shares
good, over and under at the user's tolerance, its median and 95% time, and a good charge's cost (counted, modelled).

    venv\\Scripts\\python tools\\charge_sim\\step2_cut_study.py [--rigs ...] [--throws 150] [--sessions 4]
"""

import argparse
import math
import statistics

import rigs
import step2
import step2_tuner
from legacy_pid import AR2209_PICOW_168
from step2_variants import BENCH
from throw import Session
from workers import Workers

STOPS = {"forecast": ("forecast", "none"), "reading": ("reading", "none"), "reading+aim": ("reading", "median"),
         "reading+whole": ("reading", "whole"), "none": ("none", "none")}
STARTS = {"bench": BENCH, "owner": AR2209_PICOW_168}
RIGS = ("pico2w_ar2209_f2", "pico2w_ar2209_f2+fine_by_speed", "pico2w_ar2209_night", "pico2w_bm2")
TARGET = 10.0


def case(job: tuple) -> tuple:
    rig_name, start, stop, throws, seed, records, tuned, kept = job
    cut, aim = STOPS[stop]
    rig = rigs.get_rig(rig_name)
    profile = STARTS[start]
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none", cut=cut,
                             aim=aim)
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0, axes="kp+kd", share="modelled") if tuned else None
    out, errors, times = [], [], []
    for _ in range(throws):
        controller.profile = profile
        result = step2.throw_one(session, controller, TARGET, profile.fine_stop_threshold)
        out.append((result.true_error, result.elapsed_s, result.verdict))
        errors.append(result.true_error)
        times.append(result.elapsed_s)
        if tuner is not None and len(errors) == step2_tuner.WINDOW:
            profile, _ = tuner(profile, errors, times, k=controller.carried())
            errors, times = [], []
    return (rig_name, start, stop), out[-kept:]


def summary(throws: list, tolerance: float) -> str:
    errors = sorted(e for e, _, v in throws if v != "timeout")
    times = sorted(t for _, t, v in throws if v != "timeout")
    n = len(errors)
    good = sum(abs(e) < tolerance for e in errors) / n
    over = sum(e >= tolerance for e in errors) / n
    under = sum(e <= -tolerance for e in errors) / n
    counted = step2_tuner.cost(errors, times, tolerance, step2_tuner.counted)[0]
    modelled = step2_tuner.cost(errors, times, tolerance, step2_tuner.modelled)[0]
    low, high = errors[int(0.05 * (n - 1))], errors[int(0.95 * (n - 1))]

    def seconds(value: float) -> str:
        return "  inf" if math.isinf(value) else f"{value:5.1f}"

    return (f"sd {statistics.pstdev(errors):.3f}  5-95% {low:+.2f}..{high:+.2f}  mean {statistics.fmean(errors):+.3f}"
            f"  worst {errors[-1]:+.2f}  good {good:4.0%} over {over:4.0%} under {under:4.0%}  time "
            f"{times[n // 2]:4.1f} s (95% {times[int(0.95 * (n - 1))]:4.1f})  a good charge {seconds(counted)} s "
            f"(modelled {seconds(modelled)})  timeouts {len(throws) - n}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(RIGS))
    parser.add_argument("--throws", type=int, default=150)
    parser.add_argument("--sessions", type=int, default=4)
    parser.add_argument("--stops", nargs="+", default=list(STOPS))
    parser.add_argument("--tuner", action="store_true", help="the tuner as it ships; the last --kept throws judged")
    parser.add_argument("--kept", type=int, default=None, help="the throws judged, the last of each session")
    args = parser.parse_args()
    kept = args.kept or args.throws
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        jobs = [(rig, start, stop, args.throws, 9600 + s, made[rig], args.tuner, kept) for rig in args.rigs
                for start in STARTS for stop in args.stops for s in range(args.sessions)]
        results = pool.map(case, jobs)
    grouped = {}
    for key, throws in results:
        grouped.setdefault(key, []).extend(throws)
    for rig in args.rigs:
        print(f"{rig}, {args.sessions} sessions of {args.throws} at 10 gr:")
        for start in STARTS:
            tolerance = STARTS[start].fine_stop_threshold
            print(f"  from the {start}'s gains:")
            for stop in args.stops:
                print(f"    {stop:11s} {summary(grouped[(rig, start, stop)], tolerance)}")


if __name__ == "__main__":
    main()
