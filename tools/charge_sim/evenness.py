"""How evenly must the fine tube pour at low speed for a finish by one stop to hold the tolerance?

The rig's fine tube is made kinder in steps, each a thing an oscillation or another release aid would have to
achieve, and today's loop with the recorded slow-finish settings is thrown on each:
- bursts: the share kept of what its releases carry beyond one kernel (1: as measured; 0: a kernel at a time);
- regularity: how even the gaps between releases are (as measured; 8; 30, close to a metronome);
- lateness: the share kept of how much later than the scale's own delay its powder shows.
For each, the weight short of the target at which the fine tube stops is swept and the best one taken, so a
variant is judged at its own best stop. A second table does the same with the scale showing every change at once.

Usage:
    python evenness.py [--rig pico2w_ar2209] [--throws 400] [--target 20] [--least-rps 0.03]
"""

import argparse
import dataclasses

import mc
import rigs
from legacy_pid import PICO2W_SLOW_FINISH
from workers import Workers

BURST_SHARES = (1.0, 0.5, 0.25, 0.0)
LATE_SHARES = (1.0, 0.0)
REGULARITIES = (None, 8.0, 30.0)        # None: as measured
STOPS_GR = (0.03, 0.05, 0.07, 0.09, 0.11, 0.13, 0.15)


def kinder(rig, burst_share: float, regularity, late_share: float, hold_s):
    """The rig with its fine tube's releases, their evenness and its powder's lateness changed, and the scale's hold."""
    fine = rig.fine
    fine = dataclasses.replace(
        fine, burst_kernels=tuple((speed, 1.0 + burst_share * (kernels - 1.0)) for speed, kernels in fine.burst_kernels),
        late_s=tuple((speed, late_share * seconds) for speed, seconds in fine.late_s),
        regularity=fine.regularity if regularity is None else regularity)
    scale = rig.scale if hold_s is None else dataclasses.replace(rig.scale, hold_s=hold_s)
    return dataclasses.replace(rig, fine=fine, scale=scale)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="pico2w_ar2209")
    parser.add_argument("--throws", type=int, default=400)
    parser.add_argument("--target", type=float, default=20.0)
    parser.add_argument("--least-rps", type=float, default=PICO2W_SLOW_FINISH.fine_min_rps,
                        help="the fine tube's least speed, at which it creeps to its stop")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    rig = rigs.get_rig(args.rig)
    variants = [(hold, burst, regularity, after)
                for hold in (None, 0.0) for burst in BURST_SHARES for regularity in REGULARITIES
                for after in LATE_SHARES]
    jobs, spans = [], []
    for hold, burst, regularity, after in variants:
        changed = kinder(rig, burst, regularity, after, hold)
        for stop in STOPS_GR:
            profile = dataclasses.replace(PICO2W_SLOW_FINISH, fine_min_rps=args.least_rps, fine_stop_threshold=stop)
            made = mc.session_jobs(changed, profile, args.target, args.throws, args.seed)
            spans.append(((hold, burst, regularity, after, stop), len(jobs), len(jobs) + len(made)))
            jobs.extend(made)
    with Workers() as pool:
        sessions = pool.map(mc.run_session, jobs)
    sums = {key: mc.summarise([result for session in sessions[first:last] for result in session])
            for key, first, last in spans}

    print(f"rig {args.rig}; today's loop, slow finish, least speed {args.least_rps:g} rev/s; {args.throws} throws a "
          f"stop at {args.target:g} gr; over and under are of the true weight, 0.03 gr and more off")
    for hold in (None, 0.0):
        print(f"\nthe scale {'as measured' if hold is None else 'showing every change at once (hold 0)'}")
        print(f"{'bursts kept':>12s}{'regularity':>12s}{'lateness':>12s}{'best stop':>11s}{'over':>7s}{'under':>7s}"
              f"{'good':>7s}{'spread gr':>11s}{'time s':>8s}")
        for burst in BURST_SHARES:
            for regularity in REGULARITIES:
                for after in LATE_SHARES:
                    best = max(STOPS_GR, key=lambda stop: sums[(hold, burst, regularity, after, stop)]["good_true"])
                    s = sums[(hold, burst, regularity, after, best)]
                    order = f"{rig.fine.regularity:g}" if regularity is None else f"{regularity:g}"
                    print(f"{burst:12.2f}{order:>12s}{after:12.1f}{best:11.2f}{s['over_true'] * 100:6.1f}%"
                          f"{s['under_true'] * 100:6.1f}%{s['good_true'] * 100:6.1f}%{s['error_sd']:11.3f}"
                          f"{s['time_median']:8.1f}")


if __name__ == "__main__":
    main()
