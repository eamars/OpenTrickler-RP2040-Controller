"""A series' throws stop by stop, from their recordings (ADR-009, the rig measurement after the review's job 3).

For each throw: its line (result, streams, the fine figure after it), and for each stop its recording marks: the
reading and the controller's forecast at the stop, the reading at rest after it (just before the tube turns again, or
the verdict), and so what the forecast held that did not come (the rest less the forecast). For each stream: the turns
the asked speed makes (the speed integrated over the rows: the unit's own count is not in the rows) and what it
poured, from the rest before it to the rest after it, so the grains a turn it poured. Then the reading a second and
more after the verdict, where the recording has it, and the next throw's base.

Usage, with the repository's venv:
    python uncal_look.py <series folder>
"""

import json
import sys
from pathlib import Path


def integral(rows, start_ms, end_ms, column, sign=1):
    total = 0.0
    for a, b in zip(rows, rows[1:]):
        if start_ms <= a[0] < end_ms:
            speed = a[column]
            if sign * speed > 0:
                total += abs(speed) * (b[0] - a[0]) / 1000.0
    return total


def main() -> int:
    folder = Path(sys.argv[1])
    record = json.loads((folder / "series.json").read_text())
    target = record["target_gr"]
    for i, throw in enumerate(record["throws"]):
        entry = throw["recording"]["entry"]
        rows = throw["recording"]["samples"]["rows"]
        marks = entry["marks"]
        print(f"throw {i + 1} (recording {entry['id']}): {throw['result']} rest {throw['rest']:.3f}, streams "
              f"{int(throw['streams'])}, {throw['elapsed']:.1f} s; fine figure after {throw['fine_gr']:.3f}; "
              f"coarse turns {throw['coarse_turns']:.3f}, fine {throw['fine_turns']:.3f} (the unit's count); "
              f"settled {throw['settled']:.3f}")
        coarse_fwd = integral(rows, 0, 10 ** 9, 3, 1)
        coarse_back = integral(rows, 0, 10 ** 9, 3, -1)
        print(f"  coarse asked: {coarse_fwd:.3f} turns forward, {coarse_back:.3f} back")
        stops = [m for m in marks if m["kind"] in ("coarse_stop", "fine_stop")]
        verdict = next((m for m in marks if m["kind"] == "verdict"), None)
        rest_before = None
        stream_start = 0
        for j, stop in enumerate(stops):
            at = stop["row"]
            forecast = rows[at][5]
            reading = rows[at][2]
            # the rest: the reading just before a tube turns again after this stop, or at the verdict
            end = len(rows) - 1
            for k in range(at + 1, len(rows)):
                if rows[k][3] > 0 or rows[k][4] > 0:
                    end = k - 1
                    break
            rest = rows[end][2]
            restart_ms = rows[end + 1][0] if end + 1 < len(rows) else None
            fine_t = integral(rows, rows[stream_start][0], stop["t_ms"], 4, 1)
            line = (f"  {stop['kind']} at {stop['t_ms'] / 1000:.2f} s: reading {reading:.2f}, forecast "
                    f"{forecast:.3f} (holds {forecast - reading:+.3f}); at rest {rest:.2f} "
                    f"({rest - target:+.2f} on the target), rest less forecast {rest - forecast:+.3f}; "
                    f"fine turns asked in the stream {fine_t:.3f}")
            if stop["kind"] == "fine_stop" and rest_before is not None and fine_t > 0:
                line += f", poured {rest - rest_before:.2f}: {(rest - rest_before) / fine_t:.3f} gr a turn"
            print(line)
            rest_before = rest
            stream_start = end + 1 if end + 1 < len(rows) else end
        if verdict:
            print(f"  verdict at {verdict['t_ms'] / 1000:.2f} s: {verdict['weight']:.2f}; the recording ends at "
                  f"{rows[-1][0] / 1000:.2f} s reading {rows[-1][2]:.2f}; the next throw's base less this one's "
                  f"{throw['settled']:.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
