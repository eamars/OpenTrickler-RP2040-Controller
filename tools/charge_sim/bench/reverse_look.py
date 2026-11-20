"""The reverse-length runs and the back-and-forth runs, read with no model.

Reverse lengths: forward at 0.2 rev/s, back by a part of a turn, forward again for 6 s. What the reverse turn
costs: the digits the second forward turn and its stop gave, against a forward turn of the same length with no
reverse before it; and how the second turn's flow comes back, second by second.

Back and forth: a minute with a net 0.05 rev/s forward, steady or in swings: the net flow and how evenly it
came (variance over mean of the digits a window)."""

import json
import statistics
import sys
from pathlib import Path

DIGIT = 0.02
DIGITS_PER_TURN = 0.765 / DIGIT
PIPE_S = 0.6


def at(series, t):
    last = series[0][1]
    for sample in series:
        if sample[0] > t:
            break
        last = sample[1]
    return last


def fano(series, start, end, window):
    gains, t = [], start
    while t + window <= end:
        gains.append((at(series, t + window) - at(series, t)) / DIGIT)
        t += window
    mean = statistics.fmean(gains)
    return statistics.pvariance(gains) / mean if mean > 0 else float("nan")


for folder in sys.argv[1:]:
    trials = [t for t in json.loads((Path(folder) / "runs.json").read_text(encoding="utf-8")) if t["note"] == "ramp"]
    groups = {}
    for trial in trials:
        groups.setdefault(trial["name"], []).append(trial)
    for name, rows in sorted(groups.items()):
        if name.startswith("back"):
            costs, spills, by = [], [], {s: [] for s in (1, 2, 3, 4, 5, 6)}
            for trial in rows:
                series = [(t, w) for t, w, _ in trial["during"]] + list(trial["after"])
                (t0, _), (t1, _), (t2, v2) = trial["marks"]
                end = statistics.median(w for t, w in trial["after"] if t >= trial["after"][-1][0] - 0.6)
                back_turn = 0.2 * (t2 - t1)
                # what the first forward turn's pipe still held when the reverse began is not the reverse's
                at_reverse = at(series, t1 + PIPE_S)
                spills.append((at(series, t2 + PIPE_S) - at_reverse) / DIGIT)
                given = (end - at(series, t2 + PIPE_S)) / DIGIT
                forward_turn = v2 * (0.0 - t2)
                costs.append(forward_turn * DIGITS_PER_TURN - given)
                for s in by:
                    by[s].append((at(series, t2 + PIPE_S + s) - at(series, t2 + PIPE_S)) / DIGIT)
            steady = 0.2 * DIGITS_PER_TURN
            print(f"{name}: {len(rows)} trials, back by {back_turn:.3f} turn; while turning back {statistics.fmean(spills):+.1f} digits "
                  f"(sd {statistics.pstdev(spills):.1f}); the forward turn after it gave {statistics.fmean(costs):.1f} digits less than its "
                  f"turns (sd {statistics.pstdev(costs):.1f}) = {statistics.fmean(costs) / DIGITS_PER_TURN:.3f} turn")
            print("     digits by each second of the forward turn after it: "
                  + "  ".join(f"{s}s:{statistics.fmean(by[s]):.1f} (steady {steady * s:.1f})" for s in by))
        else:
            for trial in rows:
                series = [(t, w) for t, w, _ in trial["during"]]
                start, stop = series[0][0] + 2.0, 0.0
                rate = (at(series, stop) - at(series, start)) / (stop - start) / DIGIT
                print(f"{name}: {stop - series[0][0]:.0f} s, {rate:.2f} digits/s net; variance over mean of the digits in 1, 2, 4 s: "
                      f"{fano(series, start, stop, 1.0):.2f}, {fano(series, start, stop, 2.0):.2f}, {fano(series, start, stop, 4.0):.2f}; "
                      f"largest second {max((at(series, t + 1) - at(series, t)) / DIGIT for t in [start + i for i in range(int(stop - start) - 1)]):.0f}")
