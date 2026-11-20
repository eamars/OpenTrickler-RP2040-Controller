"""Creep-stop trials by speed: the flow before the stop, what lands after it, and when it arrives."""

import json
import statistics
import sys
from pathlib import Path

DIGIT = 0.02
MARKS = (0.2, 0.4, 0.6, 0.8, 1.0, 1.5, 2.0, 3.0)


def at(series, t):
    last = series[0][1]
    for sample in series:
        if sample[0] > t:
            break
        last = sample[1]
    return last


groups = {}
for folder in sys.argv[1:]:
    for trial in json.loads((Path(folder) / "runs.json").read_text(encoding="utf-8")):
        if trial["note"] != "creepstop" or trial["run_s"] > 30:
            continue
        groups.setdefault((trial.get("tube", "fine"), trial["speed_asked_rps"], trial.get("stop", "held" if trial["held"] else "released")), []).append(trial)
for (tube, speed, stop), trials in sorted(groups.items()):
    rates, rises, by = [], [], {mark: [] for mark in MARKS}
    for trial in trials:
        during, after = trial["during"], trial["after"]
        at_stop = during[-1][1]
        span = min(1.5, trial["run_s"] - 0.5)
        rates.append((at_stop - at(during, -span)) / span / DIGIT)
        end = statistics.median(w for t, w in after if t >= after[-1][0] - 0.6)
        rises.append((end - at_stop) / DIGIT)
        for mark in MARKS:
            by[mark].append((at(after, mark) - at_stop) / DIGIT)
    rate, rise = statistics.fmean(rates), statistics.fmean(rises)
    print(f"{tube} {speed:g} rev/s {stop}: {len(trials)} stops; {rate:.1f} digits/s before; after the stop {rise:+.1f} digits "
          f"(sd {statistics.pstdev(rises):.1f}) = {rise / rate:.2f} s of the flow; beyond 0.6 s of it: {rise - 0.6 * rate:+.1f} digits")
    print("     arrived by " + "  ".join(f"{mark:g}s:{statistics.fmean(by[mark]):.1f}" for mark in MARKS))
