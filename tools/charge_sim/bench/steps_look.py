"""Speed steps with no stop between: at each change of speed, when the reading's slope changes and whether
powder is gained or lost against a clean change 0.6 s after the command.

Two straight lines are fitted to the reading, one before the change (its last seconds) and one after (from 1.5 s
after it), and set against each other: where they cross is when the flow changed as the scale shows it; their
difference 0.6 s after the command is the powder more (or less) than a clean change would give."""

import json
import statistics
import sys
from pathlib import Path

DIGIT = 0.02
PIPE_S = 0.6


def line(points):
    """Least-squares slope and intercept of (t, w)."""
    n = len(points)
    mt = sum(t for t, _ in points) / n
    mw = sum(w for _, w in points) / n
    stt = sum((t - mt) ** 2 for t, _ in points)
    slope = sum((t - mt) * (w - mw) for t, w in points) / stt
    return slope, mw - slope * mt


found = {}
for folder in sys.argv[1:]:
    for trial in json.loads((Path(folder) / "runs.json").read_text(encoding="utf-8")):
        if trial["note"] != "ramp" or trial["name"] != "steps":
            continue
        marks = trial["marks"] + [(0.0, 0.0)]
        series = [(t, w) for t, w, _ in trial["during"]]
        for index in range(1, len(marks) - 1):
            change, old, new = marks[index][0], marks[index - 1][1], marks[index][1]
            before_from = max(marks[index - 1][0] + 2.0, change - 4.0)
            after_to = marks[index + 1][0] + 0.3
            before = [(t, w) for t, w in series if before_from <= t <= change + 0.3]
            after = [(t, w) for t, w in series if change + 1.8 <= t <= after_to]
            if len(before) < 10 or len(after) < 10:
                continue
            s1, i1 = line(before)
            s2, i2 = line(after)
            cross = (i1 - i2) / (s2 - s1)
            at = change + PIPE_S
            extra = ((s2 * at + i2) - (s1 * at + i1)) / DIGIT
            found.setdefault((old, new), []).append((cross - change, extra, s1 / DIGIT, s2 / DIGIT))
for (old, new), rows in sorted(found.items()):
    print(f"{old:g} -> {new:g} rev/s: {len(rows)} changes; flow {statistics.fmean(r[2] for r in rows):.1f} -> "
          f"{statistics.fmean(r[3] for r in rows):.1f} digits/s; the reading's slope changes "
          f"{statistics.median(r[0] for r in rows):.2f} s after the command (from {min(r[0] for r in rows):.2f} to {max(r[0] for r in rows):.2f}); "
          f"powder against a clean change at {PIPE_S} s: {statistics.fmean(r[1] for r in rows):+.1f} digits "
          f"({' '.join(f'{r[1]:+.0f}' for r in rows)})")
