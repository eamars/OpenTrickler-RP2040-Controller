"""The creep-stop run in detail: when the rise after the stop arrives, and how even the flow was before it."""
import json
import statistics
import sys
from pathlib import Path

DIGIT = 0.02
runs = [r for r in json.loads((Path(sys.argv[1]) / "runs.json").read_text(encoding="utf-8")) if r["note"] == "creepstop"]


def at(series, t):
    """The last reading at or before a time."""
    value = series[0][1]
    for when, weight in series:
        if when > t:
            break
        value = weight
    return value


print("samples a second during:", round(statistics.fmean(len(r["during"]) / r["run_s"] for r in runs), 1))
for speed in sorted({r["speed_asked_rps"] for r in runs}):
    group = [r for r in runs if r["speed_asked_rps"] == speed]
    print(f"\n=== {speed} rev/s, {len(group)} stops (both kinds)")
    # the rise after the stop, by time
    for mark in (0.2, 0.4, 0.6, 0.8, 1.0, 1.5, 2.0, 2.9):
        rises = [(at(r["after"], mark) - r["during"][-1][1]) / DIGIT for r in group]
        print(f"  by {mark:.1f} s after the stop: mean +{statistics.fmean(rises):.2f} digits, spread {statistics.pstdev(rises):.2f}")
    # the flow before the stop: digits gained in each whole second counted back from the stop
    per_second = {}
    for r in group:
        for back in range(1, int(r["run_s"])):
            gain = (at(r["during"], -back + 1.0 - 1e-6) - at(r["during"], -back)) / DIGIT
            per_second.setdefault(back, []).append(gain)
    for back in sorted(per_second):
        g = per_second[back]
        print(f"  second {back} before the stop: n={len(g)} mean {statistics.fmean(g):.2f} digits, variance/mean "
              f"{statistics.pvariance(g) / max(statistics.fmean(g), 1e-9):.2f}, most {max(g):.0f}")
    # by time from the start
    from_start = {}
    for r in group:
        for second in range(0, int(r["run_s"])):
            a, b = -r["run_s"] + second, -r["run_s"] + second + 1.0
            from_start.setdefault(second, []).append((at(r["during"], b - 1e-6) - at(r["during"], a)) / DIGIT)
    for second in sorted(from_start):
        g = from_start[second]
        print(f"  second {second} from the start: n={len(g)} mean {statistics.fmean(g):.2f} digits, variance/mean "
              f"{statistics.pvariance(g) / max(statistics.fmean(g), 1e-9):.2f}, most {max(g):.0f}")
    # does the last second's rise foretell the rise after the stop?
    last = [(at(r["during"], 0) - at(r["during"], -1.0)) / DIGIT for r in group]
    gain = [(statistics.median(w for t, w in r["after"] if t >= r["after"][-1][0] - 0.6) - r["during"][-1][1]) / DIGIT for r in group]
    print(f"  correlation of the last second's rise with the rise after the stop: {statistics.correlation(last, gain):.2f}")
    total = [(statistics.median(w for t, w in r["after"] if t >= r["after"][-1][0] - 0.6) - r["during"][0][1]) / DIGIT for r in group]
    turns = [r["turns"] for r in group]
    per_turn = [t / n for t, n in zip(total, turns)]
    print(f"  whole trial: digits a turn mean {statistics.fmean(per_turn):.1f}, spread {statistics.pstdev(per_turn):.1f}; "
          f"trial total digits mean {statistics.fmean(total):.1f} spread {statistics.pstdev(total):.1f}")
    print("  the largest rises after the stop, their traces (s, digits above the stop's reading):")
    for r in sorted(group, key=lambda r: -gain[group.index(r)])[:5]:
        base = r["during"][-1][1]
        trace = [(round(t, 1), round((at(r["after"], t) - base) / DIGIT)) for t in (0.1, 0.3, 0.5, 0.7, 0.9, 1.2, 1.6, 2.0, 2.5, 2.9)]
        before = [round((at(r["during"], t) - base) / DIGIT) for t in (-2.0, -1.5, -1.0, -0.5, -0.2)]
        print(f"    held={r['held']} before {before} after {trace}")
