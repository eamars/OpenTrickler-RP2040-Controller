"""A rig's record from a bench session: what was measured becomes the simulation's numbers, by one command.

No number in a record is set by hand. Each is one of:
- measured: read straight off a figure (a tube's grains a turn, the scale's rhythm, what a reverse turn spills);
- solved: the one value at which the simulation, run through the bench's own test, gives the bench's figure (the
  scale's delay from what shows after a stop; a tube's bursts at a speed from how unevenly it poured there);
- stated: known of the rig without a recording (the scale's digit, the kernel, the fall to the cup);
- assumed: no recording gives it yet. `pinned.py` says which of these matter.

The throws of today's loop are never used here: they are the check (`fit_report.py`).

Usage:
    python build_rig.py --figures fit/bench_pico2w.json
    python build_rig.py --logs <checkout>/logs/adr009 --manifest fit/bench_pico2w.manifest.json
"""

import argparse
import copy
import json
import os
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path

import fit_report
import rigs
from workers import Workers

sys.path.insert(0, str(Path(__file__).parent / "bench"))

import measure     # noqa: E402  (bench/measure.py)

HERE = Path(__file__).parent
SEEDS = (101, 202)          # every value tried is scored on both
ROUNDS = 3                  # a grid, then two closer looks round its best value
GRID = 7
CLOSER = 5
CAP = 3.0                   # a figure's miss counts for no more than this many tolerances
AS_GOOD = (1.5, 0.1)        # a score within this ratio and this much of the best explains the figures as well:
                            # two scores of one value on other seeds differ by about as much
FLAT_SHARE = 0.10           # a flow map whose points all lie this near its median is one number

# What no recording gives yet. Each is named "assumed" in the record's sources.
ASSUMED_SCALE = {"report_jitter_s": 0.002, "sensor_tau_s": 0.03, "open_tau_s": 0.10, "closed_tau_s": 1.30,
                 "link_min_s": 0.009, "link_max_s": 0.029}
OPEN_ABOVE_DIGITS = 2.5     # the scale follows a change over this many digits at once: the bench's A&D first showed
                            # a large release as a jump of 2 or 3 digits (the tapped frames of 2026-11-19)
NOISE_DIGITS = 0.075
ASSUMED_POWDER = {"kernel_cv": 0.12}
ASSUMED_TUBE = {"regularity": 2.0, "settle_s": 0.0, "after_stop_tau_s": 0.2, "after_stop_full_turns": 0.1,
                "ripple_turn": 0.0, "ripple_rifling": 0.0, "reverse_spill_gr": 0.0, "reverse_spill_cv": 0.3,
                "reverse_spill_s": 0.25, "start_turns": 0.2, "retreat_turns": 0.5}
FANO = ("fano_1s", "fano_2s", "fano_4s")
VERBOSE = bool(os.environ.get("BUILD_RIG_VERBOSE"))     # print every value tried with its miss


@dataclass(frozen=True)
class Problem:
    """One number to solve: where it lies in the record, the range to look in, and the figures that give it.

    path: (part, field) or (part, table, speed) in the record.
    low, high: the range; log: whether to space the grid by ratio.
    only: the kinds of test to run (fit_report's). match: the tests' titles. names: the figures judged.
    least: the number is an effect that may not be there (a ripple, kernels after a stop, an evenness of the
        releases). The least value that explains the figures about as well as the best is taken, so that no
        effect is claimed from noise.
    """

    path: tuple
    low: float
    high: float
    log: bool
    only: str
    match: str
    names: tuple
    least: bool = False

    @property
    def label(self) -> str:
        return ".".join(f"{part:g}" if isinstance(part, float) else part for part in self.path)


def spaced(low: float, high: float, count: int, log: bool) -> list:
    """Values from low to high, evenly or by ratio."""
    if count < 2 or high <= low:
        return [low]
    if log:
        return [low * (high / low) ** (index / (count - 1)) for index in range(count)]
    return [low + (high - low) * index / (count - 1) for index in range(count)]


def with_value(record: dict, path: tuple, value: float) -> dict:
    """A copy of the record with one number set: a field, or a table's entry at a speed."""
    out = copy.deepcopy(record)
    if len(path) == 2:
        out[path[0]][path[1]] = value
        return out
    for row in out[path[0]][path[1]]:
        if row[0] == path[2]:
            row[1] = value
    return out


def score_job(job: tuple) -> float:
    """
    How far the simulation on a record is from the bench, on the figures one problem is solved by.

    Args:
        job: (record, the bench's figures, the kinds of test, the titles to match, the figures' names).

    Returns:
        the mean of each figure's squared miss, in tolerances, capped, over both seeds.
    """
    record, targets, only, match, names = job
    rig = rigs.rig_from_record(record)
    misses = []
    for seed in SEEDS:
        for _, name, want, got, tolerance in fit_report.evaluate(rig, targets, only.split(","), seed, match, True):
            if name in names:
                # A figure the simulation could not make at all (no single kernel seen to land) is a full miss
                misses.append(CAP ** 2 if got is None else min(CAP, abs(got - want) / tolerance) ** 2)
    if not misses:
        raise ValueError(f"no figure of {names} in the tests matching {match!r}")
    return statistics.fmean(misses)


def best_value(scored: dict, least: bool) -> float:
    """The value with the lowest score; for an effect that may not be there, the least value about as good."""
    lowest = min(scored.values())
    if least:
        return min(value for value, score in scored.items() if score <= lowest * AS_GOOD[0] + AS_GOOD[1])
    return min(scored, key=scored.get)


def solve(record: dict, problems: list, targets: dict, pool, sources: dict) -> dict:
    """
    Solve problems that do not lean on one another, side by side.

    Each is tried on a grid, then twice more closely between the neighbours of its best value; every other
    number stays as the record has it.

    Args:
        record: the record so far.
        problems: what to solve.
        targets: the bench's figures.
        pool: the workers.
        sources: each solved number's source is written here.

    Returns:
        the record with every problem's best value set.
    """
    scored = [{} for _ in problems]
    grids = [spaced(problem.low, problem.high, GRID, problem.log) for problem in problems]
    for _ in range(ROUNDS):
        asked = [(index, value) for index, grid in enumerate(grids) for value in grid if value not in scored[index]]
        jobs = [(with_value(record, problems[index].path, value), targets, problems[index].only,
                 problems[index].match, problems[index].names) for index, value in asked]
        for (index, value), result in zip(asked, pool.map(score_job, jobs)):
            scored[index][value] = result
        for index, problem in enumerate(problems):
            tried = sorted(scored[index])
            best = tried.index(best_value(scored[index], problem.least))
            grids[index] = spaced(tried[max(best - 1, 0)], tried[min(best + 1, len(tried) - 1)], CLOSER, problem.log)
    for index, problem in enumerate(problems):
        best = best_value(scored[index], problem.least)
        record = with_value(record, problem.path, round(best, 4))
        sources[problem.label] = {"how": "solved", "from": f"{problem.match.strip()}: {', '.join(problem.names)}",
                                  "miss_in_tolerances": round(scored[index][best] ** 0.5, 2)}
        print(f"  {problem.label} = {best:.4g}  (miss {scored[index][best] ** 0.5:.2f} tolerances; "
              f"looked from {problem.low:g} to {problem.high:g})", flush=True)
        if VERBOSE:
            print("      " + "  ".join(f"{value:.3g}:{score ** 0.5:.2f}"
                                       for value, score in sorted(scored[index].items())), flush=True)
    return record


def creep_rows(targets: dict, tube: str) -> dict:
    """A tube's creep-stop figures by speed: {speed: [(title, figures), ...]}."""
    out = {}
    for title, row in targets["creep"].items():
        if row["tube"] == tube:
            out.setdefault(row["speed_rps"], []).append((title, row))
    return out


def flow_map(runs: list) -> list:
    """A tube's flow map from its map runs: one point when every speed lies near the median, else a point a speed."""
    by_speed = {}
    for run in runs:
        by_speed.setdefault(run["speed_rps"], []).append(run["gr_per_turn"])
    points = sorted((speed, statistics.median(values)) for speed, values in by_speed.items())
    median = statistics.median(value for _, value in points)
    if all(abs(value - median) <= FLAT_SHARE * median for _, value in points):
        return [[1.0, round(median, 4)]]
    return [[speed, round(value, 4)] for speed, value in points]


def first_record(targets: dict, sources: dict) -> dict:
    """The record before anything is solved: what is measured, stated and assumed, and tables at a first guess."""
    stated = targets["stated"]
    digit = stated["scale"]["digit_gr"]
    scale = {**ASSUMED_SCALE, "open_above_gr": OPEN_ABOVE_DIGITS * digit, "noise_gr": NOISE_DIGITS * digit,
             "dead_s": 0.3, "hold_s": 0.0, "report_s": round(targets["scale"]["report_s"], 5), **stated["scale"]}
    for name in (*ASSUMED_SCALE, "open_above_gr", "noise_gr"):
        sources[f"scale.{name}"] = {"how": "assumed"}
    sources["scale.report_s"] = {"how": "measured", "from": "the scale's frames"}
    record = {"name": targets["name"], "what": targets["rig"], "scale": scale,
              "powder": {**ASSUMED_POWDER, **stated["powder"]}}
    sources["powder.kernel_cv"] = {"how": "assumed"}
    for tube in ("coarse", "fine"):
        by_speed = creep_rows(targets, tube)
        stops = sorted(speed for speed, rows in by_speed.items() if any(row["length"] == "stop" for _, row in rows))
        fields = {**ASSUMED_TUBE, **stated[tube], "gr_per_turn": flow_map(targets["maps"][tube]["runs"]),
                  "burst_kernels": [[speed, 1.5] for speed in sorted(by_speed)] or [[1.0, 1.0]],
                  "after_stop_kernels": [[speed, 0.0] for speed in stops] or [[1.0, 0.0]]}
        for name in ASSUMED_TUBE:
            sources[f"{tube}.{name}"] = {"how": "assumed"}
        sources[f"{tube}.gr_per_turn"] = {"how": "measured", "from": f"map {tube}"}
        spills = []
        for speed, rows in by_speed.items():
            kinds = {row["stop"]: row["rise"] for _, row in rows if row["length"] == "stop"}
            if "backoff" in kinds and "held" in kinds:
                spills.append((kinds["backoff"] - kinds["held"]) * digit)
        if spills:
            fields["reverse_spill_gr"] = round(statistics.fmean(spills), 3)
            sources[f"{tube}.reverse_spill_gr"] = {"how": "measured",
                                                   "from": f"{tube} stops: a reverse turn's rise less a plain stop's"}
        record[tube] = fields
    record["motors"] = stated["motors"]
    for part, fields in stated.items():
        for name in fields:
            if name != "name":
                sources[f"{part}.{name}"] = {"how": "stated"}
    return record


def problems_of(targets: dict, first: bool) -> list:
    """
    The numbers to solve, in the order they lean on one another.

    Args:
        targets: the bench's figures.
        first: the first pass, which alone solves what is solved once.

    Returns:
        batches of problems; those in one batch are solved side by side.
    """
    bursts, after_stops, ripples = [], [], []
    for tube in ("coarse", "fine"):
        for speed, rows in sorted(creep_rows(targets, tube).items()):
            prefix = f"{tube} {speed:g} "
            if any(row["fano_1s"] is not None for _, row in rows):
                bursts.append(Problem((tube, "burst_kernels", speed), 1.0, 12.0, True, "creep", prefix, FANO))
            elif any(row["stop"] == "held" for _, row in rows):
                # No run long enough to show how evenly it pours: the spread of what lands after its stops stands in
                bursts.append(Problem((tube, "burst_kernels", speed), 1.0, 60.0, True, "creep", prefix + "held",
                                      ("rise_sd",)))
            plain = [row for _, row in rows if row["length"] == "stop" and row["stop"] != "backoff"]
            if plain:
                most = max(4.0, 1.5 * max(row["rise"] for row in plain))
                match = ",".join(f"{prefix}{kind} stop" for kind in ("held", "released"))
                after_stops.append(Problem((tube, "after_stop_kernels", speed), 0.0, most, False, "creep", match,
                                           ("rise",), True))
        steady = ",".join(title for speed, rows in creep_rows(targets, tube).items() for title, row in rows
                          if row.get("fold_turn") is not None)
        if steady:
            ripples.append(Problem((tube, "ripple_turn"), 0.0, 0.9, False, "creep", steady, ("fold_turn",), True))
            ripples.append(Problem((tube, "ripple_rifling"), 0.0, 0.6, False, "creep", steady, ("fold_rifling",),
                                   True))
    fine_stops = ",".join(title for title, row in targets["creep"].items()
                          if row["tube"] == "fine" and row["length"] == "stop")
    fine_steady = ",".join(title for title, row in targets["creep"].items()
                           if row["tube"] == "fine" and row["fano_1s"] is not None)
    # The scale's delay is what shows after a stop from a fast flow, where a few kernels more or less do not
    # count: solved first and once, with nothing falling after a stop, so the two are never traded for each other.
    once = [[Problem(("scale", "dead_s"), 0.05, 1.0, False, "maps", "map", ("late_s_fast",))]]
    return (once if first else []) + [
        [Problem(("scale", "hold_s"), 0.0, 3.0, False, "nudges", "nudges", ("first_single_s",))],
        bursts + ripples,
        after_stops,
        [Problem(("fine", "settle_s"), 0.0, 6.0, False, "creep,nudges", f"nudges,{fine_stops}",
                 ("fano", "nothing", "three_up", "kernels", "fano_1s"), True)],
        [Problem(("fine", "regularity"), 1.0, 8.0, True, "creep", fine_steady, FANO, True)],
    ]


def build(targets: dict, passes: int) -> dict:
    """
    A record from a session's figures.

    Args:
        targets: the bench's figures (`bench/measure.py`).
        passes: how many times the solved numbers are gone through; each leans a little on the others.

    Returns:
        the record, with every number's source.
    """
    sources = {}
    record = first_record(targets, sources)
    with Workers() as pool:
        for index in range(passes):
            print(f"pass {index + 1} of {passes}", flush=True)
            for batch in problems_of(targets, index == 0):
                if batch:
                    record = solve(record, batch, targets, pool, sources)
    record["sources"] = dict(sorted(sources.items()))
    return record


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--figures", type=Path, help="a session's figures, as bench/measure.py writes them")
    parser.add_argument("--logs", type=Path, help="the session's raw folders: measured first, with --manifest")
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--out", type=Path, help="records/<name>.json when not given")
    parser.add_argument("--passes", type=int, default=2)
    args = parser.parse_args()
    if args.logs:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        targets = measure.session_figures(args.logs, manifest)
        figures_path = HERE / "fit" / f"{manifest['name']}.json"
        figures_path.write_text(json.dumps(targets, indent=1), encoding="utf-8")
        print(f"wrote {figures_path}")
    else:
        targets = json.loads(args.figures.read_text(encoding="utf-8"))
    record = build(targets, args.passes)
    out = args.out or rigs.RECORDS / f"{targets['name']}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(record, indent=1), encoding="utf-8")
    print(f"wrote {out}; check it with: python fit_report.py --rig {out.stem}")


if __name__ == "__main__":
    main()
