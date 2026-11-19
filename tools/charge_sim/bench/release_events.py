"""A steady run's releases one by one: the sizes of the reading's rises, the gaps between them, the flow by angle.

Digits that arrive within 0.35 s of each other are counted as one rise: the scale spreads a release over about
that long, so a rise here is a release or a few close together. For the long runs `record.py creepstop` makes.

Usage:
    python release_events.py <logs/adr009/..._creepstop> [<another> ...]
"""

import json
import statistics
import sys
from pathlib import Path

import stats

JOIN_S = 0.35


def events(trial: dict) -> list:
    """The reading's rises during a trial's turn as (start, end, digits)."""
    out = []
    last = trial["during"][0][1]
    for t, weight in trial["during"]:
        if abs(weight - last) < stats.DIGIT_GR / 2.0:
            continue
        digits = round((weight - last) / stats.DIGIT_GR)
        last = weight
        if out and t - out[-1][1] < JOIN_S:
            out[-1] = (out[-1][0], t, out[-1][2] + digits)
        else:
            out.append((t, t, digits))
    return out


def main() -> None:
    for name in sys.argv[1:]:
        for trial in json.loads((Path(name) / "runs.json").read_text(encoding="utf-8")):
            if trial["note"] != "creepstop" or trial["run_s"] < 30.0:
                continue
            found = [event for event in events(trial) if event[2] > 0]
            sizes = [event[2] for event in found]
            gaps = [later[0] - earlier[1] for earlier, later in zip(found, found[1:])]
            counts = {size: sizes.count(size) for size in sorted(set(sizes))}
            print(f"{trial.get('tube', 'fine')} {trial['speed_asked_rps']:g} rev/s, {trial['run_s']:.0f} s: "
                  f"{len(sizes)} rises, digits each {counts}, mean {statistics.fmean(sizes):.2f}; between rises "
                  f"median {statistics.median(gaps):.2f} s, longest {max(gaps):.1f} s")
            tenth_s = 0.1 / trial["speed_asked_rps"]
            per, start = [], -trial["run_s"] + stats.START_SKIP_S
            while start + tenth_s <= 0.0:
                per.append(round((stats.reading_at(trial["during"], start + tenth_s)
                                  - stats.reading_at(trial["during"], start)) / stats.DIGIT_GR))
                start += tenth_s
            print(f"  digits a tenth of a turn: {per}")


if __name__ == "__main__":
    main()
