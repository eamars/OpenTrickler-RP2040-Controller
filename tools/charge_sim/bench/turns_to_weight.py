"""Is the powder poured a matter of how far the tube turned, or of how fast?

Every open-loop trial that begins and ends at rest (a stop from a steady turn, a ramp, a speed-step run) is one
point: the tube's true turns, from the unit's count of its steps, against the digits in the cup once all has
landed. By tube and by speed: the grains a turn, what a start and a stop cost together (the line's value at no
turn), and how far a trial falls from its line. Then all speeds on one line: how much is lost by forgetting the
speed altogether.

Usage:
    python turns_to_weight.py <logs/adr009> [--since 20261120] [--folders <name> ...]
"""

import argparse
import json
import statistics
from pathlib import Path

DIGIT_GR = 0.02
KERNEL_GR = 0.021


def line(points: list) -> tuple:
    """Least squares through (x, y): slope, intercept, and the spread of the points about it."""
    count = len(points)
    mean_x = sum(x for x, _ in points) / count
    mean_y = sum(y for _, y in points) / count
    sxx = sum((x - mean_x) ** 2 for x, _ in points)
    if sxx <= 0.0:
        return 0.0, mean_y, statistics.pstdev(y for _, y in points)
    slope = sum((x - mean_x) * (y - mean_y) for x, y in points) / sxx
    intercept = mean_y - slope * mean_x
    spread = (sum((y - intercept - slope * x) ** 2 for x, y in points) / max(1, count - 2)) ** 0.5
    return slope, intercept, spread


def through_zero(points: list) -> tuple:
    """The line through no turn, no powder: its slope and the spread about it."""
    slope = sum(x * y for x, y in points) / sum(x * x for x, _ in points)
    spread = (sum((y - slope * x) ** 2 for x, y in points) / max(1, len(points) - 1)) ** 0.5
    return slope, spread


def trials_of(logs: Path, since: str, names: list) -> list:
    """(tube, kind, speed or plan, turns, digits poured) for every trial that began and ended at rest, of the
    folders named, or of every stop and ramp folder from a day on when none is."""
    out = []
    folders = [logs / name for name in names] or sorted(logs.glob("*_creepstop")) + sorted(logs.glob("*_ramp"))
    for folder in folders:
        if (not names and folder.name[:8] < since) or not (folder / "runs.json").exists():
            continue
        for run in json.loads((folder / "runs.json").read_text(encoding="utf-8")):
            if run["note"] not in ("creepstop", "ramp") or "turns" not in run:
                continue
            if run["note"] == "creepstop" and run.get("stop", "held") == "backoff":
                continue        # a reverse turn's steps are counted with the forward ones
            if run["note"] == "ramp" and any(speed < 0.0 for _, speed in run["marks"]):
                continue
            if run["note"] == "ramp" and run["name"] == "prime":
                continue        # a tube being filled is not yet the tube
            after = run["after"]
            end = statistics.median(w for t, w in after if t >= after[-1][0] - 0.6)
            poured = (end - run["during"][0][1]) / DIGIT_GR
            kind = f"{run['speed_asked_rps']:g} rev/s" if run["note"] == "creepstop" else f"plan {run['name']}"
            out.append((run.get("tube", "fine"), kind, run["turns"], poured))
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("logs", type=Path)
    parser.add_argument("--since", default="20261120", help="folders from this day on (their names' first eight)")
    parser.add_argument("--folders", nargs="*", default=[], help="only these folders of the logs")
    args = parser.parse_args()
    trials = trials_of(args.logs, args.since, args.folders)
    for tube in ("fine", "coarse"):
        mine = [trial for trial in trials if trial[0] == tube]
        if not mine:
            continue
        print(f"{tube} tube, {len(mine)} trials")
        for kind in sorted({trial[1] for trial in mine}, key=lambda text: (text.startswith("plan"), text)):
            points = [(turns, poured) for _, k, turns, poured in mine if k == kind]
            if len(points) < 3:
                slope, _ = through_zero(points)
                print(f"  {kind:18s} {len(points):3d} trials, {min(x for x, _ in points):.2f} to {max(x for x, _ in points):.2f} "
                      f"turns: {slope * DIGIT_GR:.3f} gr a turn")
                continue
            slope, intercept, spread = line(points)
            zero_slope, zero_spread = through_zero(points)
            turns = statistics.fmean(x for x, _ in points)
            kernels = turns * zero_slope * DIGIT_GR / KERNEL_GR
            print(f"  {kind:18s} {len(points):3d} trials, {min(x for x, _ in points):.2f} to {max(x for x, _ in points):.2f} "
                  f"turns: {zero_slope * DIGIT_GR:.3f} gr a turn through nothing, a trial {zero_spread:.1f} digits off it "
                  f"(variance over mean {(zero_spread * DIGIT_GR / KERNEL_GR) ** 2 / kernels:.2f}); "
                  f"free line {slope * DIGIT_GR:.3f} gr a turn, {intercept:+.1f} digits at no turn, {spread:.1f} off it")
        points = [(turns, poured) for _, _, turns, poured in mine]
        slope, spread = through_zero(points)
        print(f"  every speed on one line: {slope * DIGIT_GR:.3f} gr a turn; a trial is {spread:.1f} digits off it "
              f"({spread * DIGIT_GR:.3f} gr)")
        short = [(x, y) for x, y in points if x <= 3.0]
        if len(short) >= 10:
            slope, spread = through_zero(short)
            print(f"  trials of up to 3 turns on one line: {slope * DIGIT_GR:.3f} gr a turn; {spread:.1f} digits off it")


if __name__ == "__main__":
    main()
