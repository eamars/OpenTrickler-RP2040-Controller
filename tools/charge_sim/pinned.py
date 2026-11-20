"""Which of a record's numbers the recordings pin down, and which of the rest matter to a throw.

Each number is moved down and up by a half (from zero, to a plain value) and two things are looked at:
- would the bench's own tests notice: how many of the record's figures the simulation then gives differently, by
  more than the figure's tolerance;
- would a throw notice: today's loop at the recorded slow-finish settings, its error, its spread, its share of
  overs and its time.

A number the tests notice is pinned. One they do not notice but a throw does is loose and matters: the next
bench session needs a recording for it. One neither notices is loose and harmless.

Chance is measured first: the record as built, run on other seeds. A figure or a sum of the throws counts as
moved only when it moves by more than its own limit and by more than chance moved it.

Usage:
    python pinned.py [--rig bench_pico2w] [--throws 600] [--out fit/pinned_<rig>.txt]
"""

import argparse
import json
import os
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import build_rig
import fit_report
import mc
import rigs
from legacy_pid import PICO2W_SLOW_FINISH

HERE = Path(__file__).parent
FACTOR = 1.5
FROM_ZERO = {"ripple_turn": 0.3, "ripple_rifling": 0.3, "settle_s": 2.0, "after_stop_kernels": 1.0,
             "reverse_spill_gr": 0.5, "hold_s": 1.0}
SKIP = {"name", "digit_gr", "rifling_starts", "report_jitter_s", "link_min_s", "link_max_s"}
LEAST = {"burst_kernels": 1.0, "regularity": 1.0}
TARGET_GR = 20.0
SEED = 1
CHANCE_SEEDS = (2, 3, 4, 5)
BEYOND_CHANCE = 1.5         # moved: by more than this many times the most chance moved it
# A throw notices: the mean error or its band moves by half a digit, the overs by 8 throws in 100, the time by 0.7 s
NOTICE = {"error_mean": 0.01, "error_band": 0.01, "over_true": 0.08, "time_median": 0.7}


def numbers(record: dict) -> list:
    """Every number of a record worth moving: (path, value)."""
    out = []
    for part in ("scale", "powder", "coarse", "fine"):
        for field, value in record[part].items():
            if field in SKIP:
                continue
            if isinstance(value, list):
                out.extend(((part, field, row[0]), row[1]) for row in value)
            elif isinstance(value, (int, float)):
                out.append(((part, field), value))
    return out


def moves(path: tuple, value: float) -> list:
    """The values a number is tried at: a half down and up, or from zero to a plain value."""
    field = path[1]
    if value == 0.0:
        return [FROM_ZERO[field]] if field in FROM_ZERO else []
    least = LEAST.get(field, 0.0)
    return sorted({max(least, value / FACTOR), value * FACTOR} - {value})


def look(job: tuple) -> tuple:
    """
    The simulation's figures and a run of throws on one record.

    Args:
        job: (record, the bench's figures, seed, throws).

    Returns:
        ({(title, figure): (simulated, tolerance)}, the throws summed up).
    """
    record, targets, seed, throws = job
    rig = rigs.rig_from_record(record)
    rows = fit_report.evaluate(rig, targets, ["maps", "creep", "nudges"], seed)
    figures = {(title, name): (got, tolerance) for title, name, _, got, tolerance in rows}
    results = [result for session in mc.session_jobs(rig, PICO2W_SLOW_FINISH, TARGET_GR, throws, seed)
               for result in mc.run_session(session)]
    return figures, mc.summarise(results)


def compare(base: tuple, moved: tuple, floor: dict) -> tuple:
    """
    What a move changed.

    Args:
        base: the record as built: (figures, throws summed up).
        moved: the same with one number moved.
        floor: the most chance moved each figure, by (title, figure).

    Returns:
        (the figures that moved by more than their tolerance and beyond chance, how far each sum of the throws moved).
    """
    figures, summary = base
    noticed = [f"{title}: {name}" for (title, name), (got, tolerance) in moved[0].items()
               if (title, name) in figures
               and abs(got - figures[(title, name)][0]) > max(tolerance, BEYOND_CHANCE * floor.get((title, name), 0.0))]
    deltas = {name: moved[1][name] - summary[name] for name in NOTICE}
    return noticed, deltas


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="bench_pico2w")
    parser.add_argument("--throws", type=int, default=600)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    record = rigs.load_record(args.rig)
    name = Path(args.rig).stem
    targets = json.loads((HERE / "fit" / f"{name}.json").read_text(encoding="utf-8"))
    sources = record.get("sources", {})

    tried = [(path, value, move) for path, value in numbers(record) for move in moves(path, value)]
    jobs = [(record, targets, seed, args.throws) for seed in (SEED, *CHANCE_SEEDS)]
    jobs += [(build_rig.with_value(record, path, move), targets, SEED, args.throws) for path, _, move in tried]
    with ProcessPoolExecutor(max_workers=os.cpu_count()) as pool:
        looks = list(pool.map(look, jobs))
    base, others = looks[0], looks[1:1 + len(CHANCE_SEEDS)]
    floor = {key: max(abs(other[0][key][0] - got) for other in others if key in other[0])
             for key, (got, _) in base[0].items()}
    chance = {key: max(abs(other[1][key] - base[1][key]) for other in others) for key in NOTICE}
    limits = {key: max(limit, BEYOND_CHANCE * chance[key]) for key, limit in NOTICE.items()}
    over_tolerance = sum(1 for key, (_, tolerance) in base[0].items() if floor[key] > tolerance)

    lines = [f"rig {name}; throws: today's loop at the slow-finish settings, {args.throws} at {TARGET_GR:g} gr; "
             f"as built: error {base[1]['error_mean']:+.3f} gr, band {base[1]['error_band']:.3f}, "
             f"over {base[1]['over_true'] * 100:.0f}%, {base[1]['time_median']:.1f} s",
             f"chance alone ({len(CHANCE_SEEDS)} other seeds, the most each moved): {over_tolerance} of "
             f"{len(base[0])} figures past their tolerance; throws: error {chance['error_mean']:.3f}, "
             f"band {chance['error_band']:.3f}, over {chance['over_true'] * 100:.0f}%, "
             f"time {chance['time_median']:.1f} s",
             f"a throw notices from: error {limits['error_mean']:.3f} gr, band {limits['error_band']:.3f} gr, "
             f"over {limits['over_true'] * 100:.0f}%, time {limits['time_median']:.1f} s", ""]
    by_number = {}
    for (path, value, move), moved in zip(tried, looks[1 + len(CHANCE_SEEDS):]):
        noticed, deltas = compare(base, moved, floor)
        entry = by_number.setdefault(path, {"value": value, "noticed": set(), "deltas": dict.fromkeys(NOTICE, 0.0)})
        entry["noticed"].update(noticed)
        for key, delta in deltas.items():
            if abs(delta) > abs(entry["deltas"][key]):
                entry["deltas"][key] = delta
    groups = {"pinned": [], "loose, matters": [], "loose, harmless": [], "not tried (zero, no plain value)": []}
    for path, value in numbers(record):
        label = ".".join(f"{part:g}" if isinstance(part, float) else part for part in path)
        how = sources.get(label, sources.get(".".join(path[:2]), {})).get("how", "?")
        if path not in by_number:
            groups["not tried (zero, no plain value)"].append(f"  {label} = {value:g} ({how})")
            continue
        entry = by_number[path]
        deltas = entry["deltas"]
        matters = any(abs(deltas[key]) >= limit for key, limit in limits.items())
        group = "pinned" if entry["noticed"] else "loose, matters" if matters else "loose, harmless"
        throw = (f"error {deltas['error_mean']:+.3f}, band {deltas['error_band']:+.3f}, "
                 f"over {deltas['over_true'] * 100:+.0f}%, time {deltas['time_median']:+.1f} s")
        seen = sorted(entry["noticed"])
        figures = f"{len(seen)} figures ({'; '.join(seen[:2])}{'; ...' if len(seen) > 2 else ''})" if seen else "no figure"
        lines_for = groups[group]
        lines_for.append(f"  {label} = {value:g} ({how}): {figures}; throws: {throw}{'  <-- a throw notices' if matters else ''}")
    for group, rows in groups.items():
        lines.append(f"{group}: {len(rows)}")
        lines.extend(rows)
        lines.append("")
    text = "\n".join(lines)
    print(text)
    out = args.out or HERE / "fit" / f"pinned_{name}.txt"
    out.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
