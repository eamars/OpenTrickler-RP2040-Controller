"""Many throws of a controller on a rig, summed up: how often over, how often under, how wide, how long.

Usage: python mc.py [--controller legacy] [--powder ar2209] [--scale and_fast] [--target 42.5] [--throws 500]
                    [--seed 1] [--plot out.png]
"""

import argparse
import statistics
from concurrent.futures import ProcessPoolExecutor

from controllers import CONTROLLERS
from rigs import POWDERS, make_rig
from scale import SCALES
from throw import Session

THROWS_PER_SESSION = 25     # a session keeps the tubes' state from throw to throw, as a loading session does


def run_session(job: tuple) -> list:
    """One session of throws in a worker. job: (controller name, powder, scale, target, throws, seed)."""
    controller_name, powder, scale, target, throws, seed = job
    rig = make_rig(powder, scale)
    session = Session(rig, seed)
    controller, tolerance = CONTROLLERS[controller_name](rig)
    return [session.throw(controller, target, tolerance) for _ in range(throws)]


def run_many(controller: str, powder: str, scale: str, target: float, throws: int, seed: int) -> list:
    """Throws spread over sessions and processor cores; the same seed gives the same throws."""
    sessions = max(1, throws // THROWS_PER_SESSION)
    jobs = [(controller, powder, scale, target, THROWS_PER_SESSION, seed * 100003 + index)
            for index in range(sessions)]
    with ProcessPoolExecutor() as pool:
        return [result for results in pool.map(run_session, jobs) for result in results]


def summarise(results: list, tolerance: float) -> dict:
    """The figures a controller is ranked by. Shares are of all throws; errors in grains, true weight less target."""
    count = len(results)
    done = [r for r in results if r.verdict != "timeout"]
    errors = sorted(r.true_error for r in done)
    times = sorted(r.elapsed_s for r in done)
    return {
        "throws": count,
        "over_shown": sum(r.verdict == "over" for r in results) / count,
        "under_shown": sum(r.verdict == "under" for r in results) / count,
        "over_true": sum(r.true_error >= tolerance for r in done) / count,
        "under_true": sum(r.true_error <= -tolerance for r in done) / count,
        "timeout": (count - len(done)) / count,
        "error_mean": statistics.fmean(errors),
        "error_sd": statistics.pstdev(errors),
        "error_max": errors[-1],
        "time_median": times[len(times) // 2],
        "time_p95": times[int(0.95 * (len(times) - 1))],
    }


def format_summary(label: str, summary: dict) -> str:
    """One line of a comparison table."""
    return (f"{label:34s} over {summary['over_shown'] * 100:5.1f}% (true {summary['over_true'] * 100:5.1f}%)  "
            f"under {summary['under_shown'] * 100:5.1f}% (true {summary['under_true'] * 100:5.1f}%)  "
            f"error {summary['error_mean']:+.3f} sd {summary['error_sd']:.3f} max {summary['error_max']:+.2f}  "
            f"time {summary['time_median']:5.1f} s, 95% {summary['time_p95']:5.1f} s  "
            f"timeout {summary['timeout'] * 100:.1f}%")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--controller", default="legacy", choices=sorted(CONTROLLERS))
    parser.add_argument("--powder", default="ar2209", choices=sorted(POWDERS))
    parser.add_argument("--scale", default="and_fast", choices=sorted(SCALES))
    parser.add_argument("--target", type=float, default=42.5)
    parser.add_argument("--throws", type=int, default=500)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    results = run_many(args.controller, args.powder, args.scale, args.target, args.throws, args.seed)
    _, tolerance = CONTROLLERS[args.controller](make_rig(args.powder, args.scale))
    print(format_summary(f"{args.controller} / {args.powder} / {args.scale}", summarise(results, tolerance)))


if __name__ == "__main__":
    main()
