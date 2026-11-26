"""Step 2 whole, the tuner on, its candidates against gate 1's build (ADR-009, 2026-11-26, the owner asleep).

Each case is a session of throws from a start's gains on a rig, step 1's records made on that rig as calibration.py
makes them, the tuner on (the README's rule, z 0). Judged on the first throws (getting started) and the last (where
it settles): a good charge's cost (the median time over the share within the tolerance), the share good, over and
under, the final weight's mean and sd, the median time.

Variants (step2.Step2's and step2_tuner.Tuner's switches):
- built: gate 1's, as gate 2 judged it;
- seed: with nothing carried, step 1's own weight a turn;
- seed+landed, seed+quiet: and the fine tube held at its least until the coarse tube's powder has landed (or is quiet);
- seed+blind: and the tuner judging two windows with no good throw by their median miss;
- seed+landed+blind;
- product: seed, and the tuner's first trial the way the first window misses (as it ships, 2026-11-26). The others
  try up first, as the study ran.

`--by-window` prints the product from the bench's gains window by window, over more sessions: what the
bench's first windows should show (gate 4's prediction).

    venv\\Scripts\\python tools\\charge_sim\\step2_variants.py [--throws 300] [--seeds 2]
    venv\\Scripts\\python tools\\charge_sim\\step2_variants.py --by-window [--throws 150] [--seeds 8]
"""

import argparse
import dataclasses
import math
import statistics
from pathlib import Path

import rigs
import step2
import step2_tuner
from legacy_pid import AR2209_PICOW_168, Profile
from throw import Session
from workers import Workers

# The bench's Pico 2 W's stored AR2209 profile (read over its API, 2026-11-26): gate 4's baseline, the owner's
BENCH = Profile(coarse_kp=0.02, coarse_ki=0.0, coarse_kd=0.3, coarse_min_rps=0.16, coarse_max_rps=5.0,
                fine_kp=2.0, fine_ki=0.0, fine_kd=10.0, fine_min_rps=0.0336, fine_max_rps=5.0)
STARTS = {"bench": BENCH, "owner": AR2209_PICOW_168}
VARIANTS = {"built": (False, "none", "stay", "up"), "seed": (True, "none", "stay", "up"),
            "seed+landed": (True, "landed", "stay", "up"), "seed+quiet": (True, "quiet", "stay", "up"),
            "seed+blind": (True, "none", "error", "up"), "seed+landed+blind": (True, "landed", "error", "up"),
            "product": (True, "none", "stay", "miss"),
            "product+kd": (True, "none", "stay", "miss", "kp+kd"),
            # the owner's objective estimated more steadily (2026-11-26, 03:2x): the share good from the window's
            # spread (step2_tuner.modelled), or a keep only when cheaper by its noise (z 1); and no tuner at all
            "modelled": (True, "none", "stay", "miss", "kp", "modelled", 0.0),
            "modelled+kd": (True, "none", "stay", "miss", "kp+kd", "modelled", 0.0),
            "z1": (True, "none", "stay", "miss", "kp", "counted", 1.0),
            "z1+kd": (True, "none", "stay", "miss", "kp+kd", "counted", 1.0),
            "fixed": (True, "none", "stay", "miss", None),
            # the throw never cut on the forecast, stopped by the reading (the owner, 2026-11-26: "Yes, build it"):
            # the tuner as it ships with it (at main's rule, within the fine threshold; "@target", at the target as
            # first built), and no tuner
            # the objective the tolerance first, then quicker (the owner, 2026-11-26: the product); "@cost", the
            # time per good charge it replaced
            "shipped": (True, "none", "stay", "miss", "kp+kd", "modelled", 0.0, "threshold", "tolerance"),
            "shipped@cost": (True, "none", "stay", "miss", "kp+kd", "modelled", 0.0, "threshold", "cost"),
            "shipped@target": (True, "none", "stay", "miss", "kp+kd", "modelled", 0.0, "reading"),
            "fixed+reading": (True, "none", "stay", "miss", None, "counted", 0.0, "reading")}
TARGET = 10.0
FIRST = 50
LAST = 100


def case(job: tuple) -> tuple:
    rig_name, start, variant, throws, seed, records = job
    seeded, hold, blind, first, *more = VARIANTS[variant]
    axes, share, z, cut, objective = (more + ["kp", "counted", 0.0, "forecast", "cost"][len(more):])[:5]
    rig = rigs.get_rig(rig_name)
    profile = STARTS[start]
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=seeded, hold=hold, cut=cut)
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=z, share=share, blind=blind, first=first,
                              axes=axes, objective=objective) if axes else None
    out, errors, times = [], [], []
    for _ in range(throws):
        controller.profile = profile
        result = step2.throw_one(session, controller, TARGET, profile.fine_stop_threshold)
        out.append((result.true_error, result.elapsed_s, profile.fine_kp))
        errors.append(result.true_error)
        times.append(result.elapsed_s)
        if len(errors) == step2_tuner.WINDOW and tuner is not None:
            profile, _ = tuner(profile, errors, times, k=controller.carried(),
                               lag=statistics.median(lag for _, lag in records.lag))
            errors, times = [], []
    return job[:3], out, (profile.fine_kp, profile.fine_kd)


def judged(throws: list, tolerance: float) -> dict:
    errors = [t[0] for t in throws]
    times = sorted(t[1] for t in throws)
    good = sum(abs(e) < tolerance for e in errors) / len(errors)
    median = times[len(times) // 2]
    modelled = step2_tuner.modelled(errors, tolerance)
    return {"cost": median / good if good else math.inf, "good": good,
            "modelled": median / modelled if modelled else math.inf,
            "over": sum(e >= tolerance for e in errors) / len(errors),
            "under": sum(e <= -tolerance for e in errors) / len(errors),
            "mean": statistics.fmean(errors), "sd": statistics.pstdev(errors), "time": median}


def line(j: dict) -> str:
    cost = "  inf" if math.isinf(j["cost"]) else f"{j['cost']:5.1f}"
    modelled = "  inf" if math.isinf(j["modelled"]) else f"{j['modelled']:5.1f}"
    return (f"{cost} s (modelled {modelled})  good {j['good'] * 100:3.0f}%  over {j['over'] * 100:3.0f}%  "
            f"under {j['under'] * 100:3.0f}%  mean {j['mean']:+.3f}  sd {j['sd']:.3f}  time {j['time']:4.1f} s")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=300)
    parser.add_argument("--seeds", type=int, default=2)
    parser.add_argument("--rigs", nargs="+", default=list(step2.DEFAULT_RIGS))
    parser.add_argument("--variants", nargs="+", default=list(VARIANTS))
    parser.add_argument("--by-window", action="store_true", help="the product from the bench's gains, by window")
    args = parser.parse_args()
    tolerance = BENCH.fine_stop_threshold
    if args.by_window:
        by_window(args, tolerance)
        return
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        jobs = [(rig, start, variant, args.throws, 9100 + s, made[rig]) for rig in args.rigs for start in STARTS
                for variant in args.variants for s in range(args.seeds)]
        results = pool.map(case, jobs)
    grouped = {}
    for (rig, start, variant), throws, kp in results:
        grouped.setdefault((rig, start, variant), []).append((throws, kp))
    for rig in args.rigs:
        records = made[rig]
        print(f"{rig}: step 1's fine weight a turn {records.fine_gr_turn}, least fine {records.fine_least_rps:.3f}")
        for start in STARTS:
            print(f"  from {start}'s gains, {TARGET:g} gr, {args.seeds} sessions of {args.throws}:")
            for variant in args.variants:
                runs = grouped[(rig, start, variant)]
                first = judged([t for throws, _ in runs for t in throws[:FIRST]], tolerance)
                last = judged([t for throws, _ in runs for t in throws[-LAST:]], tolerance)
                kps = ", ".join(f"{kp:.2f}/{kd:.0f}" for _, (kp, kd) in runs)
                print(f"    {variant:18s} first {FIRST}: {line(first)}")
                print(f"    {'':18s} last {LAST}:  {line(last)}   (kp at the end {kps})")


def by_window(args, tolerance: float) -> None:
    window = step2_tuner.WINDOW
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        jobs = [(rig, "bench", "product", args.throws, 9100 + s, made[rig]) for rig in args.rigs
                for s in range(args.seeds)]
        results = pool.map(case, jobs)
    for rig in args.rigs:
        runs = [throws for (r, _, _), throws, _ in results if r == rig]
        print(f"{rig}: the product from the bench's gains, {TARGET:g} gr, {len(runs)} sessions, by window of {window}")
        for w in range(args.throws // window):
            part = [t for throws in runs for t in throws[w * window:(w + 1) * window]]
            kps = sorted({round(t[2], 2) for t in part})
            print(f"  window {w + 1}: {line(judged(part, tolerance))}  kp {', '.join(f'{k:g}' for k in kps)}")


if __name__ == "__main__":
    main()
