"""Watched nudges: how the scale shows a small change, by the change's size.

A watched nudge is a creep-stop trial with a turn of a few hundredths of a second and seconds of watching after
it (`record.py creepstop --speeds 0.2 --run-s 0.05:0.06 --still-s 5`). For each: the digits it gave in the end,
when the reading first moved after the nudge began, by how much, and for a release of two digits shown one at a
time, how long the second took after the first.

Usage:
    python nudge_watch.py <logs/adr009/..._nudgewatch> [...]
"""

import argparse
import json
import statistics
from pathlib import Path

DIGIT_GR = 0.02
LARGEST = 5         # releases of this many digits and more are one group
LATE_S = 1.5


def quantile(values: list, share: float) -> float:
    """The value a share of the way through the sorted values."""
    return values[int(share * (len(values) - 1))]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    args = parser.parse_args()
    for folder in args.folders:
        runs = [run for run in json.loads((folder / "runs.json").read_text(encoding="utf-8"))
                if run["note"] == "creepstop" and run["run_s"] < 1.0]
        sizes, first, second, turns = {}, {}, [], []
        for run in runs:
            base = run["during"][0][1]
            end = statistics.median(w for t, w in run["after"] if t >= run["after"][-1][0] - 0.6)
            size = min(max(round((end - base) / DIGIT_GR), 0), LARGEST)
            sizes[size] = sizes.get(size, 0) + 1
            turns.append(run["turns"])
            rises, shown = [], base
            for t, weight in run["during"] + run["after"]:
                if weight != shown:
                    shown = weight
                    if weight > base:
                        rises.append((t + run["run_s"], round((weight - base) / DIGIT_GR)))
            if size >= 1 and rises:
                first.setdefault(size, []).append(rises[0])
                reached = [t for t, digits in rises if digits >= 2]
                if size == 2 and rises[0][1] == 1 and reached:
                    second.append(reached[0] - rises[0][0])
        count = len(runs)
        print(f"{folder.name}: {count} nudges of {statistics.fmean(turns):.4f} turn, "
              f"{sum(size * n for size, n in sizes.items()) / count:.2f} digits a nudge")
        print("  share by digits given (0, 1, 2, ...; the last is that many and more): "
              + ", ".join(f"{sizes.get(size, 0) / count:.3f}" for size in range(LARGEST + 1)))
        for size in sorted(first):
            times = sorted(t for t, _ in first[size])
            jumps = [digits for _, digits in first[size]]
            print(f"  {size} digits, {len(times)} releases: the reading first moves {quantile(times, 0.1):.2f}, "
                  f"{quantile(times, 0.25):.2f}, {quantile(times, 0.5):.2f}, {quantile(times, 0.75):.2f}, "
                  f"{quantile(times, 0.9):.2f} s after the nudge began (a tenth, a quarter, half, three quarters, nine "
                  f"tenths of them); later than {LATE_S} s: {sum(t > LATE_S for t in times) / len(times):.2f}; "
                  f"first jump {statistics.fmean(jumps):.1f} digits")
        if second:
            print(f"  two digits shown one at a time: {len(second)} releases, the second "
                  f"{statistics.median(second):.2f} s after the first (median)")


if __name__ == "__main__":
    main()
