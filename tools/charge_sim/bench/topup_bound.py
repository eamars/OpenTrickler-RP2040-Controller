"""What a finish by small moves and waits could reach with a powder, from its recorded nudges alone. No model.

A finish that ends short and adds small moves of the fine tube until the settled reading is in the band is drawn
many times, each move's gain taken at random from the session's watched nudges (what the bench's tube and powder
really released on a move of that size, as the scale showed it after 5 s). It stops as soon as the reading is
within the band's lower edge, so it is the kindest such finish: it knows each move's gain exactly and never moves
once in the band. What is over is the powder's doing, not a controller's.

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
FINISHES = 100000
WAITS_S = (2.0, 4.0)                                # a wait after each move; the A&D shows half of single digits
                                                    # by 2 s and nine in ten by 4 s (progress.md, 2026-11-20)


def nudge_gains(recordings: Path, folder: str) -> list:
    """Each watched nudge's gain in whole digits."""
    gains = []
    for trial in json.loads((recordings / folder / "runs.json").read_text(encoding="utf-8")):
        if trial["note"] == "creepstop":
            gains.append(round((stats.end_reading(trial) - trial["during"][0][1]) / stats.DIGIT_GR))
    return gains


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--session", required=True)
    parser.add_argument("--recordings", type=Path, default=measure.HERE.parent / "recordings")
    parser.add_argument("--folder", help="the watched nudges; the session's manifest names them when not given")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    folder = args.folder or measure.manifest_of(args.session)["recordings"]["nudges"]
    gains = nudge_gains(args.recordings, folder)
    rng = random.Random(args.seed)
    shares = {gain: gains.count(gain) / len(gains) for gain in sorted(set(gains))}
    print(f"{args.session}, {len(gains)} watched nudges ({folder}); a nudge's gain in digits, share of nudges:")
    print("  " + ", ".join(f"{gain:+d}: {share * 100:.1f}%" for gain, share in shares.items()))
    print(f"{'band':>8s}{'ends short':>12s}{'good':>8s}{'over':>8s}{'moves, mean':>13s}{'95%':>6s}"
          + "".join(f"{f'time at {wait:g} s':>14s}" for wait in WAITS_S))
    for band, label in BANDS_DIGITS:
        for short in SHORTS_DIGITS:
            over, moves = 0, []
            for _ in range(FINISHES):
                error, count = -short, 0
                while error < -band:
                    error += rng.choice(gains)
                    count += 1
                over += error > band
                moves.append(count)
            moves.sort()
            mean = sum(moves) / FINISHES
            print(f"{label:>8s}{short:>10d} d{(1 - over / FINISHES) * 100:7.1f}%{over / FINISHES * 100:7.1f}%{mean:13.1f}"
                  f"{moves[int(0.95 * FINISHES)]:6d}" + "".join(f"{mean * wait:12.0f} s" for wait in WAITS_S))


if __name__ == "__main__":
    main()
