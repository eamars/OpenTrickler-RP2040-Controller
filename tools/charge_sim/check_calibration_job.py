"""The calibration's measurements in C (src/charge_calibration_job.c) against its reference (ADR-009, gate 3).

`staged.measure_on` measures on a bench whose runs are recorded; the very same runs, in their order, are then given
to the C, which asks for each run itself: built in double, as the reference computes, and in float, as the unit runs
it. Pass, in each build: the C asks for the same runs as the reference asked (tube, speed, run's length, watch, and
the back-off with its turns and speed) in the same order, each number to a part in a billion in double and in ten
thousand in float, and its measured calibration equals the reference's field by field, to a part in ten thousand; in
float, or within FLOAT_DIGITS of the scale's digit, the rounding of a float's readings (a reading of a 30 gr cup is
held to about 2e-6 gr, and a spread or a mean of several sums them). On the simulation's rig and every stand-in
scale, with the back-off off and on, at the simulation's stand-in user's limits and the default profile's; and on each
kept trial's runs (`runs.json` under `recordings/`), measured again (`trial.measured_again`). A kept trial made in an
older form of the calibration (before F6's prime, 2026-11-24) is said and not judged; how many were judged is printed.

Usage, with the repository's venv, from this folder:
    python check_calibration_job.py
"""

import ctypes
import dataclasses
import json
import sys
from pathlib import Path

import f4_cases
import firmware_controller
import staged

SHARE = 1e-4
FLOAT_DIGITS = 0.01     # a hundredth of a digit: no scale shows it, and a port's fault is digits, not this
FIELDS = ("digit_gr", "interval_s", "coarse_gr_turn", "fine_gr_turn", "watch_s", "coarse_share", "backoff_unseen_gr",
          "backoff_spread_gr", "backoff_turns", "backoff_rps")


class Recorded:
    """A bench whose runs are kept, as they were given to the reference."""

    def __init__(self, bench):
        self.bench = bench
        self.runs = []
        self.asked = []     # what the reference asked for each run: its length, its watch, its back-off

    def __getattr__(self, name):
        return getattr(self.bench, name)

    def creep_stop(self, tube, speed_rps, run_s, watch_s, stop, **kw):
        trial = self.bench.creep_stop(tube, speed_rps, run_s, watch_s, stop, **kw)
        self.runs.append(trial)
        self.asked.append({"run_s": run_s, "watch_s": watch_s, "backoff": kw.get("backoff")})
        return trial


def in_c(runs: list, asked_by_reference: list, digit: float, interval_s: float, limits, double: bool) -> tuple:
    """The C given the runs in order: (its calibration, the differences in what it asked)."""
    lib = firmware_controller.library(double)
    share = 1e-9 if double else SHARE

    def same(a, b):
        return abs(a - b) <= share * max(1.0, abs(b))
    job = ctypes.create_string_buffer(lib.lib.shim_sizeof_cal_job())
    k = lib.default_constants(limits)
    lib.lib.charge_cal_job_begin(job, digit, interval_s, ctypes.byref(k))
    asked = lib.Run()
    differs = []
    index = 0
    while lib.lib.charge_cal_job_next(job, ctypes.byref(asked)):
        if index >= len(runs):
            differs.append(f"run {index}: the C asks for more runs than the reference made")
            break
        trial = runs[index]
        tube = "coarse" if asked.tube == 0 else "fine"
        backoff = trial["stop"] == "backoff"
        if (tube != trial["tube"] or not same(asked.speed_rps, trial["speed_asked_rps"])
                or bool(asked.backoff) != backoff):
            differs.append(f"run {index}: the C asks {tube} at {asked.speed_rps:.6f} (back-off {bool(asked.backoff)}), "
                           f"the reference ran {trial['tube']} at {trial['speed_asked_rps']:.6f} ({trial['stop']})")
            break
        ref = asked_by_reference[index]
        if not same(asked.run_s, ref["run_s"]) or not same(asked.watch_s, ref["watch_s"]):
            differs.append(f"run {index}: the C asks a run of {asked.run_s:.6f} s watched {asked.watch_s:.6f} s, the "
                           f"reference {ref['run_s']:.6f} s watched {ref['watch_s']:.6f} s")
            break
        if backoff and (ref["backoff"] is None or not same(asked.backoff_turns, ref["backoff"][0])
                        or not same(asked.backoff_rps, ref["backoff"][1])):
            differs.append(f"run {index}: the C backs off {asked.backoff_turns:.6f} turns at {asked.backoff_rps:.6f} "
                           f"rev/s, the reference {ref['backoff']}")
            break
        for t, w in trial["during"] + trial["after"]:
            lib.lib.charge_cal_job_reading(job, t, w)
        lib.lib.charge_cal_job_ran(job, trial.get("turns", -1.0), trial["run_s"])
        index += 1
    if index < len(runs):
        differs.append(f"the C asked for {index} runs, the reference made {len(runs)}")
    return lib.lib.shim_cal_job_calibration(job).contents, differs


def compare(name: str, reference: staged.Calibration, c, differs: list, slack_gr: float = 0.0) -> list:
    off = list(differs)

    def near(a, b):
        return abs(a - b) <= max(SHARE * max(1e-3, abs(b)), slack_gr)

    for field in FIELDS:
        if not near(getattr(c, field), getattr(reference, field)):
            off.append(f"{field}: C {getattr(c, field):.6g}, reference {getattr(reference, field):.6g}")
    for kind, count_name, ref_list in (("coarse_stops", "coarse_stop_count", reference.coarse_stops),
                                       ("fine_stops", "fine_stop_count", reference.fine_stops)):
        if getattr(c, count_name) != len(ref_list):
            off.append(f"{kind}: C {getattr(c, count_name)}, reference {len(ref_list)}")
            continue
        for i, stop in enumerate(ref_list):
            mine = getattr(c, kind)[i]
            lag = -1.0 if stop.lag_s is None else stop.lag_s
            for field, a, b in (("speed", mine.speed_rps, stop.speed), ("unseen", mine.unseen_gr, stop.unseen_gr),
                                ("spread", mine.spread_gr, stop.spread_gr), ("quiet", mine.quiet_s, stop.quiet_s),
                                ("lag", mine.lag_s, lag)):
                if not near(a, b):
                    off.append(f"{kind}[{i}].{field}: C {a:.6g}, reference {b:.6g}")
    if c.move_count != len(reference.moves):
        off.append(f"moves: C {c.move_count}, reference {len(reference.moves)}")
    else:
        for i, move in enumerate(reference.moves):
            mine = c.moves[i]
            for field, a, b in (("turns", mine.turns, move.turns), ("mean", mine.mean_gr, move.mean_gr),
                                ("most", mine.most_gr, move.most_gr), ("wait", mine.wait_s, move.wait_s)):
                if not near(a, b):
                    off.append(f"moves[{i}].{field}: C {a:.6g}, reference {b:.6g}")
    print(f"{name}: {'the same' if not off else f'{len(off)} differences, first: {off[0]}'}", flush=True)
    return [f"{name}: {line}" for line in off]


def main() -> int:
    failures = []
    for double in (True, False):
        print(f"-- the C built in {'double' if double else 'float'}", flush=True)
        failures += check(double)
    for failure in failures[:20]:
        print("FAIL", failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)}")
    return 0 if not failures else 1


def check(double: bool) -> list:
    build = "double" if double else "float"
    failures = []
    users = {"sim": staged.sim_limits(),
             "profile": staged.Limits(*f4_cases.LIMITS["profile"]),
             "profile, back-off": dataclasses.replace(staged.Limits(*f4_cases.LIMITS["profile"]), backoff_turns=0.25,
                                                      backoff_rps=1.0)}
    for scale in f4_cases.names():
        rig = f4_cases.stand_in(scale)
        for user, limits in users.items():
            bench = Recorded(staged.TwinBench(rig, 7))
            reference = staged.measure_on(bench, rig.scale.digit_gr, rig.scale.report_s, limits)
            c, differs = in_c(bench.runs, bench.asked, rig.scale.digit_gr, rig.scale.report_s, limits, double)
            slack = 0.0 if double else FLOAT_DIGITS * rig.scale.digit_gr
            failures += compare(f"{build}, {scale}, {user}", reference, c, differs, slack)
    # The kept trials, measured again by their kept runs: those of today's form only, the others said and counted
    sys.path.insert(0, str(Path(__file__).parent / "bench"))
    import trial
    kept, judged = 0, 0
    for folder in sorted((Path(__file__).parent / "recordings").glob("*_trial_*")):
        if not (folder / "runs.json").exists() or not (folder / "calibration.json").exists():
            continue
        old = json.loads((folder / "calibration.json").read_text(encoding="utf-8"))
        runs = [run for run in json.loads((folder / "runs.json").read_text(encoding="utf-8"))
                if run.get("note") == "creepstop"]
        # its user's limits, as its ladder shows them: the ends of its coarse and fine stops
        limits = staged.Limits(old["coarse_stops"][0]["speed"], old["coarse_stops"][-1]["speed"],
                               old["fine_stops"][0]["speed"], old["fine_stops"][-1]["speed"])
        kept += 1
        try:
            bench = Recorded(trial.KeptBench(runs, old["seconds"], old["poured_gr"]))
            reference = staged.measure_on(bench, old["digit_gr"], old["interval_s"], limits)
        except Exception as error:      # noqa: BLE001  a kept trial of another form: said, not judged
            print(f"{folder.name}: not measured again ({error})")
            continue
        judged += 1
        c, differs = in_c(bench.runs, bench.asked, old["digit_gr"], old["interval_s"], limits, double)
        slack = 0.0 if double else FLOAT_DIGITS * old["digit_gr"]
        failures += compare(f"{build}, {folder.name}", reference, c, differs, slack)
    print(f"{build}: kept trials judged {judged} of {kept} (the others are of an older form)", flush=True)
    return failures


if __name__ == "__main__":
    sys.exit(main())
