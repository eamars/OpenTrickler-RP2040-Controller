"""The one real figure the plant can be set against: PR #113's fall after the stop against the fine Kp.

Its author measured on a unit with an A&D and Vihtavuori N135 (`powder_analysis_config.h` on the PR's branch):
fine Kp 6.0 gave +0.156 gr after the stop, Kp 3.0 gave 0 to +0.028 gr. This runs today's loop at both gains on
plants whose fine tube flows more and more, and prints the mean error, to see which plant agrees.

Usage: python pr113_check.py [--throws 200]
"""

import argparse
import dataclasses
import statistics

from controllers import CONTROLLERS
from rigs import make_rig
from throw import Session


def mean_error(rig, controller_name: str, throws: int) -> tuple:
    """Mean and largest true error of a controller on a rig, and its median time."""
    controller, tolerance = CONTROLLERS[controller_name](rig)
    results = []
    for index in range(max(1, throws // 25)):
        session = Session(rig, 7000 + index)
        results += [session.throw(controller, 42.5, tolerance) for _ in range(25)]
    errors = [r.true_error for r in results]
    over = sum(r.verdict == "over" for r in results) / len(results)
    return statistics.fmean(errors), max(errors), over, statistics.median(r.elapsed_s for r in results)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=200)
    args = parser.parse_args()
    base = make_rig()
    for flow_x, inlet_x in ((1.0 / 3.0, 1.0 / 3.0), (1.0, 1.0), (2.0, 1.0), (3.3, 1.7)):
        fine = dataclasses.replace(base.fine, out_per_rev=base.fine.out_per_rev * flow_x,
                                   max_inflow_gr_s=base.fine.max_inflow_gr_s * inlet_x)
        rig = dataclasses.replace(base, fine=fine)
        line = f"fine flow x{flow_x:g}, inlet x{inlet_x:g}:"
        for name in ("legacy", "legacy_kp3", "legacy_kp6", "flow_dose"):
            mean, worst, over, time_s = mean_error(rig, name, args.throws)
            line += f"  {name} {mean:+.3f} (max {worst:+.2f}, over {over * 100:.0f}%, {time_s:.0f} s)"
        print(line)


if __name__ == "__main__":
    main()
