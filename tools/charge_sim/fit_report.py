"""The simulation against the bench: every figure the bench measured, beside the same figure from the simulation.

Each bench test is run again in the simulation (`bench_twin.py`) on a rig's parameters and summed up by the code
that summed up the recording (`bench/stats.py`). A figure passes when the simulation is within its tolerance of the
measurement; the tolerances are engineering ones (what a controller designed on the simulation would notice), wider
where the measurement is itself a few dozen trials.

Usage:
    python fit_report.py [--rig bench_pico2w] [--targets fit/bench_pico2w.json] [--only creep,throws] [--seed 1]
"""

import argparse
import json
from pathlib import Path

import bench_twin
import rigs
from legacy_pid import Profile

stats = bench_twin.stats    # bench/stats.py, which the recordings were summed up with

# (figure, absolute tolerance, relative tolerance): a figure passes within either.
CREEP_STOP = [("rate", 0.2, 0.12), ("start_2s", 0.8, 0.3), ("fano_1s", 0.5, 0.35), ("rise", 0.6, 0.25),
              ("rise_sd", 0.6, 0.25), ("three_hold", 0.12, 0.0)]
CREEP_STEADY = [("rate", 0.2, 0.12), ("fano_1s", 0.5, 0.35), ("fano_2s", 0.5, 0.35), ("fano_4s", 0.5, 0.35),
                ("most_1s", 2.0, 0.3)]
NUDGES = [("kernels", 0.1, 0.25), ("fano", 1.0, 0.3), ("nothing", 0.08, 0.0), ("three_up", 0.04, 0.0),
          ("first_single_s", 0.4, 0.0), ("first_many_s", 0.25, 0.0)]
THROWS = [("time", 0.7, 0.15), ("coarse_s", 0.5, 0.15), ("at_stop", 0.04, 0.3), ("at_rest", 0.03, 0.25),
          ("at_rest_sd", 0.03, 0.4), ("over", 0.17, 0.0)]
MAPS = [("gr_per_turn", 0.0, 0.08), ("late_s_fast", 0.12, 0.0)]
# The unit's stored coarse settings, the same in every recorded throw (logs/adr009/settings_before)
COARSE = {"coarse_kp": 0.02, "coarse_ki": 0.0, "coarse_kd": 0.3, "coarse_min_rps": 0.1, "coarse_max_rps": 5.0}
SIM_STOPS = 150         # stops simulated for each kind the bench made a few dozen of
SIM_THROWS = 100


class Report:
    """Lines of measured against simulated, and the count that pass."""

    def __init__(self):
        self.passed = 0
        self.judged = 0

    def rows(self, title: str, measured: dict, simulated: dict, figures: list) -> None:
        cells = []
        for name, absolute, relative in figures:
            want, got = measured.get(name), simulated.get(name)
            if want is None or got is None:
                continue
            ok = abs(got - want) <= max(absolute, relative * abs(want))
            self.judged += 1
            self.passed += ok
            cells.append(f"{name} {want:.2f}|{got:.2f}{'' if ok else ' OFF'}")
        print(f"{title}: " + ", ".join(cells))


def map_twin(rig, tube: str) -> dict:
    """A tube's powder a turn and the seconds of its flow that show after a stop, from a fast simulated run."""
    speed = 1.0
    trials = bench_twin.creep_trials(rig, tube, speed, "held", (5.0, 5.0), 4.0, 12, 11)
    figures = stats.creep_stats(trials)
    rate_gr_s = figures["rate"] * stats.DIGIT_GR
    return {"gr_per_turn": rate_gr_s / speed, "late_s_fast": figures["rise"] * stats.DIGIT_GR / rate_gr_s}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="bench_pico2w")
    parser.add_argument("--targets", type=Path, default=Path(__file__).parent / "fit" / "bench_pico2w.json")
    parser.add_argument("--only", default="maps,creep,nudges,throws")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    targets = json.loads(args.targets.read_text(encoding="utf-8"))
    rig = rigs.RIGS[args.rig]
    only = args.only.split(",")
    report = Report()
    print(f"measured|simulated, rig {args.rig}; OFF marks a figure outside its tolerance")

    if "maps" in only:
        for tube, measured in targets["maps"].items():
            report.rows(f"map {tube}", measured, map_twin(rig, tube), MAPS)
    if "creep" in only:
        for key, measured in targets["creep"].items():
            steady = measured["length"] == "steady"
            count = 8 if steady else SIM_STOPS
            trials = bench_twin.creep_trials(rig, measured["tube"], measured["speed_rps"], measured["stop"],
                                             tuple(measured["run_s"]), 4.0 if steady else 3.0, count, args.seed)
            report.rows(key, measured, stats.creep_stats(trials), CREEP_STEADY if steady else CREEP_STOP)
    if "nudges" in only:
        measured = targets["nudges"]
        simulated = bench_twin.nudge_series(rig, measured["speed_rps"], measured["turn"], measured["still_s"], 600,
                                            args.seed)
        report.rows("nudges", measured, simulated, NUDGES)
    if "throws" in only:
        for name, measured in targets["throws"].items():
            profile = Profile(fine_kp=measured["fine_kp"], fine_ki=0.0, fine_kd=10.0, fine_min_rps=measured["fine_min"],
                              fine_max_rps=measured["fine_max"], **COARSE)
            simulated = stats.throw_stats(bench_twin.throws(rig, profile, measured["target"], SIM_THROWS, args.seed))
            report.rows(f"throws {name} at {measured['target']:g} gr", measured, simulated, THROWS)
    print(f"{report.passed} of {report.judged} figures within tolerance")


if __name__ == "__main__":
    main()
