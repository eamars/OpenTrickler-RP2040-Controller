"""Where a throw's time goes, by phase, from the series' rows (ADR-009, 2026-11-22 night).

For each throw: the bulk (start to the reading first within `--near` grains of the target), the approach (from there
to the fine tube's first stop), and the rest and top-ups (from that stop to the verdict). Medians by controller.

    python phase_times.py <series folder> ... [--near 0.7]
"""

import argparse
import json
import statistics
from pathlib import Path


def phases(throw: dict, target: float, near: float):
    rows = throw["rows"]
    bulk = next((r[0] for r in rows if r[1] >= target - near), None)
    poured = False
    stop = None
    for r in rows:
        if r[4] > 0.0:
            poured = True
        elif poured and r[3] <= 0.0:
            stop = r[0]
            break
    if bulk is None or stop is None:
        return None
    end = throw["elapsed"] * 1000.0
    return bulk / 1000.0, (stop - bulk) / 1000.0, (end - stop) / 1000.0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    parser.add_argument("--near", type=float, default=0.7)
    args = parser.parse_args()
    by = {}
    for folder in args.folders:
        record = json.loads((folder / "series.json").read_text())
        controller = record.get("controller") or "staged"
        for throw in record["throws"]:
            p = phases(throw, record["target_gr"], args.near)
            if p:
                by.setdefault(controller, []).append(p)
    for controller, rows in by.items():
        med = [statistics.median(r[i] for r in rows) for i in range(3)]
        print(f"{controller:10s} {len(rows):3d} throws  bulk to within {args.near} gr {med[0]:5.2f} s  approach to the "
              f"fine stop {med[1]:5.2f} s  rest and top-ups {med[2]:5.2f} s  (medians)")


if __name__ == "__main__":
    main()
