"""How evenly kernels leave the fine tube, from recordings already made: a steady slow turn against nudges.

For each slow run of a `runs` folder: the kernels gained in windows of 1, 2 and 4 s, and their variance over their
mean. Kernels leaving singly and at random give 1; leaving together gives the mean size of a release or more.
For a `nudges` folder: the same figure from each nudge's gain, and each release's size with the time its reading
first moved.

Usage:
    python steady_release.py runs <logs/adr009/..._runs>
    python steady_release.py nudges <logs/adr009/..._nudges>
"""

import json
import statistics
import sys
from pathlib import Path

KERNEL_GR = 0.021
SLOWEST_WORTH_READING_RPS = 0.5


def load(folder: Path) -> tuple:
    """A folder's frames (unit time in ms, weight, header) and its runs."""
    frames = []
    for line in (folder / "frames.csv").read_text(encoding="utf-8").splitlines()[1:]:
        t_ms, weight, header = line.split(",")
        if weight not in ("nan", "inf"):
            frames.append((float(t_ms), float(weight), header))
    return frames, json.loads((folder / "runs.json").read_text(encoding="utf-8"))


def weight_at(frames: list, t_ms: float) -> float:
    """The reading nearest a time."""
    return min(frames, key=lambda frame: abs(frame[0] - t_ms))[1]


def steady(folder: Path) -> None:
    """Each slow run's kernels a window, and the largest rise within six frames."""
    frames, runs = load(folder)
    for run in runs:
        if run["note"] != "map" or run["speed_asked_rps"] > SLOWEST_WORTH_READING_RPS:
            continue
        start, end = run["t_before_ms"] + 2000.0, run["t_stop_asked_ms"]
        span = [weight for t_ms, weight, _ in frames if start <= t_ms <= end]
        rate = (span[-1] - span[0]) / ((end - start) / 1000.0) / KERNEL_GR
        print(f"fine {run['speed_asked_rps']:g} rev/s, {(end - start) / 1000.0:.0f} s, {rate:.2f} kernels/s")
        for window_s in (1.0, 2.0, 4.0):
            gains = []
            at = start
            while at + window_s * 1000.0 <= end:
                gains.append((weight_at(frames, at + window_s * 1000.0) - weight_at(frames, at)) / KERNEL_GR)
                at += window_s * 1000.0
            if len(gains) < 4:
                continue
            mean = statistics.fmean(gains)
            print(f"  windows of {window_s:g} s: {len(gains)}, mean {mean:.2f} kernels, variance over mean "
                  f"{statistics.pvariance(gains) / mean:.2f}, least {min(gains):.1f}, most {max(gains):.1f}")
        rises = [(span[index + 6] - span[index]) / KERNEL_GR for index in range(len(span) - 6)]
        print(f"  largest rise within six frames (0.3 s): {max(rises):.1f} kernels")


def nudged(folder: Path) -> None:
    """The nudges' gains as one figure, and each release with the time its reading first moved."""
    frames, runs = load(folder)
    gains, releases = [], []
    for run in runs:
        span = [(t_ms, weight) for t_ms, weight, _ in frames if run["t_before_ms"] - 600.0 <= t_ms <= run["t_after_ms"]]
        before = [weight for t_ms, weight in span if t_ms <= run["t_before_ms"]]
        after = [weight for t_ms, weight in span if t_ms >= run["t_after_ms"] - 700.0]
        if not before or not after:
            continue
        gain = (statistics.fmean(after) - statistics.fmean(before)) / KERNEL_GR
        gains.append(gain)
        if gain >= 0.7:
            moved = [t_ms for t_ms, weight in span
                     if t_ms > run["t_before_ms"] and weight >= statistics.fmean(before) + 0.01]
            releases.append((round(gain, 1), round(moved[0] - run["t_before_ms"]) if moved else None))
    mean = statistics.fmean(gains)
    print(f"{len(gains)} nudges, mean {mean:.2f} kernels, variance over mean {statistics.pvariance(gains) / mean:.2f}")
    print("releases (kernels, ms until the reading first moved):", sorted(releases))


if __name__ == "__main__":
    if sys.argv[1] == "runs":
        steady(Path(sys.argv[2]))
    else:
        nudged(Path(sys.argv[2]))
