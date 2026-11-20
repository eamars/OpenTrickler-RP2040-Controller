"""Many throws of a controller on a rig, summed up: how often over, how often under, how wide, how long.

Usage: python mc.py [--rig bench_pico2w] [--controller legacy_slow_finish] [--target 20] [--throws 500] [--seed 1]
                    [--scale dead_s=0.8,report_s=0.25]
"""

import argparse
import dataclasses
import statistics
from concurrent.futures import ProcessPoolExecutor

import rigs
from controllers import CONTROLLERS, VERDICT_TOLERANCE_GR
from legacy_pid import LegacyPid, Profile
from throw import Session

THROWS_PER_SESSION = 25     # a session keeps the tubes' state from throw to throw, as a loading session does


def run_session(job: tuple) -> list:
    """
    One session of throws in a worker.

    Args:
        job: (rig, controller, target, throws, seed). The controller is a name in CONTROLLERS, or a Profile for
            today's loop with those settings.

    Returns:
        the throws' results.
    """
    rig, controller, target, throws, seed = job
    session = Session(rig, seed)
    if isinstance(controller, Profile):
        made, tolerance = LegacyPid(controller), VERDICT_TOLERANCE_GR
    else:
        made, tolerance = CONTROLLERS[controller](rig)
    return [session.throw(made, target, tolerance) for _ in range(throws)]


def session_jobs(rig, controller, target: float, throws: int, seed: int) -> list:
    """The jobs that make up a run of throws: sessions of THROWS_PER_SESSION, each with its own seed."""
    sessions = max(1, throws // THROWS_PER_SESSION)
    return [(rig, controller, target, THROWS_PER_SESSION, seed * 100003 + index) for index in range(sessions)]


def run_many(rig, controller, target: float, throws: int, seed: int, pool=None) -> list:
    """Throws spread over sessions and processor cores; the same seed gives the same throws."""
    jobs = session_jobs(rig, controller, target, throws, seed)
    if pool is not None:
        return [result for results in pool.map(run_session, jobs) for result in results]
    with ProcessPoolExecutor() as own:
        return [result for results in own.map(run_session, jobs) for result in results]


def summarise(results: list, tolerance: float = VERDICT_TOLERANCE_GR) -> dict:
    """
    The figures a controller is ranked by.

    Args:
        results: the throws.
        tolerance: a throw is over or under from this far off the target.

    Returns:
        shares of all throws (over and under as the unit showed them and as they truly were, good as they truly
        were, timeouts); the true error's mean, spread and most, in grains, and error_band, half the width that
        holds the middle 68% of the errors (a spread one stray throw does not move); the time's median and 95%; and
        s_per_good, the median time divided by the share of good throws: what a good charge costs when every
        bad one is thrown again.
    """
    count = len(results)
    done = [r for r in results if r.verdict != "timeout"]
    errors = sorted(r.true_error for r in done)
    times = sorted(r.elapsed_s for r in done)
    over = sum(r.true_error >= tolerance for r in done) / count
    under = sum(r.true_error <= -tolerance for r in done) / count
    good = sum(abs(r.true_error) < tolerance for r in done) / count
    median = times[len(times) // 2]
    return {
        "throws": count,
        "over_shown": sum(r.verdict == "over" for r in results) / count,
        "under_shown": sum(r.verdict == "under" for r in results) / count,
        "over_true": over,
        "under_true": under,
        "good_true": good,
        "timeout": (count - len(done)) / count,
        "error_mean": statistics.fmean(errors),
        "error_sd": statistics.pstdev(errors),
        "error_band": (errors[int(0.84 * (len(errors) - 1))] - errors[int(0.16 * (len(errors) - 1))]) / 2.0,
        "error_max": errors[-1],
        "time_median": median,
        "time_p95": times[int(0.95 * (len(times) - 1))],
        "s_per_good": median / good if good > 0.0 else float("inf"),
    }


def format_summary(label: str, summary: dict) -> str:
    """One line of a comparison table."""
    return (f"{label:34s} over {summary['over_shown'] * 100:5.1f}% (true {summary['over_true'] * 100:5.1f}%)  "
            f"under {summary['under_shown'] * 100:5.1f}% (true {summary['under_true'] * 100:5.1f}%)  "
            f"error {summary['error_mean']:+.3f} sd {summary['error_sd']:.3f} max {summary['error_max']:+.2f}  "
            f"time {summary['time_median']:5.1f} s, 95% {summary['time_p95']:5.1f} s  "
            f"timeout {summary['timeout'] * 100:.1f}%")


def with_scale(rig, changes: str):
    """The rig with some of its scale's figures changed: 'dead_s=0.8,report_s=0.25'."""
    if not changes:
        return rig
    values = {name: float(value) for name, value in (item.split("=") for item in changes.split(","))}
    return dataclasses.replace(rig, scale=dataclasses.replace(rig.scale, **values))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="bench_pico2w", help="a record's name in records/, or a path to one")
    parser.add_argument("--controller", default="legacy_slow_finish", choices=sorted(CONTROLLERS))
    parser.add_argument("--target", type=float, default=20.0)
    parser.add_argument("--throws", type=int, default=500)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--scale", default="", help="the scale's figures to change: name=value,...")
    args = parser.parse_args()

    rig = with_scale(rigs.get_rig(args.rig), args.scale)
    results = run_many(rig, args.controller, args.target, args.throws, args.seed)
    print(format_summary(f"{args.controller} / {args.rig}", summarise(results)))


if __name__ == "__main__":
    main()
