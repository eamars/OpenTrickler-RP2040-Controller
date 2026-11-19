"""Try sets of the simulation's parameters against the bench's figures and rank them.

Each parameter is named by its part of the rig and its field, with the values to try; every combination is scored
by `fit_report.evaluate`: the mean of each figure's squared miss, counted in tolerances and capped at three.

Usage:
    python fit_search.py fine.slump_deg=12,16,20 powder.regularity=1.4,2,3 [--only creep,nudges] [--match steady]
"""

import argparse
import dataclasses
import itertools
import json
from pathlib import Path

import fit_report
import rigs

CAP = 3.0


def with_values(rig, names: list, values: tuple):
    """The rig with each named field set."""
    for name, value in zip(names, values):
        part, field = name.split(".")
        rig = dataclasses.replace(rig, **{part: dataclasses.replace(getattr(rig, part), **{field: value})})
    return rig


def score(rows: list) -> float:
    return sum(min(CAP, abs(got - want) / tolerance) ** 2 for _, _, want, got, tolerance in rows) / len(rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("parameters", nargs="+", help="part.field=value,value,...  (part: fine, coarse, powder, scale)")
    parser.add_argument("--rig", default="bench_pico2w")
    parser.add_argument("--targets", type=Path, default=Path(__file__).parent / "fit" / "bench_pico2w.json")
    parser.add_argument("--only", default="maps,creep,nudges,throws")
    parser.add_argument("--match", default="")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--show", type=int, default=6)
    args = parser.parse_args()
    targets = json.loads(args.targets.read_text(encoding="utf-8"))
    names = [text.split("=")[0] for text in args.parameters]
    choices = [[float(value) for value in text.split("=")[1].split(",")] for text in args.parameters]
    results = []
    for values in itertools.product(*choices):
        rig = with_values(rigs.RIGS[args.rig], names, values)
        rows = fit_report.evaluate(rig, targets, args.only.split(","), args.seed, args.match)
        passed = sum(1 for _, _, want, got, tolerance in rows if abs(got - want) <= tolerance)
        results.append((score(rows), passed, len(rows), values))
        print(f"{dict(zip(names, values))}: score {results[-1][0]:.2f}, {passed} of {len(rows)}", flush=True)
    print("best:")
    for result, passed, judged, values in sorted(results)[:args.show]:
        print(f"  score {result:.2f}, {passed} of {judged}: {dict(zip(names, values))}")


if __name__ == "__main__":
    main()
