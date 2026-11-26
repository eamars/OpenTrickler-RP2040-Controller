"""The side by side of 2026-11-22 evening, summed up: the glide and the PID with learning, ABBA in blocks of 10.

ADR-009, research/pid_learning_2026-11-22.md. For each block and each controller: good, over and under at the band by
the weight at rest and by the settled weight, the median time, the first rests' mean and spread, the throws topped
up, and the fine tube's speed when it first stopped (from the rows, `@r <ms> <reading> <forecast> <coarse> <fine>`).

    python abba_look.py <series folder> ...      in the order thrown, each with its controller in series.json
"""

import json
import statistics
import sys
from pathlib import Path


def stop_speed(rows: list) -> float:
    """The fine tube's last speed above zero before it first stood still after pouring."""
    last = None
    for row in rows:
        fine = row[4]
        if fine > 0.0:
            last = fine
        elif last is not None:
            return last
    return float("nan")


def summary(throws: list, target: float, band: float) -> str:
    within = band + 1e-6
    rest = [t["rest"] - target for t in throws]
    settled = [t["settled"] - target for t in throws if t.get("settled") is not None]
    firsts = [t["first"] for t in throws if isinstance(t.get("first"), float)]
    good = sum(abs(e) <= within for e in rest)
    over = sum(e > within for e in rest)
    sgood = sum(abs(e) <= within for e in settled)
    sover = sum(e > within for e in settled)
    topped = sum(1 for t in throws if t["moves"] > 0 or t["streams"] > 0)
    speeds = [stop_speed(t["rows"]) for t in throws if t.get("rows")]
    return (f"{len(throws):3d} throws  rest good {good:2d} over {over:2d}  settled good {sgood:2d} over {sover:2d}  "
            f"median {statistics.median(t['elapsed'] for t in throws):5.1f} s  first mean {statistics.fmean(firsts):+.3f} "
            f"sd {statistics.stdev(firsts):.3f}  topped {topped:2d}  fine at stop {statistics.median(speeds):.3f} rev/s")


def main() -> None:
    by_controller = {}
    for folder in sys.argv[1:]:
        record = json.loads((Path(folder) / "series.json").read_text())
        controller = record.get("controller") or "staged"
        throws = record["throws"]
        by_controller.setdefault(controller, []).extend(throws)
        print(f"{Path(folder).name:42s} {controller:10s} {summary(throws, record['target_gr'], record['band_gr'])}")
        target, band = record["target_gr"], record["band_gr"]
    for controller, throws in by_controller.items():
        print(f"{'all':42s} {controller:10s} {summary(throws, target, band)}")


if __name__ == "__main__":
    main()
