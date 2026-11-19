"""Does a tube pour in step with its own turning? A steady run's flow folded onto a fraction of a turn.

The reading's gain is put into bins by the tube's angle modulo a period (a fifth of a turn for the tubes' five
rifling starts). A flow that ignores the angle fills the bins evenly; one that follows it does not. The figure
given is the share of the flow in the fuller half of the period (0.5 is even, 1 is all of it), beside the same
for a period that should show nothing.

Usage:
    python fold_by_angle.py <logs/adr009/..._creepstop> [...] [--periods 0.2,1,0.137]
"""

import argparse
import json
from pathlib import Path

import stats

BINS = 10


def fold(trial: dict, period_turns: float) -> list:
    """Digits gained in each of BINS equal parts of the period, summed over the trial's steady part."""
    speed = trial["speed_asked_rps"]
    bin_s = period_turns / BINS / speed
    start = -trial["run_s"] + stats.START_SKIP_S
    sums = [0.0] * BINS
    index = 0
    while start + bin_s <= 0.0:
        gain = stats.reading_at(trial["during"], start + bin_s) - stats.reading_at(trial["during"], start)
        sums[index % BINS] += gain / stats.DIGIT_GR
        start += bin_s
        index += 1
    return sums


def fuller_half(sums: list) -> float:
    """The largest share of the flow that any half of the period (consecutive bins, wrapping) holds."""
    total = sum(sums)
    half = BINS // 2
    return max(sum(sums[(first + offset) % BINS] for offset in range(half)) for first in range(BINS)) / total


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    parser.add_argument("--periods", default="0.2,1,0.137", help="fractions of a turn to fold onto")
    args = parser.parse_args()
    for folder in args.folders:
        for trial in json.loads((folder / "runs.json").read_text(encoding="utf-8")):
            if trial["note"] != "creepstop" or trial["run_s"] < 30.0:
                continue
            turns = trial["speed_asked_rps"] * (trial["run_s"] - stats.START_SKIP_S)
            print(f"{trial.get('tube', 'fine')} {trial['speed_asked_rps']:g} rev/s, {turns:.1f} turns")
            for period in (float(text) for text in args.periods.split(",")):
                sums = fold(trial, period)
                print(f"  period {period:g} turn: fuller half holds {fuller_half(sums):.2f}; digits by tenth of the "
                      f"period {[round(value) for value in sums]}")


if __name__ == "__main__":
    main()
