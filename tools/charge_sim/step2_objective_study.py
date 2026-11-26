"""ADR-009 step 2: what the tuner is judged by, on the twins nearest the bench's unit (2026-11-26, after its 49 throws).

The owner: "You can do all sort of attempt in simulation ... prioritize on the simulation based on data already."
The bench's unit is nearest the twin's f2 rig (step2_bench_ab.py); f2 with the fine tube pouring by speed and the
night rig bracket it. Two questions, from fixed gains (the seed on, no tuner):

- **The landscape:** each cell of kp and kd thrown long, judged by the owner's objective (a good charge's time: the
  median time over the share within the tolerance), its share counted and its share modelled (a normal spread with
  the median and the middle 68%'s half-width), with its SD, mean and time: where each optimum lies, and how flat.
- **The judge's noise:** at the bench's gains and their neighbours, how often a window of 25 throws against a pool of
  75 on the kept gains picks the truly cheaper (by the long run's cost): the tuner's decision, made by chance or not.

    venv\\Scripts\\python tools\\charge_sim\\step2_objective_study.py [--rigs ...] [--throws 50] [--sessions 4]
"""

import argparse
import dataclasses
import math
import random
import statistics

import rigs
import step2
import step2_tuner
from step2_variants import BENCH
from throw import Session
from workers import Workers

KPS = (0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0)
KDS = (10.0, 300.0, 1000.0, 3000.0)
RIGS = ("pico2w_ar2209_f2", "pico2w_ar2209_f2+fine_by_speed", "pico2w_ar2209_night")
TARGET = 10.0
# The tuner's moves from the bench's gains (kp 2, kd 10): each against the bench's gains, as one window against a pool
NEIGHBOURS = ((1.3333, 10.0), (3.0, 10.0), (2.0, 300.0), (2.0, 1000.0))
TRIALS = 2000


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


def costs(throws: list, tolerance: float) -> dict:
    errors = [e for e, _ in throws]
    times = [t for _, t in throws]
    out = {"sd": statistics.pstdev(errors), "mean": statistics.fmean(errors), "time": statistics.median(times)}
    for name, share in step2_tuner.SHARES.items():
        out[name] = step2_tuner.cost(errors, times, tolerance, share)[0]
    return out


def decisions(base: list, other: list, tolerance: float, share: str, rng: random.Random) -> float:
    """How often a window of the other gains' throws against a pool of the base's picks the truly cheaper one."""
    truth = costs(other, tolerance)[share] < costs(base, tolerance)[share]
    right = 0
    for _ in range(TRIALS):
        window = rng.sample(other, step2_tuner.WINDOW)
        pool = rng.sample(base, step2_tuner.POOL_THROWS)
        trial_cost = step2_tuner.cost(*zip(*window), tolerance, step2_tuner.SHARES[share])[0]
        pool_cost = step2_tuner.cost(*zip(*pool), tolerance, step2_tuner.SHARES[share])[0]
        right += (trial_cost < pool_cost) == truth
    return right / TRIALS


def fmt(value: float) -> str:
    return "  inf" if math.isinf(value) else f"{value:5.1f}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(RIGS))
    parser.add_argument("--throws", type=int, default=50)
    parser.add_argument("--sessions", type=int, default=4)
    parser.add_argument("--long", type=int, default=600, help="throws at the bench's gains and each neighbour")
    args = parser.parse_args()
    tolerance = BENCH.fine_stop_threshold
    cells = [(kp, kd) for kp in KPS for kd in KDS]
    long_cells = [(2.0, 10.0)] + list(NEIGHBOURS)
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        jobs = [(rig, kp, kd, args.throws, 9700 + s, made[rig]) for rig in args.rigs for kp, kd in cells
                for s in range(args.sessions)]
        jobs += [(rig, kp, kd, args.long // 4, 9800 + s, made[rig]) for rig in args.rigs for kp, kd in long_cells
                 for s in range(4)]
        results = pool.map(cell, jobs)
    grid, long_runs = {}, {}
    for job, (key, throws) in zip(jobs, results):
        target = long_runs if job[3] == args.long // 4 and job[4] >= 9800 else grid
        target.setdefault(key, []).extend(throws)
    rng = random.Random(1)
    for rig in args.rigs:
        print(f"{rig}: {args.sessions} x {args.throws} throws a cell. Each: a good charge counted, modelled (s); "
              f"SD; median time")
        print("  kp \\ kd " + "".join(f"{kd:>28g}" for kd in KDS))
        judged = {}
        for kp in KPS:
            row = ""
            for kd in KDS:
                j = judged[(kp, kd)] = costs(grid[(rig, kp, kd)], tolerance)
                row += f"   {fmt(j['counted'])} {fmt(j['modelled'])}  {j['sd']:.3f} {j['time']:4.1f}"
            print(f"  {kp:6g}  {row}")
        for share in step2_tuner.SHARES:
            best = min(judged, key=lambda c: judged[c][share])
            near = sorted(c for c in judged if judged[c][share] <= 1.15 * judged[best][share])
            print(f"  {share}: least at kp {best[0]:g}, kd {best[1]:g} ({judged[best][share]:.1f} s, SD "
                  f"{judged[best]['sd']:.3f}, {judged[best]['time']:.1f} s a throw); within 15% of it: {len(near)} "
                  f"of {len(judged)} cells")
        base = long_runs[(rig, 2.0, 10.0)]
        base_costs = costs(base, tolerance)
        print(f"  the judge, from the bench's gains (kp 2, kd 10; {len(base)} throws: counted {base_costs['counted']:.1f}"
              f" s, modelled {base_costs['modelled']:.1f} s):")
        for kp, kd in NEIGHBOURS:
            other = long_runs[(rig, kp, kd)]
            other_costs = costs(other, tolerance)
            print(f"    to kp {kp:g}, kd {kd:g} (counted {other_costs['counted']:.1f} s, modelled "
                  f"{other_costs['modelled']:.1f} s): a window picks the truly cheaper "
                  + ", ".join(f"{share} {decisions(base, other, tolerance, share, rng):.0%}"
                              for share in step2_tuner.SHARES))


if __name__ == "__main__":
    main()
