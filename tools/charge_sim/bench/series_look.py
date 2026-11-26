"""A series of the unit's staged throws (series.py's series.json), a line a throw, from its rows (ADR-009, gate 2).

For each throw: its result and first error; the coarse tube's turns and when it stopped, with the reading and the
forecast then; when the fine tube last stopped, with its forecast against where the throw came to rest. A rest above
its forecast at the fine stop is powder the forecast did not count (late, or a lag too short); below it, powder it
counted that did not come.

Usage, with the repository's venv:
    python series_look.py <the series' folder or its series.json>
"""

import json
import statistics
import sys
from pathlib import Path


def last_stop(rows: list, column: int):
    """The row at which a speed last fell to 0 after running, or None."""
    found = None
    for before, row in zip(rows, rows[1:]):
        if before[column] > 0.0 and row[column] == 0.0:
            found = row
    return found


def main() -> int:
    path = Path(sys.argv[1])
    if path.is_dir():
        path = path / "series.json"
    series = json.loads(path.read_text())
    target = series["target_gr"]
    print(f"{series['name']}: {len(series['throws'])} throws of {target} gr, {series['finish']} at {series['band_gr']}")
    print(" n result  first   coarse  stop_s  read  fcast | fine  stop_s  fcast  rest  rest-fcast settled  s")
    offs = {"high": [], "low": []}
    for i, t in enumerate(series["throws"], 1):
        rows = t.get("rows", [])      # t_ms, reading, forecast, coarse rev/s, fine rev/s
        coarse = last_stop(rows, 3)
        fine = last_stop(rows, 4)
        c_txt = f"{coarse[0] / 1000:6.2f} {coarse[1]:6.2f} {coarse[2]:6.2f}" if coarse else "     -      -      -"
        f_txt = f"{fine[0] / 1000:6.2f} {fine[2]:6.3f}" if fine else "     -      -"
        off = t["rest"] - fine[2] if fine else float("nan")
        mode = "high" if t["coarse_turns"] > 0.8 else "low"
        if fine:
            offs[mode].append(off)
        settled = t.get("settled")
        print(f"{i:2} {t['result']:6} {t.get('first', float('nan')):+.3f}  {t['coarse_turns']:.3f} {c_txt} | "
              f"{t['fine_turns']:.3f} {f_txt} {t['rest']:6.3f}  {off:+.3f}  "
              f"{settled if settled is None else round(settled, 3)!s:>7} {t['elapsed']:5.1f}")
    for mode, values in offs.items():
        if values:
            print(f"coarse {mode} (>0.8 turns: {mode == 'high'}): {len(values)} throws, rest less forecast at the "
                  f"fine stop: median {statistics.median(values):+.3f}, from {min(values):+.3f} to {max(values):+.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
