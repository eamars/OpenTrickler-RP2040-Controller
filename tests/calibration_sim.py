"""The firmware's calibration method (src/calibration_method.c) run on the simulation, beside the PC's (calibration.py).

ADR-009, gate 3, with no unit: the C is compiled for this PC as a library (tests/host/calibration_lib.c; the host's
compiler as tests/host_test.py finds it). Each part is run twice for each seed:
- judged: calibration.py runs it on the simulation (tools/charge_sim/bench_twin.TwinBench), each run's readings kept;
  then the C runs it on those readings, given back call for call. The C must ask for the same runs (tube, speed, run,
  watch) in the same order and make the same records, within float's rounding: the run fails otherwise;
- alone: the C runs it on the simulation itself, its bench the twin's (a tube's speed set, time let pass and the scale's
  readings taken, the cup's pause), as the job runs it on a unit. Printed, not judged: its float times step the twin
  apart from Python's after a while.

    venv\\Scripts\\python tests\\calibration_sim.py [--seeds 3] [--rig pico2w_ar2209_night]
"""

import argparse
import ctypes
import math
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools" / "charge_sim"))
sys.path.insert(0, str(REPO / "tests"))

import calibration as reference  # noqa: E402  (the PC's method, tools/charge_sim)
import rigs  # noqa: E402
from bench_twin import TwinBench  # noqa: E402
from host_test import compiler  # noqa: E402

LAG_POINTS = 5
FAILURE_LEN = 72
DURING_MAX = 128
AFTER_MAX = 256
TUBES = {"coarse": 0, "fine": 1}
CAPACITY = 1000.0               # no pause in either: the two count the cup's powder differently near its capacity
MOTOR_LIMITS = {"fine": (0.01, 5.0), "coarse": (0.08, 5.0)}
CLOSE_S = 0.02                  # two times this near are the same, float's rounding over a part
CLOSE_RPS = 1e-4


class Sample(ctypes.Structure):
    _fields_ = [("t_s", ctypes.c_float), ("weight", ctypes.c_float)]


SET_SPEED = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_float)
WATCH = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_float, ctypes.POINTER(Sample), ctypes.c_int)
CUP_FULL = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_float, ctypes.c_float)
REPORT = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p)


class Bench(ctypes.Structure):
    _fields_ = [("ctx", ctypes.c_void_p), ("during", ctypes.POINTER(Sample)), ("after", ctypes.POINTER(Sample)),
                ("set_speed", SET_SPEED), ("watch", WATCH), ("cup_full", CUP_FULL), ("report", REPORT)]


def bench_of(callbacks) -> tuple:
    """A bench for the C with its room for a run's readings, the room kept alive beside it."""
    room = ((Sample * DURING_MAX)(), (Sample * AFTER_MAX)())
    return Bench(None, ctypes.cast(room[0], ctypes.POINTER(Sample)), ctypes.cast(room[1], ctypes.POINTER(Sample)),
                 *callbacks), room


class ScaleRecord(ctypes.Structure):
    _fields_ = [("settle_s", ctypes.c_float), ("lag_count", ctypes.c_int),
                ("lag_flow_per_s", ctypes.c_float * LAG_POINTS), ("lag_s", ctypes.c_float * LAG_POINTS),
                ("seconds", ctypes.c_float), ("poured", ctypes.c_float), ("failure", ctypes.c_char * FAILURE_LEN)]


class SpeedsRecord(ctypes.Structure):
    _fields_ = [("least_rps", ctypes.c_float), ("stream_per_turn", ctypes.c_float), ("seconds", ctypes.c_float),
                ("poured", ctypes.c_float), ("failure", ctypes.c_char * FAILURE_LEN)]


def build_library() -> ctypes.CDLL:
    """calibration_method.c built for this PC as a library, and loaded."""
    out = REPO / "build_host"
    out.mkdir(exist_ok=True)
    library = out / ("calibration_lib.dll" if os.name == "nt" else "calibration_lib.so")
    command = compiler() + ["-shared", "-std=c11", "-Wall", "-O1", "-fPIC", "-I", str(REPO / "src"),
                            str(REPO / "tests" / "host" / "calibration_lib.c"), str(REPO / "src" / "calibration_method.c"),
                            "-o", str(library)]
    built = subprocess.run(command, capture_output=True, text=True)
    if built.returncode != 0:
        raise SystemExit(built.stdout + built.stderr + "\nVERDICT FAIL the method did not build")
    lib = ctypes.CDLL(str(library))
    lib.lib_calibration_scale_part.argtypes = [ctypes.POINTER(Bench), ctypes.POINTER(ScaleRecord)]
    lib.lib_calibration_speeds_part.argtypes = [ctypes.POINTER(Bench), ctypes.c_int, ctypes.c_float, ctypes.c_float,
                                                ctypes.POINTER(ScaleRecord), ctypes.POINTER(SpeedsRecord)]
    lib.lib_calibration_cup.argtypes = [ctypes.c_float, ctypes.c_float]
    return lib


class RecordingTwin(TwinBench):
    """The simulation, each run Python's method makes kept: (tube, speed, run, watch, the trial)."""

    def __init__(self, rig, seed: int):
        super().__init__(rig, seed)
        self.runs = []

    def creep_stop(self, tube, speed_rps, run_s, still_s, stop, backoff=(0.25, 1.0)):
        trial = super().creep_stop(tube, speed_rps, run_s, still_s, stop, backoff)
        self.runs.append((tube, speed_rps, run_s, still_s, trial))
        return trial


class ReplayForC:
    """The runs Python's method made, given back to the C call for call: each asked as Python asked it, or noted."""

    def __init__(self, runs: list):
        self.runs = runs
        self.next = 0
        self.phase = "idle"
        self.mismatch = None
        self.callbacks = (SET_SPEED(self.set_speed), WATCH(self.watch), CUP_FULL(self.cup_full), REPORT(self.report))
        self.bench, self.room = bench_of(self.callbacks)

    def note(self, what):
        if self.mismatch is None:
            self.mismatch = f"run {self.next + 1}: {what}"

    def set_speed(self, ctx, tube, rps):
        if rps <= 0.0:
            return
        if self.next >= len(self.runs):
            self.note("the C turned a tube after Python's last run")
            return
        py_tube, py_speed = self.runs[self.next][:2]
        if TUBES[py_tube] != tube or abs(py_speed - rps) > 1e-4 * max(1.0, py_speed):
            self.note(f"the C turned tube {tube} at {rps:.5f}, Python {py_tube} at {py_speed:.5f}")
        self.phase = "during"

    def watch(self, ctx, seconds, out, room):
        if self.next >= len(self.runs) or self.phase == "idle":
            self.note("the C watched with no run")
            return -1
        _, _, run_s, watch_s, trial = self.runs[self.next]
        if self.phase == "during":
            if abs(run_s - seconds) > 1e-3:
                self.note(f"the C ran {seconds:.4f} s, Python {run_s:.4f} s")
            samples = [(t + run_s, w) for t, w in trial["during"]]
            self.phase = "after"
        else:
            if abs(watch_s - seconds) > 1e-3:
                self.note(f"the C watched {seconds:.4f} s, Python {watch_s:.4f} s")
            samples = trial["after"]
            self.phase = "idle"
            self.next += 1
        for idx, (t, weight) in enumerate(samples[:room]):
            out[idx].t_s = t
            out[idx].weight = weight
        return min(len(samples), room)

    def cup_full(self, ctx, held, most):
        return True

    def report(self, ctx, percent, line):
        pass


class TwinForC:
    """The simulation as the C method's bench: its callbacks kept alive while it runs."""

    def __init__(self, rig, seed: int):
        self.twin = TwinBench(rig, seed)
        self.pauses = 0
        self.callbacks = (SET_SPEED(self.set_speed), WATCH(self.watch), CUP_FULL(self.cup_full), REPORT(self.report))
        self.bench, self.room = bench_of(self.callbacks)

    def set_speed(self, ctx, tube, rps):
        self.twin.tubes["fine" if tube == TUBES["fine"] else "coarse"].set_speed(rps)

    def watch(self, ctx, seconds, out, room):
        start = self.twin.now
        samples = self.twin.advance(seconds)
        for idx, (t, weight) in enumerate(samples[:room]):
            out[idx].t_s = t - start
            out[idx].weight = weight
        return min(len(samples), room)

    def cup_full(self, ctx, held, most):
        self.pauses += 1
        return True

    def report(self, ctx, percent, line):
        pass


def close(a: float, b: float, tolerance: float) -> bool:
    return (a is None and (b is None or math.isnan(b))) or (a is not None and not math.isnan(b) and abs(a - b) <= tolerance)


def main() -> int:
    """
    Run each part in C and in Python on each seed, and compare their records.

    Returns:
        0 when they agree, 1 otherwise.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rig", default="pico2w_ar2209_night")
    parser.add_argument("--seeds", type=int, default=3)
    args = parser.parse_args()
    lib = build_library()
    rig = rigs.get_rig(args.rig)
    differ = []
    for seed in range(args.seeds):
        # The scale part: Python's, then the C on its readings
        twin = RecordingTwin(rig, seed)
        py = reference.scale_part(twin, reference.Cup(capacity_gr=CAPACITY))
        replay = ReplayForC(twin.runs)
        lib.lib_calibration_cup(CAPACITY, 0.0)
        record = ScaleRecord()
        lib.lib_calibration_scale_part(ctypes.byref(replay.bench), ctypes.byref(record))
        c_lag = [(record.lag_flow_per_s[i], record.lag_s[i]) for i in range(record.lag_count)]
        py_lag = py.records.get("lag", [])
        print(f"seed {seed} scale, {len(twin.runs)} runs: settle C {record.settle_s:.4f} s, "
              f"Python {py.records.get('settle_s', float('nan')):.4f} s")
        for (cf, cl), (pf, pl) in zip(c_lag, py_lag):
            print(f"    lag C {cf:7.3f} gr/s {cl:.4f} s   Python {pf:7.3f} gr/s {pl:.4f} s")
        if (replay.mismatch or replay.next != len(twin.runs)
                or not close(py.records.get("settle_s"), record.settle_s, CLOSE_S) or len(c_lag) != len(py_lag)
                or any(abs(cf - pf) > 1e-3 * max(1.0, pf) or abs(cl - pl) > CLOSE_S
                       for (cf, cl), (pf, pl) in zip(c_lag, py_lag))):
            differ.append(f"seed {seed} scale ({replay.mismatch or f'{replay.next} of {len(twin.runs)} runs'})")
        # Each tube's speeds, on the scale's defaults
        for tube in ("fine", "coarse"):
            twin = RecordingTwin(rig, seed)
            py = reference.speeds_part(twin, reference.Cup(capacity_gr=CAPACITY), tube, *MOTOR_LIMITS[tube])
            replay = ReplayForC(twin.runs)
            lib.lib_calibration_cup(CAPACITY, 0.0)
            speeds = SpeedsRecord()
            lib.lib_calibration_speeds_part(ctypes.byref(replay.bench), TUBES[tube], *MOTOR_LIMITS[tube], None,
                                            ctypes.byref(speeds))
            py_least = py.records.get("least_rps")
            print(f"seed {seed} {tube}, {len(twin.runs)} runs: least C {speeds.least_rps:.4f}, Python "
                  f"{float('nan') if py_least is None else py_least:.4f} rev/s; stream C {speeds.stream_per_turn:.4f}, "
                  f"Python {py.records.get('stream_gr_turn', float('nan')):.4f} gr/turn")
            if replay.mismatch or replay.next != len(twin.runs) or not close(py_least, speeds.least_rps, CLOSE_RPS):
                differ.append(f"seed {seed} {tube} ({replay.mismatch or f'{replay.next} of {len(twin.runs)} runs'})")
        # The C alone on the twin, as on a unit
        twin = TwinForC(rig, seed)
        lib.lib_calibration_cup(CAPACITY, 0.0)
        alone = ScaleRecord()
        lib.lib_calibration_scale_part(ctypes.byref(twin.bench), ctypes.byref(alone))
        line = (f"    alone: scale settle {alone.settle_s:.3f} s, lag {alone.lag_count} flows "
                f"({twin.twin.now / 60:.1f} min, {twin.twin.cup.landed_gr:.0f} gr)")
        for tube in ("fine", "coarse"):
            twin = TwinForC(rig, seed)
            lib.lib_calibration_cup(CAPACITY, 0.0)
            speeds = SpeedsRecord()
            lib.lib_calibration_speeds_part(ctypes.byref(twin.bench), TUBES[tube], *MOTOR_LIMITS[tube],
                                            ctypes.byref(alone), ctypes.byref(speeds))
            line += (f"; {tube} least {speeds.least_rps:.3f} rev/s ({twin.twin.now / 60:.1f} min, "
                     f"{twin.twin.cup.landed_gr:.0f} gr)")
        print(line)
    print("VERDICT PASS" if not differ else "VERDICT FAIL the C and the PC's method differ: " + "; ".join(differ))
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main())
