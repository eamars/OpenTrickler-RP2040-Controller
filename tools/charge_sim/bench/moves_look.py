"""Fixed moves from rest to rest: what a move of a given turn pours, and how surely, by its speed.

Each trial of a `ramp` recording with a plan of one speed is one move: the tube's true turn (from its steps)
and the digits in the cup once all has landed. By plan: the mean and spread of what it poured, the variance over
mean in kernels (1 is kernels leaving singly and at random), the share of moves that poured nothing, and the
share that landed within a digit of the move's own mean (the most a finish could ask of it).

Usage:
    python moves_look.py <logs/adr009/..._ramp> [...]
"""

import argparse
import json
import statistics
from pathlib import Path

DIGIT_GR = 0.02
KERNEL_GR = 0.021


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    args = parser.parse_args()
    for folder in args.folders:
        runs = [run for run in json.loads((folder / "runs.json").read_text(encoding="utf-8"))
                if run["note"] == "ramp" and len(run["plan"]) == 1]
        groups = {}
        for run in runs:
            end = statistics.median(w for t, w in run["after"] if t >= run["after"][-1][0] - 0.6)
            groups.setdefault((run["tube"], run["plan"][0][0], run["name"]), []).append(
                (run["turns"], (end - run["during"][0][1]) / DIGIT_GR))
        print(f"{folder.name}: {len(runs)} moves")
        print(f"  {'tube':6s} {'rev/s':>6s} {'turn':>6s} {'moves':>6s} {'digits':>7s} {'spread':>7s} {'var/mean':>9s} "
              f"{'nothing':>8s} {'within 1':>9s} {'gr a turn':>10s}")
        for (tube, speed, _), rows in sorted(groups.items(), key=lambda item: (item[0][0], item[0][1], statistics.fmean(x for x, _ in item[1]))):
            turns = statistics.fmean(x for x, _ in rows)
            poured = [y for _, y in rows]
            mean, spread = statistics.fmean(poured), statistics.pstdev(poured)
            kernels = mean * DIGIT_GR / KERNEL_GR
            fano = (spread * DIGIT_GR / KERNEL_GR) ** 2 / kernels if kernels > 0.0 else float("nan")
            nothing = sum(1 for y in poured if y < 0.5) / len(poured)
            near = sum(1 for y in poured if abs(y - round(mean)) <= 1.0) / len(poured)
            print(f"  {tube:6s} {speed:6g} {turns:6.3f} {len(rows):6d} {mean:7.1f} {spread:7.1f} {fano:9.2f} {nothing:8.2f} "
                  f"{near:9.2f} {mean * DIGIT_GR / turns:10.2f}")


if __name__ == "__main__":
    main()
