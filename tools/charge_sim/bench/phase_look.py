"""Does what a move pours depend on where in its turn the tube is?

The tube's angle is followed through a whole recording of fixed moves by adding up every move's true turn (and
the creep of the hold after it). Each move's powder is set against the mean of its own kind, in that kind's
spreads, and the moves are sorted by the angle at the middle of the move. A flow that follows the tube's angle
once a turn shows as a wave over the eight parts of a turn; its size is given beside the size chance alone gives
(the same moves with their angles shuffled).

Usage:
    python phase_look.py <logs/adr009/..._ramp> [--most-turn 0.3]
"""

import argparse
import json
import math
import random
import statistics
from pathlib import Path

DIGIT_GR = 0.02
HOLD_RPS = 0.0005       # record.py's hold after a stop
PARTS = 8
SHUFFLES = 500


def wave(points: list) -> float:
    """The size of the once-a-turn wave in (angle, value) points: least squares on a cosine and a sine."""
    count = len(points)
    cos = sum(value * math.cos(2.0 * math.pi * angle) for angle, value in points) * 2.0 / count
    sin = sum(value * math.sin(2.0 * math.pi * angle) for angle, value in points) * 2.0 / count
    return math.hypot(cos, sin)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", type=Path)
    parser.add_argument("--most-turn", type=float, default=0.3, help="only moves up to this much of a turn")
    args = parser.parse_args()
    runs = json.loads((args.folder / "runs.json").read_text(encoding="utf-8"))
    angle, moves = 0.0, []
    for run in runs:
        if run["note"] != "ramp":
            angle += run.get("turns", 0.0)
            continue
        end = statistics.median(w for t, w in run["after"] if t >= run["after"][-1][0] - 0.6)
        poured = (end - run["during"][0][1]) / DIGIT_GR
        moves.append((run["name"], run["turns"], (angle + run["turns"] / 2.0) % 1.0, poured))
        angle += run["turns"] + HOLD_RPS * run["after"][-1][0]
    kinds = {}
    for name, turns, _, poured in moves:
        kinds.setdefault(name, []).append((turns, poured))
    points = []
    for name, turns, middle, poured in moves:
        mine = [value for _, value in kinds[name]]
        if statistics.fmean(t for t, _ in kinds[name]) > args.most_turn or statistics.pstdev(mine) <= 0.0:
            continue
        points.append((middle, (poured - statistics.fmean(mine)) / statistics.pstdev(mine)))
    size = wave(points)
    draw = random.Random(1)
    values = [value for _, value in points]
    chance = []
    for _ in range(SHUFFLES):
        draw.shuffle(values)
        chance.append(wave([(a, v) for (a, _), v in zip(points, values)]))
    chance.sort()
    print(f"{args.folder.name}: {len(points)} moves of up to {args.most_turn:g} turn, the tube turned {angle:.1f} turns in all")
    print("  mean of the moves in each eighth of a turn, in spreads of their own kind: "
          + "  ".join(f"{statistics.fmean([v for a, v in points if int(a * PARTS) == part] or [0.0]):+.2f}" for part in range(PARTS)))
    print(f"  once-a-turn wave: {size:.2f} spreads; chance alone gives {statistics.median(chance):.2f} (19 in 20 under "
          f"{chance[int(0.95 * (SHUFFLES - 1))]:.2f}); larger than chance in {sum(c < size for c in chance) / SHUFFLES:.0%} of shuffles")


if __name__ == "__main__":
    main()
