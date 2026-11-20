"""A ramp recording by plan: the powder a turn, what lands after the stop against what the last 0.6 s of turning
would account for, when it arrives, and the lag of the reading behind the tube's own speed that fits best."""

import json
import statistics
import sys
from pathlib import Path

DIGIT = 0.02
GR_PER_TURN = 0.765
PIPE_S = 0.6
FIXED = True     # the fit's grains a turn is the trial's own
MARKS = (0.2, 0.4, 0.6, 0.8, 1.0, 1.5, 2.0, 3.0)
LAGS = [0.2 + 0.05 * index for index in range(30)]


def at(series, t, column=1):
    last = series[0][column]
    for sample in series:
        if sample[0] > t:
            break
        last = sample[column]
    return last


def turned(during, since, until):
    """Turns of the tube between two moments, from its own reported speed."""
    total, prev_t, prev_v = 0.0, None, 0.0
    for t, _, v in during:
        if prev_t is not None:
            lo, hi = max(prev_t, since), min(t, until)
            if hi > lo:
                total += prev_v * (hi - lo)
        prev_t, prev_v = t, v
    return total


groups = {}
for folder in sys.argv[1:]:
    for trial in json.loads((Path(folder) / "runs.json").read_text(encoding="utf-8")):
        if trial["note"] == "ramp":
            groups.setdefault(trial["name"], []).append(trial)
for name, trials in groups.items():
    rises, nominal, per_turn, lags, gains = [], [], [], [], []
    by = {mark: [] for mark in MARKS}
    for trial in trials:
        during, after = trial["during"], trial["after"]
        first, at_stop = during[0][1], during[-1][1]
        end = statistics.median(w for t, w in after if t >= after[-1][0] - 0.6)
        rises.append((end - at_stop) / DIGIT)
        nominal.append(turned(during, -PIPE_S, 0.0) * GR_PER_TURN / DIGIT)
        per_turn.append((end - first) / trial["turns"])
        for mark in MARKS:
            by[mark].append((at(after, mark) - at_stop) / DIGIT)
        series = [(t, w) for t, w, _ in during] + list(after)
        start = during[0][0] + 0.1
        best = None
        for lag in LAGS:
            xs = [turned(during, during[0][0], t - lag) - turned(during, during[0][0], start - lag) for t, _ in series if t >= start]
            ys = [w - at(series, start) for t, w in series if t >= start]
            sxx = sum(x * x for x in xs)
            if sxx <= 0.0:
                continue
            gain = (end - first) / trial["turns"] if FIXED else sum(x * y for x, y in zip(xs, ys)) / sxx
            rms = statistics.fmean((y - gain * x) ** 2 for x, y in zip(xs, ys)) ** 0.5 / DIGIT
            if best is None or rms < best[2]:
                best = (lag, gain, rms)
        lags.append(best[0])
        gains.append(best[1])
    print(f"{name}: {len(trials)} trials; {statistics.fmean(per_turn):.3f} gr a turn (sd {statistics.pstdev(per_turn):.3f}); "
          f"after the stop {statistics.fmean(rises):+.1f} digits (sd {statistics.pstdev(rises):.1f}), the last {PIPE_S} s of turning "
          f"accounts for {statistics.fmean(nominal):.1f}")
    print("     arrived by " + "  ".join(f"{mark:g}s:{statistics.fmean(by[mark]):.1f}" for mark in MARKS))
    print(f"     the reading trails the tube's speed by {statistics.median(lags):.2f} s (from {min(lags):.2f} to {max(lags):.2f}), "
          f"at {statistics.median(gains):.2f} gr a turn")
