"""ADR-009 step 2: the fine tube's kp and kd mapped on the twin, the bench's other gains, the seed on, the tuner off.

The owner (2026-11-26, 03:01): "Make sure you are not just tuning the Kp, consider Kd as well." On the forecast, while
the fine tube pours steadily, its D term only scales the gain: the forecast rises by the weight a turn k times the
speed, so the speed asked is kp x error / (1 + kd x k). Where powder the forecast does not hold lands (the coarse
tube's still in the air at its stop, the reading's steps), the D term acts by itself: it brakes the fine tube. This
maps each cell's good charge's cost, spread and time, and its steady gain kp / (1 + kd x k), so a column of equal
gains shows whether kd matters beyond it.

    venv\\Scripts\\python tools\\charge_sim\\step2_kd_map.py [--rigs ...] [--throws 60] [--sessions 2]
"""

import argparse
import dataclasses
import math
import statistics

import step2
from step2_variants import BENCH
from throw import Session
from workers import Workers
import rigs

KPS = (0.5, 1.0, 2.0, 4.0, 8.0)
KDS = (0.0, 2.5, 5.0, 10.0, 20.0, 40.0)
TARGET = 10.0
RIGS = ("pico2w_ar2209_f2", "pico2w_ar2209_f2+fine_by_speed", "pico2w_ar2209_night", "pico2w_bm2")


def cell(job: tuple) -> tuple:
    rig_name, kp, kd, throws, seed, records = job
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(BENCH, fine_kp=kp, fine_kd=kd)
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none")
    out = []
    for _ in range(throws):
        result = step2.throw_one(session, controller, TARGET, profile.fine_stop_threshold)
        out.append((result.true_error, result.elapsed_s))
    return (rig_name, kp, kd), out


def judged(throws: list, tolerance: float) -> dict:
    errors = [e for e, _ in throws]
    good = sum(abs(e) < tolerance for e in errors) / len(errors)
    time = statistics.median(t for _, t in throws)
    return {"cost": time / good if good else math.inf, "sd": statistics.pstdev(errors),
            "mean": statistics.fmean(errors), "time": time, "good": good}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(RIGS))
    parser.add_argument("--throws", type=int, default=60)
    parser.add_argument("--sessions", type=int, default=2)
    parser.add_argument("--kps", nargs="+", type=float, default=list(KPS))
    parser.add_argument("--kds", nargs="+", type=float, default=list(KDS),
                        help="the D term is per millisecond (the original's): kd 10 is a hundredth of kp's effect")
    args = parser.parse_args()
    tolerance = BENCH.fine_stop_threshold
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        jobs = [(rig, kp, kd, args.throws, 9500 + s, made[rig]) for rig in args.rigs for kp in args.kps for kd in args.kds
                for s in range(args.sessions)]
        results = pool.map(cell, jobs)
    grouped = {}
    for key, throws in results:
        grouped.setdefault(key, []).extend(throws)
    for rig in args.rigs:
        k = made[rig].fine_gr_turn
        print(f"{rig} (seed {k:.3f}), {args.sessions} x {args.throws} throws a cell: a good charge's cost in s "
              f"[sd, median time], the steady gain kp/(1+kd k/1000) after the |")
        print("  kp \\ kd " + "".join(f"{kd:>24g}" for kd in args.kds))
        for kp in args.kps:
            row = ""
            for kd in args.kds:
                j = judged(grouped[(rig, kp, kd)], tolerance)
                cost = "  inf" if math.isinf(j["cost"]) else f"{j['cost']:5.1f}"
                row += f"  {cost} [{j['sd']:.3f} {j['time']:4.1f}] |{kp / (1 + kd * k / 1000.0):5.2f}"
            print(f"  {kp:6g}  {row}")


if __name__ == "__main__":
    main()
