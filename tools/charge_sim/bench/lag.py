"""From recorded nudges: how long after a nudge the reading first moves and when it stops moving. And a long run's flow by time."""
import json, statistics, sys
from pathlib import Path

def load(folder):
    frames = []
    for line in open(folder / "frames.csv", encoding="utf-8").read().splitlines()[1:]:
        t, w, h = line.split(",")
        frames.append((float(t), float(w), h))
    return frames, json.loads((folder / "runs.json").read_text(encoding="utf-8"))

mode, folder = sys.argv[1], Path(sys.argv[2])
frames, runs = load(folder)
if mode == "nudges":
    first, last, unstable = [], [], []
    for run in runs:
        span = [(t, w, h) for t, w, h in frames if run["t_before_ms"] - 300 <= t <= run["t_after_ms"]]
        start_w = statistics.fmean(w for t, w, h in span if t <= run["t_before_ms"] + 40)
        end_w = statistics.fmean(w for t, w, h in span if t >= run["t_after_ms"] - 500)
        if end_w - start_w < 0.015:
            continue
        moved = [t for t, w, h in span if t > run["t_before_ms"] and w >= start_w + 0.01]
        settled = [t for t, w, h in span if t > run["t_before_ms"] and abs(w - end_w) > 0.011]
        first.append(moved[0] - run["t_before_ms"])
        last.append((settled[-1] if settled else moved[0]) - run["t_before_ms"])
        unstable.append(sum(1 for t, w, h in span if h != "ST" and t > run["t_before_ms"]))
    q = lambda v, p: sorted(v)[int(p * (len(v) - 1))]
    print(f"{len(first)} nudges that released: reading first moved after median {q(first, .5):.0f} ms (10% {q(first, .1):.0f}, 90% {q(first, .9):.0f}); "
          f"last frame off the end value at median {q(last, .5):.0f} ms (90% {q(last, .9):.0f}); unstable frames per nudge median {q(unstable, .5)}")
else:
    marks = [r for r in runs if r["note"].startswith("step")]
    start, end = marks[0]["t_ms"], marks[-1]["t_ms"] - 5000
    print("flow by second from the step's start:")
    for second in range(int((end - start) / 1000)):
        a = [w for t, w, h in frames if abs(t - (start + second * 1000)) < 30]
        b = [w for t, w, h in frames if abs(t - (start + (second + 1) * 1000)) < 30]
        if a and b:
            print(f"  {second}-{second + 1} s: {b[0] - a[0]:.2f} gr/s")
