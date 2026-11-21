"""What a finish by small moves and waits could reach with a powder, from its recorded nudges alone. No model.

A finish that ends short and adds small moves of the fine tube until the reading is in the band is drawn many
times, each move taken at random from the session's watched nudges: what the bench's tube and powder really
released on a move of that size, and how the scale really showed it over the 5 s after.

Two tables:
- the kindest finish: it knows each move's whole gain (the reading 5 s after) and never moves once in the band.
  What is over is the powder's doing, not a controller's or the scale's;
- the same finish deciding after a wait: it sees of every move so far only what the scale had shown of it by
  then, moves again when that is short of the band, and stops when it is in. What the scale shows late is then
  either moved for twice, or arrives after the finish has stopped. The weight judged is the true one.

Usage:
    python topup_bound.py --session pico2w_ar2209 [--folder <the session's nudgewatch folder>]
"""

import argparse
import json
import random
from pathlib import Path

import measure
import stats

BANDS_DIGITS = ((1, "0.02 gr"), (2, "0.04 gr"))     # a throw is good this many digits either side of its target
SHORTS_DIGITS = (3, 6, 10)                          # how far short the feed ends
WAIT_SHORT_DIGITS = 3                               # the second table's feed ends this far short
WAITS_S = (0.5, 1.0, 1.5, 2.0, 3.0, 4.0)            # the wait after each move before the reading is taken
STEP_S = 0.25                                       # a nudge's showing is kept at this step
FINISHES = 100000
MOST_MOVES = 200


def watched_nudges(recordings: Path, folder: str) -> list:
    """Each watched nudge as (its whole gain in digits, what the scale had shown of it every STEP_S after it began)."""
    nudges = []
    for trial in json.loads((recordings / folder / "runs.json").read_text(encoding="utf-8")):
        if trial["note"] != "creepstop":
            continue
        base = trial["during"][0][1]
        series = trial["during"] + trial["after"]
        gain = round((stats.end_reading(trial) - base) / stats.DIGIT_GR)
        steps = int((trial["after"][-1][0] + trial["run_s"]) / STEP_S)
        shown = [round((stats.reading_at(series, index * STEP_S - trial["run_s"]) - base) / stats.DIGIT_GR)
                 for index in range(1, steps + 1)]
        nudges.append((gain, shown))
    return nudges


def shown_by(nudge: tuple, seconds: float) -> int:
    """What the scale had shown of a nudge this long after it began: its whole gain past the watch's end."""
    gain, shown = nudge
    index = int(seconds / STEP_S) - 1
    return gain if index >= len(shown) else shown[max(index, 0)]


def kindest(nudges: list, band: int, short: int, rng: random.Random) -> tuple:
    """(share over, the moves of every finish) for the finish that knows each move's whole gain."""
    over, moves = 0, []
    for _ in range(FINISHES):
        error, count = -short, 0
        while error < -band:
            error += rng.choice(nudges)[0]
            count += 1
        over += error > band
        moves.append(count)
    return over / FINISHES, moves


def waiting(nudges: list, band: int, short: int, wait_s: float, rng: random.Random) -> tuple:
    """(share over, share under, the moves of every finish) for the finish that decides wait_s after each move."""
    over, under, moves = 0, 0, []
    for _ in range(FINISHES):
        made = []
        seen = -short
        while seen < -band and len(made) < MOST_MOVES:
            made.append(rng.choice(nudges))
            # every move so far, as far as the scale had shown it at this decision
            seen = -short + sum(shown_by(nudge, (len(made) - index) * wait_s) for index, nudge in enumerate(made))
        error = -short + sum(nudge[0] for nudge in made)
        over += error > band
        under += error < -band
        moves.append(len(made))
    return over / FINISHES, under / FINISHES, moves


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--session", required=True)
    parser.add_argument("--recordings", type=Path, default=measure.HERE.parent / "recordings")
    parser.add_argument("--folder", help="the watched nudges; the session's manifest names them when not given")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    folder = args.folder or measure.manifest_of(args.session)["recordings"]["nudges"]
    nudges = watched_nudges(args.recordings, folder)
    gains = [gain for gain, _ in nudges]
    rng = random.Random(args.seed)
    print(f"{args.session}, {len(nudges)} watched nudges ({folder}); a nudge's gain in digits, share of nudges:")
    print("  " + ", ".join(f"{gain:+d}: {gains.count(gain) / len(gains) * 100:.1f}%" for gain in sorted(set(gains))))
    releases = [nudge for nudge in nudges if nudge[0] > 0]
    print("  of the nudges that gave something, the share the scale had shown in full by "
          + ", ".join(f"{wait:g} s: {sum(shown_by(n, wait) >= n[0] for n in releases) / len(releases) * 100:.0f}%"
                      for wait in WAITS_S))

    print("\nthe kindest finish: each move's whole gain known")
    print(f"{'band':>8s}{'ends short':>12s}{'good':>8s}{'over':>8s}{'moves, mean':>13s}{'95%':>6s}")
    for band, label in BANDS_DIGITS:
        for short in SHORTS_DIGITS:
            over, moves = kindest(nudges, band, short, rng)
            moves.sort()
            print(f"{label:>8s}{short:>10d} d{(1 - over) * 100:7.1f}%{over * 100:7.1f}%{sum(moves) / FINISHES:13.1f}"
                  f"{moves[int(0.95 * FINISHES)]:6d}")

    print(f"\nthe same finish deciding after a wait, from {WAIT_SHORT_DIGITS} digits short")
    print(f"{'band':>8s}{'wait':>8s}{'good':>8s}{'over':>8s}{'under':>8s}{'moves, mean':>13s}{'finish, mean':>14s}{'95%':>8s}")
    for band, label in BANDS_DIGITS:
        for wait in WAITS_S:
            over, under, moves = waiting(nudges, band, WAIT_SHORT_DIGITS, wait, rng)
            moves.sort()
            mean = sum(moves) / FINISHES
            print(f"{label:>8s}{wait:6g} s{(1 - over - under) * 100:7.1f}%{over * 100:7.1f}%{under * 100:7.1f}%{mean:13.1f}"
                  f"{mean * wait:12.0f} s{moves[int(0.95 * FINISHES)] * wait:6.0f} s")


if __name__ == "__main__":
    main()
