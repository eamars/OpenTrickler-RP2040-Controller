"""The legacy PID with the glide's learning, against the glide, in the simulation (ADR-009, 2026-11-22 evening; not a plan).

research/pid_learning_2026-11-22.md holds the design and the passes written before this run. Each controller thrown in
sessions of 25 (a fresh controller each, so the learning-in is counted), judged at the true weight.

    python pid_learning_study.py [--throws 300] [--targets 10,5] [--rigs pico2w_ar2209,pico2w_bm2] [--out fit/...]
"""

import argparse
from pathlib import Path

import mc
import rigs
from controllers import LOOSE_TOLERANCE_GR, VERDICT_TOLERANCE_GR
from workers import Workers

# (label, controller, tolerance)
CASES = (
    ("glide, fast (0.04)", "glide_loose", LOOSE_TOLERANCE_GR),
    ("PID as today, owner's gains", "legacy_picow_loose", LOOSE_TOLERANCE_GR),
    ("PID on reading + learnt stop", "lpid_read_picow_fast", LOOSE_TOLERANCE_GR),
    ("PID on learnt forecast", "lpid_fc_picow_fast", LOOSE_TOLERANCE_GR),
    ("PID on learnt forecast, floor 0.03", "lpid_fc_picow_floor_fast", LOOSE_TOLERANCE_GR),
    ("PID on learnt forecast, bench gains", "lpid_fc_bench_fast", LOOSE_TOLERANCE_GR),
    ("glide, accurate (0.02)", "glide", VERDICT_TOLERANCE_GR),
    ("PID on learnt forecast, 0.02", "lpid_fc_picow_acc", VERDICT_TOLERANCE_GR),
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=300)
    parser.add_argument("--targets", default="10,5")
    parser.add_argument("--rigs", default="pico2w_ar2209,pico2w_bm2")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    lines = [f"{args.throws} throws a case in sessions of {mc.THROWS_PER_SESSION}, the measured scale; shares of the "
             f"true weight at each case's own band",
             f"{'rig':15s}{'gr':>4s}  {'controller':36s}{'good':>7s}{'over':>7s}{'under':>7s}{'mean':>8s}{'sd':>7s}"
             f"{'median':>8s}{'95%':>7s}"]
    print(lines[0])
    print(lines[1])
    with Workers() as pool:
        for rig_name in args.rigs.split(","):
            rig = rigs.get_rig(rig_name)
            for target in (float(t) for t in args.targets.split(",")):
                for label, name, tolerance in CASES:
                    results = mc.run_many(rig, name, target, args.throws, args.seed, pool)
                    s = mc.summarise(results, tolerance)
                    lines.append(f"{rig_name:15s}{target:4.0f}  {label:36s}{s['good_true'] * 100:6.1f}%"
                                 f"{s['over_true'] * 100:6.1f}%{s['under_true'] * 100:6.1f}%{s['error_mean']:+8.3f}"
                                 f"{s['error_sd']:7.3f}{s['time_median']:7.1f}s{s['time_p95']:6.1f}s")
                    print(lines[-1], flush=True)
    if args.out:
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
