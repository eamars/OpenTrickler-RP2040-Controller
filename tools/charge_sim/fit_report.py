"""The simulation against the bench: every figure the bench measured, beside the same figure from the simulation.

Each bench test is run again in the simulation (`bench_twin.py`) on a rig's record and summed up by the code
that summed up the recording (`bench/stats.py`). A figure passes when the simulation is within its tolerance of the
measurement; the tolerances are engineering ones (what a controller designed on the simulation would notice), wider
where the measurement is itself a few dozen trials.

The open-loop tests (maps, stops, steady runs, nudges) are what `build_rig.py` made the record from. The throws of
today's loop are not: they are the check, and are counted apart.

Usage:
    python fit_report.py [--rig bench_pico2w] [--targets fit/<rig>.json] [--only creep,throws] [--seed 1]
"""

import argparse
import json
from pathlib import Path

import bench_twin
import rigs
from legacy_pid import Profile

stats = bench_twin.stats    # bench/stats.py, which the recordings were summed up with

# (figure, absolute tolerance, relative tolerance): a figure passes within either.
CREEP_STOP = [("rate", 0.2, 0.12), ("start_1s", 0.5, 0.3), ("start_2s", 0.8, 0.3), ("fano_1s", 0.5, 0.35), ("rise", 0.6, 0.25),
              ("rise_sd", 0.6, 0.25), ("three_hold", 0.12, 0.0)]
# A coarse stop's start is not judged: the bench shuffled plain stops and back-offs, so a trial's start follows
# another kind's stop.
COARSE_STOP = [("rise", 5.0, 0.2), ("rise_sd", 6.0, 0.5)]
CREEP_STEADY = [("rate", 0.2, 0.12), ("fano_1s", 0.5, 0.35), ("fano_2s", 0.5, 0.35), ("fano_4s", 0.5, 0.35),
                ("most_1s", 2.0, 0.3), ("fold_turn", 0.06, 0.0), ("fold_rifling", 0.06, 0.0)]
NUDGES = [("kernels", 0.1, 0.25), ("fano", 1.0, 0.3), ("nothing", 0.08, 0.0), ("three_up", 0.04, 0.0),
          ("first_single_s", 0.4, 0.0), ("first_many_s", 0.25, 0.0), ("single_late", 0.15, 0.0)]
THROWS = [("time", 0.7, 0.15), ("coarse_s", 0.5, 0.15), ("at_stop", 0.04, 0.3), ("at_rest", 0.03, 0.25),
          ("at_rest_sd", 0.03, 0.4), ("over", 0.17, 0.0)]
MAPS = [("gr_per_turn", 0.0, 0.08), ("late_s_fast", 0.12, 0.0)]
# The unit's stored coarse settings, the same in every recorded throw (logs/adr009/settings_before)
COARSE = {"coarse_kp": 0.02, "coarse_ki": 0.0, "coarse_kd": 0.3, "coarse_min_rps": 0.1, "coarse_max_rps": 5.0}
SIM_STOPS = 150         # stops simulated for each kind the bench made a few dozen of
SIM_THROWS = 100


def map_twin(rig, tube: str) -> dict:
    """A tube's powder a turn and the seconds of its flow that show after a stop, from a fast simulated run."""
    speed = 1.0
    trials = bench_twin.creep_trials(rig, tube, speed, "held", (5.0, 5.0), 4.0, 30, 11)
    figures = stats.creep_stats(trials)
    rate_gr_s = figures["rate"] * stats.DIGIT_GR
    return {"gr_per_turn": rate_gr_s / speed, "late_s_fast": figures["rise"] * stats.DIGIT_GR / rate_gr_s}


def evaluate(rig, targets: dict, only: list, seed: int, match: str = "", missing: bool = False) -> list:
    """
    Every measured figure with the simulation's.

    Args:
        rig: the simulation's parameters.
        targets: the bench's figures (`bench/measure.py`).
        only: the kinds of test to run: maps, creep, nudges, throws.
        seed: the simulation's.
        match: run only the tests whose title holds one of these texts, comma-separated.
        missing: also give the figures the bench has and the simulation could not make, with None for it.

    Returns:
        (title, figure, measured, simulated, tolerance) for each figure both sides have.
    """
    tests = []
    if "maps" in only:
        for tube, measured in targets["maps"].items():
            tests.append((f"map {tube}", measured, MAPS, lambda tube=tube: map_twin(rig, tube)))
    if "creep" in only:
        for key, measured in targets["creep"].items():
            steady = measured["length"] == "steady"
            count = 8 if steady else 40 if measured["tube"] == "coarse" else SIM_STOPS
            figures = CREEP_STEADY if steady else COARSE_STOP if measured["tube"] == "coarse" else CREEP_STOP
            tests.append((key, measured, figures, lambda m=measured, steady=steady, count=count: stats.creep_stats(
                bench_twin.creep_trials(rig, m["tube"], m["speed_rps"], m["stop"], tuple(m["run_s"]),
                                        4.0 if steady else 3.0, count, seed))))
    if "nudges" in only:
        measured = targets["nudges"]
        tests.append(("nudges", measured, NUDGES, lambda m=measured: bench_twin.nudge_series(
            rig, m["speed_rps"], m["turn"], m["still_s"], 600, seed)))
    if "throws" in only:
        for name, measured in targets["throws"].items():
            profile = Profile(fine_kp=measured["fine_kp"], fine_ki=0.0, fine_kd=10.0, fine_min_rps=measured["fine_min"],
                              fine_max_rps=measured["fine_max"], **COARSE)
            tests.append((f"throws {name} at {measured['target']:g} gr", measured, THROWS,
                          lambda m=measured, profile=profile: stats.throw_stats(
                              bench_twin.throws(rig, profile, m["target"], SIM_THROWS, seed))))
    rows = []
    for title, measured, figures, run in tests:
        if not any(part in title for part in match.split(",")):
            continue
        simulated = run()
        for name, absolute, relative in figures:
            want, got = measured.get(name), simulated.get(name)
            if want is not None and (got is not None or missing):
                rows.append((title, name, want, got, max(absolute, relative * abs(want))))
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="bench_pico2w")
    parser.add_argument("--targets", type=Path, help="the bench's figures; fit/<rig>.json when not given")
    parser.add_argument("--only", default="maps,creep,nudges,throws")
    parser.add_argument("--match", default="", help="only the tests whose title holds this text")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    targets_path = args.targets or Path(__file__).parent / "fit" / f"{Path(args.rig).stem}.json"
    targets = json.loads(targets_path.read_text(encoding="utf-8"))
    rows = evaluate(rigs.get_rig(args.rig), targets, args.only.split(","), args.seed, args.match)
    print(f"measured|simulated, rig {args.rig}; OFF marks a figure outside its tolerance")
    titles = []
    for row in rows:
        if row[0] not in titles:
            titles.append(row[0])
    for title in titles:
        cells = [f"{name} {want:.2f}|{got:.2f}{'' if abs(got - want) <= tolerance else ' OFF'}"
                 for row_title, name, want, got, tolerance in rows if row_title == title]
        print(f"{title}: " + ", ".join(cells))
    for label, wanted in (("the record's own recordings", False), ("the check, throws of today's loop", True)):
        judged = [row for row in rows if row[0].startswith("throws") == wanted]
        passed = sum(1 for _, _, want, got, tolerance in judged if abs(got - want) <= tolerance)
        print(f"{label}: {passed} of {len(judged)} figures within tolerance")


if __name__ == "__main__":
    main()
