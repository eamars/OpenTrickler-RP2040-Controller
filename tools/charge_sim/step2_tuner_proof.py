"""The tuner on its objective, the tolerance first, then quicker (the owner, 2026-11-26), proven on the twin.

From the bench's stored gains (fine kp 2, kd 10), step 2 as it ships throws at 10 gr with the tuner on and nothing set
by hand; each window's pass rate (the share within the tolerance, by the reading at the verdict, as the unit judges),
its median time and the gains it was thrown on. The time per good charge it replaced runs beside it. Then, on the gains
each session ended with, 10 throws at 10 gr and 10 at 42.5 gr (the tuner still on; ten throws close no window): each
throw's time and verdict, as the owner asked to see them.

The twin's rigs bracket the bench's unit (step2_unit_fit.py): f2 throws slower and more accurately than the unit, night
quicker and less accurately; a tuner that climbs on both is expected to climb on the unit.

    venv\\Scripts\\python tools\\charge_sim\\step2_tuner_proof.py [--rigs ...] [--throws 600] [--seeds 4]
"""

import argparse
import statistics

import rigs
import step2
import step2_tuner
from legacy_pid import AR2209_PICOW_168
from step2_variants import BENCH
from throw import Session
from workers import Workers

TOLERANCE = 0.03
TARGET = 10.0
SHOWN = ((10.0, 10), (42.5, 10))
STARTS = {"bench": BENCH, "owner": AR2209_PICOW_168}


def run(job: tuple) -> dict:
    rig_name, seed, objective, throws, records, start = job
    rig = rigs.get_rig(rig_name)
    profile = STARTS[start]
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none",
                             cut="threshold")
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0, axes="kp+kd", share="counted",
                              objective=objective)
    lag = statistics.median(lag for _, lag in records.lag)
    session = Session(rig, seed)
    windows, errors, times, whole = [], [], [], []
    for _ in range(throws):
        controller.profile = profile
        result = step2.throw_one(session, controller, TARGET, TOLERANCE)
        errors.append(result.error)
        # The tuner times the fine tube's part of the throw (the PR's review, T1); the window's time shown is the whole
        times.append(controller.fine_part_s(result.elapsed_s))
        whole.append(result.elapsed_s)
        if len(errors) == step2_tuner.WINDOW:
            good = sum(abs(e) < TOLERANCE - 1e-9 for e in errors) / len(errors)
            held = tuner.holding
            windows.append((good, statistics.median(whole), profile.fine_kp, profile.fine_kd, held))
            profile, _ = tuner(profile, errors, times, k=controller.carried(), lag=lag,
                               coarse_k=records.coarse_gr_turn)
            errors, times, whole = [], [], []
    shown = []
    for target, count in SHOWN:
        for _ in range(count):
            controller.profile = profile
            result = step2.throw_one(session, controller, target, TOLERANCE)
            shown.append((target, result.elapsed_s, result.error, result.verdict))
    return {"windows": windows, "shown": shown, "kp": profile.fine_kp, "kd": profile.fine_kd}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=["pico2w_ar2209_f2", "pico2w_ar2209_night"])
    parser.add_argument("--throws", type=int, default=600)
    parser.add_argument("--seeds", type=int, default=4)
    parser.add_argument("--objectives", nargs="+", default=["tolerance"])
    parser.add_argument("--starts", nargs="+", default=list(STARTS))
    args = parser.parse_args()
    records = {rig: step2.rig_records(rig) for rig in args.rigs}
    jobs = [(rig, seed, objective, args.throws, records[rig], start) for rig in args.rigs for start in args.starts
            for objective in args.objectives for seed in range(args.seeds)]
    results = Workers().map(run, jobs)
    for rig, start, objective in [(r, s, o) for r in args.rigs for s in args.starts for o in args.objectives]:
        runs = [r for (name, _, obj, _, _, st), r in zip(jobs, results)
                if name == rig and obj == objective and st == start]
        print(f"{rig}, objective {objective}, {len(runs)} sessions from the {start}'s gains "
              f"(kp {STARTS[start].fine_kp:g}, kd {STARTS[start].fine_kd:g}); H: held:")
        count = min(len(r["windows"]) for r in runs)
        for w in range(count):
            cells = [r["windows"][w] for r in runs]
            print(f"  window {w + 1:2d}: good " + " ".join(f"{c[0]:4.0%}{'H' if c[4] else ' '}" for c in cells)
                  + "   time " + " ".join(f"{c[1]:4.1f}" for c in cells)
                  + "   kp/kd " + " ".join(f"{c[2]:.2f}/{c[3]:.0f}" for c in cells))
        print("  the gains reached: " + ", ".join(f"kp {r['kp']:.2f} kd {r['kd']:.0f}" for r in runs))
        for target, _ in SHOWN:
            every = [s for r in runs for s in r["shown"] if s[0] == target]
            good = sum(s[3] == "good" for s in every) / len(every)
            print(f"  {target:g} gr, {len(every)} throws: time median {statistics.median(s[1] for s in every):.1f} s, "
                  f"most {max(s[1] for s in every):.1f} s; good {good:.0%}, over "
                  f"{sum(s[3] == 'over' for s in every) / len(every):.0%}, under "
                  f"{sum(s[3] == 'under' for s in every) / len(every):.0%}")
            first = [s for s in runs[0]["shown"] if s[0] == target]
            print("    session 1: " + "  ".join(f"{s[1]:.1f} s {s[2]:+.2f}" for s in first))


if __name__ == "__main__":
    main()
