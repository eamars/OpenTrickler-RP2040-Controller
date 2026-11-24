"""The rehearsal's form R and its count (ADR-009, gate 3; progress.md, 2026-11-24, 02:44): figures for the owner.

At the shipped defaults (coarse 0.1 to 5, fine 0.08 to 5 rev/s; tolerance 0.03) on every stand-in scale, the controller
being the PID law on the AR2209 profile's gains: today's form (10 rehearsals of 1 gr by the glide's law), R with 40 and
R with 10. For each: the rehearsals' grains and minutes, where the stop is placed (the throws' median first error less
the aim), and 25 throws at 10 gr: good and over, all of them and the first 5. Decides nothing: the count is the owner's.

    python r_study.py [--seeds 2] [--throws 25]
"""

import argparse
import statistics
import sys

import f4_cases
import staged
from throw import Session
from workers import Workers

SHIPPED = staged.Limits(0.1, 5.0, 0.08, 5.0)
TOLERANCE_GR = 0.03
TARGET_GR = 10.0
FORMS = (("today", "gram", 10), ("R 40", "throw", 40), ("R 10", "throw", 10))


def job(args: tuple) -> dict:
    name, seed, label, form, count, throws = args
    rig = f4_cases.stand_in(name)
    digit = rig.scale.digit_gr
    band = TOLERANCE_GR - digit / 2.0
    measured = staged.measure_on(staged.TwinBench(rig, seed), digit, rig.scale.report_s, SHIPPED)
    cal = staged.for_finish(measured, "glide", band, SHIPPED)
    law = staged.profile_pid(SHIPPED)
    seconds0, poured0 = cal.seconds, cal.poured_gr
    staged.rehearse(cal, Session(rig, seed + 1), rig.acceleration_rps2, form=form, count=count, law=law)
    controller = staged.Staged(cal, rig.acceleration_rps2)
    controller.pid = law
    session = Session(rig, seed + 2)
    errors, firsts, times = [], [], []
    for _ in range(throws):
        r = session.throw(controller, TARGET_GR, TOLERANCE_GR)
        errors.append(r.true_error)
        times.append(r.elapsed_s)
        if controller.log:
            firsts.append(controller.log[-1][0])
    return {"name": name, "label": label, "grains": cal.poured_gr - poured0, "minutes": (cal.seconds - seconds0) / 60.0,
            "placed": (statistics.median(firsts) - cal.aim_gr) / digit if firsts else float("nan"),
            "good": sum(abs(e) < TOLERANCE_GR for e in errors), "over": sum(e >= TOLERANCE_GR for e in errors),
            "good5": sum(abs(e) < TOLERANCE_GR for e in errors[:5]), "over5": sum(e >= TOLERANCE_GR for e in errors[:5]),
            "time": statistics.median(times), "throws": throws}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--seeds", type=int, default=2)
    parser.add_argument("--throws", type=int, default=25)
    args = parser.parse_args()
    jobs = [(name, seed, label, form, count, args.throws) for name in f4_cases.names() for seed in range(args.seeds)
            for label, form, count in FORMS]
    with Workers() as pool:
        results = pool.map(job, jobs)
    print(f"{'scale':10s} {'form':6s} {'gr':>6s} {'min':>5s} {'placed':>7s}  good/over of {args.throws * args.seeds}  "
          f"first 5s: good/over of {5 * args.seeds}  median s")
    totals = {}
    for name in f4_cases.names():
        for label, _, _ in FORMS:
            rows = [r for r in results if r["name"] == name and r["label"] == label]
            line = {k: sum(r[k] for r in rows) for k in ("good", "over", "good5", "over5")}
            grains = statistics.fmean(r["grains"] for r in rows)
            minutes = statistics.fmean(r["minutes"] for r in rows)
            placed = statistics.fmean(r["placed"] for r in rows)
            time = statistics.median(r["time"] for r in rows)
            print(f"{name:10s} {label:6s} {grains:6.1f} {minutes:5.1f} {placed:+6.1f}d  {line['good']:3d}/{line['over']:<3d}"
                  f"                {line['good5']:2d}/{line['over5']:<2d}                 {time:5.1f}", flush=True)
            total = totals.setdefault(label, {"good": 0, "over": 0, "good5": 0, "over5": 0, "grains": [], "minutes": []})
            for k in ("good", "over", "good5", "over5"):
                total[k] += line[k]
            total["grains"].append(grains)
            total["minutes"].append(minutes)
    n = len(f4_cases.names()) * args.seeds
    for label, total in totals.items():
        print(f"all {label:6s}: rehearsals {statistics.fmean(total['grains']):.1f} gr and "
              f"{statistics.fmean(total['minutes']):.1f} min a calibration; good {total['good']} over {total['over']} of "
              f"{n * args.throws}; first 5s good {total['good5']} over {total['over5']} of {n * 5}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
