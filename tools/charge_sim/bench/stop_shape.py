"""Print the reading after each run's stop, against the settled end: how the scale catches up."""
import json, sys
from pathlib import Path
folder = Path(sys.argv[1])
frames = []
for line in open(folder / "frames.csv", encoding="utf-8").read().splitlines()[1:]:
    t, w, h = line.split(",")
    frames.append((float(t), float(w), h))
for run in json.loads((folder / "runs.json").read_text(encoding="utf-8")):
    if run["note"] == "precondition":
        continue
    stop = run["t_stop_asked_ms"]
    end = [w for t, w, h in frames if run["t_after_ms"] - 700 <= t <= run["t_after_ms"]]
    final = sum(end) / len(end)
    out = []
    for offset in (-200, 0, 100, 200, 300, 400, 500, 600, 800, 1000, 1500, 2000):
        near = [(abs(t - (stop + offset)), w, h) for t, w, h in frames if abs(t - (stop + offset)) < 40]
        if near:
            _, w, h = min(near)
            out.append(f"{offset}:{w - final:+.2f}{'' if h == 'ST' else '*'}")
    print(run["tube"], run["speed_asked_rps"], "final", round(final, 2), " ".join(out))
