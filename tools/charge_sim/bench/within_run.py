"""Inside a steady run, with no start and no stop: the spread of the powder poured over a part of a turn.

A scatter that grew as kernels leaving at random would grow with the square root of the turn. One that levels
off says the tube delivers a fixed amount a turn into something small at the lip whose content wanders.

Usage:
    python within_run.py <logs/adr009> [--since 20261120] [--tube fine]
"""

import argparse
import json
import statistics
from pathlib import Path

DIGIT_GR = 0.02
PARTS = (0.05, 0.1, 0.25, 0.5, 1.0, 2.0)
LEAST_RUN_S = 18.0
SKIP_S = 2.5            # a run's start is not its steady flow
LEAST_WINDOW_S = 0.5    # shorter than this the scale's own delay and digit are what is seen
LEAST_WINDOWS = 8


def reading_at(series: list, t: float) -> float:
    """The last reading at or before a time."""
    last = series[0][1]
    for sample in series:
        if sample[0] > t:
            break
        last = sample[1]
    return last


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("logs", type=Path)
    parser.add_argument("--since", default="20261120")
    parser.add_argument("--tube", default="fine", choices=("coarse", "fine"))
    args = parser.parse_args()
    runs = []
    for folder in sorted(args.logs.glob("*_creepstop")):
        if folder.name[:8] < args.since:
            continue
        runs += [run for run in json.loads((folder / "runs.json").read_text(encoding="utf-8"))
                 if run["note"] == "creepstop" and run["run_s"] >= LEAST_RUN_S and run.get("tube", "fine") == args.tube]
    print(f"{args.tube} tube: spread of the digits poured over a part of a turn, inside steady runs")
    print(f"{'rev/s':>6s} {'run s':>6s} " + " ".join(f"{part:>7g}t" for part in PARTS) + "   digits a turn")
    for run in sorted(runs, key=lambda run: run["speed_asked_rps"]):
        speed, series = run["speed_asked_rps"], run["during"]
        start, end = series[0][0] + SKIP_S, series[-1][0]
        rate = (reading_at(series, end) - reading_at(series, start)) / (end - start) / DIGIT_GR
        cells = []
        for part in PARTS:
            window = part / speed
            if window < LEAST_WINDOW_S or (end - start) / window < LEAST_WINDOWS:
                cells.append(f"{'-':>8s}")
                continue
            gains, t = [], start
            while t + window <= end:
                gains.append((reading_at(series, t + window) - reading_at(series, t)) / DIGIT_GR)
                t += window
            cells.append(f"{statistics.pstdev(gains):8.1f}")
        print(f"{speed:6g} {run['run_s']:6.0f} " + " ".join(cells) + f"   {rate / speed:.1f}")


if __name__ == "__main__":
    main()
