"""Controllers on rigs, each judged at the owner's two tolerances: how many throws are good, and how long one takes.

A throw is good within 0.02 gr of its target, and within 0.04 gr for a load that need not be that exact (the
owner, 2026-11-21). On a scale with a digit of 0.02 gr those are errors under 0.03 and under 0.05 gr. Every figure
is of the true weight in the cup, after the powder has landed.

Usage:
    python bands.py [--rigs pico2w_bm2,pico2w_ar2209] [--controllers legacy,legacy_slow_finish,flow_dose]
                    [--target 20] [--throws 500] [--seed 1] [--out fit/bands.txt]
"""

import argparse
from pathlib import Path

import mc
import rigs
from controllers import CONTROLLERS
from workers import Workers

BANDS_GR = (0.03, 0.05)     # an error under these is within 0.02 and within 0.04 gr on a 0.02 gr digit


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", default="pico2w_bm2,pico2w_ar2209")
    parser.add_argument("--controllers", default="legacy,legacy_kp08,legacy_slow_finish,flow_dose")
    parser.add_argument("--target", type=float, default=20.0)
    parser.add_argument("--throws", type=int, default=500)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    lines = [f"{args.throws} throws each at {args.target:g} gr, seed {args.seed}; shares of all throws, of the true weight",
             f"{'rig':16s}{'controller':22s}{'good 0.02':>10s}{'over':>7s}{'under':>7s}{'good 0.04':>11s}{'over':>7s}"
             f"{'under':>7s}{'error gr':>10s}{'spread':>8s}{'time s':>8s}{'95%':>7s}{'timeout':>9s}"]
    with Workers() as pool:
        for rig_name in args.rigs.split(","):
            rig = rigs.get_rig(rig_name)
            for controller in args.controllers.split(","):
                if controller not in CONTROLLERS:
                    raise SystemExit(f"no controller named {controller}: {sorted(CONTROLLERS)}")
                results = mc.run_many(rig, controller, args.target, args.throws, args.seed, pool)
                tight, loose = (mc.summarise(results, band) for band in BANDS_GR)
                lines.append(f"{rig_name:16s}{controller:22s}{tight['good_true'] * 100:9.1f}%{tight['over_true'] * 100:6.1f}%"
                             f"{tight['under_true'] * 100:6.1f}%{loose['good_true'] * 100:10.1f}%"
                             f"{loose['over_true'] * 100:6.1f}%{loose['under_true'] * 100:6.1f}%"
                             f"{tight['error_mean']:+10.3f}{tight['error_sd']:8.3f}{tight['time_median']:8.1f}"
                             f"{tight['time_p95']:7.1f}{tight['timeout'] * 100:8.1f}%")
                print(lines[-1], flush=True)
    if args.out:
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
