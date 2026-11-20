"""A controller across scales: its overs, unders and time against a scale's delay, reading period and hold.

The rig's own scale is replaced by one with each combination of:
- delay: seconds before a change on the pan shows at all;
- period: seconds between two readings;
- hold: seconds the scale holds back a change of a digit or two before showing it (0: it shows it at once).
Everything else is the rig's. A scale of another make is a point on this map once its three figures are measured;
the map does not claim what any make's figures are.

Usage:
    python scale_map.py [--rig bench_pico2w] [--controller legacy_slow_finish] [--throws 300] [--target 20]
                        [--gain-rule]
"""

import argparse
import dataclasses
import os
from concurrent.futures import ProcessPoolExecutor

import mc
import rigs
from controllers import CONTROLLERS
from legacy_pid import PICO2W_SLOW_FINISH
from plant import table_value

DELAYS_S = (0.2, 0.45, 0.8, 1.2)
PERIODS_S = (0.05, 0.1, 0.25, 0.5)
GAIN_SHARE = 0.37           # a proportional loop round a delay does not overshoot under 1/e
REST_OF_LAG_S = 0.15        # the fall, the scale's smoothing and the link, beside its delay and period


def by_rule(rig):
    """Today's loop with its fine gain set from the rig's own figures: 0.37 / (the scale's lag x grains a turn)."""
    lag = rig.scale.dead_s + rig.scale.report_s + REST_OF_LAG_S
    gain = GAIN_SHARE / (lag * table_value(rig.fine.gr_per_turn, 1.0))
    return dataclasses.replace(PICO2W_SLOW_FINISH, fine_kp=round(gain, 2))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="bench_pico2w")
    parser.add_argument("--controller", default="legacy_slow_finish", choices=sorted(CONTROLLERS))
    parser.add_argument("--throws", type=int, default=300)
    parser.add_argument("--target", type=float, default=20.0)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--gain-rule", action="store_true",
                        help="today's loop with its fine gain set for each scale by the rule, in place of --controller")
    args = parser.parse_args()
    rig = rigs.get_rig(args.rig)
    holds = sorted({0.0, rig.scale.hold_s})
    cells = [(hold, delay, period) for hold in holds for delay in DELAYS_S for period in PERIODS_S]
    jobs, spans = [], []
    for hold, delay, period in cells:
        changed = dataclasses.replace(rig, scale=dataclasses.replace(rig.scale, hold_s=hold, dead_s=delay,
                                                                     report_s=period))
        controller = by_rule(changed) if args.gain_rule else args.controller
        made = mc.session_jobs(changed, controller, args.target, args.throws, args.seed)
        spans.append((len(jobs), len(jobs) + len(made)))
        jobs.extend(made)
    with ProcessPoolExecutor(max_workers=os.cpu_count()) as pool:
        sessions = list(pool.map(mc.run_session, jobs))
    sums = {cell: mc.summarise([result for session in sessions[first:last] for result in session])
            for cell, (first, last) in zip(cells, spans)}

    what = "today's loop, fine gain by the rule" if args.gain_rule else args.controller
    print(f"rig {args.rig}, {what}, {args.throws} throws a cell at {args.target:g} gr")
    print(f"the rig's own scale: delay {rig.scale.dead_s:g} s, a reading every {rig.scale.report_s:g} s, "
          f"hold {rig.scale.hold_s:g} s")
    print("each cell: over % / under % / median seconds")
    for hold in holds:
        print(f"\nhold {hold:g} s" + "".join(f"{f'every {period:g} s':>22s}" for period in PERIODS_S))
        for delay in DELAYS_S:
            row = "".join(f"{s['over_true'] * 100:9.0f} /{s['under_true'] * 100:3.0f} /{s['time_median']:5.1f}"
                          for s in (sums[(hold, delay, period)] for period in PERIODS_S))
            print(f"delay {delay:4.2f} s{row}")


if __name__ == "__main__":
    main()
