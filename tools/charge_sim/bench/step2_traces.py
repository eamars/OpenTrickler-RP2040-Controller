"""ADR-009 step 2 on the bench: each real throw's scale trace from the tapped frames (`scale tap on`), drawn by kp.

Reads rc_run.py's session logs of charges thrown with the scale tapped: each throw's start (`Charge: throw N on fine
kp K, ...`), its verdict (`Charge: throw N at rest, W of T in S s`) and the A&D's frames (`@sf <unit ms> <weight>
<raw>`). Draws each throw's reading from its start, its stop marked, one panel by kp; a throw that took more than the
median of all is drawn dashed. Times are the PC's, as each line was read from the console. `--json` keeps the throws
(logs/ is not tracked): each one's kp, the weight a turn it started on, its verdict and time, and its trace.

    venv\\Scripts\\python tools\\charge_sim\\bench\\step2_traces.py --out <png> [--json <file>] logs\\rc\\<session>.log ...
"""

import argparse
import json
import re
import statistics
from datetime import datetime
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

START = re.compile(r"^(\S+) unit +Charge: throw (\d+) on fine kp ([\d.]+)(?: kd [\d.]+)?, weight a turn "
                   r"(carried|its own, none yet) ([\d.]+)")
CARRIED = re.compile(r"Charge: carried weight a turn ([\d.]+)")
AT_REST = re.compile(r"^(\S+) unit +Charge: throw (\d+) at rest, (-?[\d.]+) of ([\d.]+) in ([\d.]+) s")
FRAME = re.compile(r"^(\S+) unit +@sf \d+ (-?[\d.]+) ")
AFTER_S = 3.0


def seconds(stamp: str) -> float:
    return (datetime.strptime(stamp, "%H:%M:%S.%f") - datetime(1900, 1, 1)).total_seconds()


def throws(paths: list) -> list:
    """(kp, the trace as (s from the start, reading), the stop's s, the final weight, the target, what else is known
    as a dict) for each throw."""
    out = []
    for path in paths:
        lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
        frames = [(seconds(m[1]), float(m[2])) for m in map(FRAME.search, lines) if m]
        start = None
        for text in lines:
            if m := START.search(text):
                start = (seconds(m[1]), float(m[3]), float(m[5]) if m[4] == "carried" else None, int(m[2]))
            elif (m := AT_REST.search(text)) and start:
                stop = float(m[5])
                trace = [(t - start[0], w) for t, w in frames if start[0] - 0.5 <= t <= start[0] + stop + AFTER_S]
                out.append((start[1], trace, stop, float(m[3]), float(m[4]),
                            {"session": Path(path).name, "throw": start[3], "carried_before": start[2]}))
                start = None
            elif (m := CARRIED.search(text)) and out and out[-1][5]["session"] == Path(path).name:
                out[-1][5]["carried_after"] = float(m[1])
    return out


def draw(panels: list, out: str, title: str, rows: int = 1, ylabel: str = "the A&D's reading, gr") -> None:
    """Panels of throws, each (its name, [(trace as (s, reading), the stop's s, the final weight, the target), ...]):
    each throw's reading from its start, its stop a dot; one slower than the median of all is dashed."""
    median_stop = statistics.median(stop for _, throws_drawn in panels for _, stop, _, _ in throws_drawn)
    columns = -(-len(panels) // rows)
    fig, axes = plt.subplots(rows, columns, figsize=(7 * columns, 5 * rows), sharey=True, squeeze=False)
    for ax, (name, throws_drawn) in zip(axes.flat, panels):
        for trace, stop, final, target in throws_drawn:
            style = "--" if stop > median_stop else "-"
            ax.plot([t for t, _ in trace], [w for _, w in trace], style, linewidth=0.8)
            ax.plot([stop], [next((w for t, w in reversed(trace) if t <= stop), float("nan"))], "k.", markersize=4)
        target = throws_drawn[0][3]
        ax.axhline(target, color="grey", linewidth=0.6)
        ax.set_ylim(target - 3.0, target + 0.8)
        errors = [final - target for _, _, final, target in throws_drawn]
        ax.set_title(f"{name}: {len(throws_drawn)} throws, mean {statistics.fmean(errors):+.3f}, "
                     f"sd {statistics.pstdev(errors):.3f} gr", fontsize=10)
        ax.set_xlabel("s from the throw's start (dot: the stop)")
    for row in axes:
        row[0].set_ylabel(ylabel)
    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("logs", nargs="+")
    parser.add_argument("--out", required=True)
    parser.add_argument("--json", help="keep the throws here")
    args = parser.parse_args()
    found = throws(args.logs)
    kps = sorted({kp for kp, *_ in found}, reverse=True)
    panels = [(f"fine kp {kp:g}", [(trace, stop, final, target) for k, trace, stop, final, target, _ in found
                                   if k == kp]) for kp in kps]
    draw(panels, args.out, "ADR-009 step 2 on the Pico 2 W: real 10 gr throws, AR2209 (dashed: slower than the median)")
    print(f"{len(found)} throws drawn into {args.out}")
    if args.json:
        kept = [dict(about, kp=kp, target=target, final=final, stop_s=stop,
                     trace=[[round(t, 3), w] for t, w in trace]) for kp, trace, stop, final, target, about in found]
        Path(args.json).write_text(json.dumps(kept, indent=0), encoding="utf-8")
        print(f"kept in {args.json}")


if __name__ == "__main__":
    main()
