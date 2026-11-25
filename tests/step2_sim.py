"""The charge controller and the auto-tuner in C (src/charge_controller.c, src/charge_tuner.c) run on the simulation
beside the Python (tools/charge_sim/step2.py, step2_tuner.py): ADR-009, step 2, gate 2, with no unit.

The C is compiled for this PC as a library (tests/host/charge_lib.c; the host's compiler as tests/host_test.py finds
it). Three runs on the twin's rigs:
- the controller, judged: step2.py's Step2 throws on the twin and drives it; the C is given the same calls (each
  millisecond's tick and each reading) and must ask the same speeds and stop at the same millisecond; at each throw's
  rest both carry its own weight a turn, which must agree;
- the controller, alone: the C throws on the twin itself, as the charge mode will on a unit, its sessions summed up
  beside the Python's on the same seeds. Printed, not judged: the plant follows the C's own speeds;
- the tuner, judged: step2_tuner.py's Tuner, on the rule gate 1 passed with (a lower cost is enough: z 0), tunes the
  Python's throws from a start's gains; the C tuner is given the same throws and must take the same steps to the same
  kp, window for window.

    venv\\Scripts\\python tests\\step2_sim.py [--rigs ...] [--sessions 2] [--tuner-throws 300]
"""

import argparse
import ctypes
import dataclasses
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools" / "charge_sim"))
sys.path.insert(0, str(REPO / "tests"))

import rigs  # noqa: E402
import step2  # noqa: E402
import step2_tuner  # noqa: E402
from host_test import compiler  # noqa: E402
from legacy_pid import AR2209_PICOW_168  # noqa: E402
from throw import Session  # noqa: E402
from workers import Workers  # noqa: E402

LIBRARY = REPO / "build_host" / ("charge_lib.dll" if os.name == "nt" else "charge_lib.so")
LAG_POINTS = 5
TARGET = 10.0
CLOSE_RPS = 1e-4                # two speeds this near are the same: the C's settings are floats, the Python's doubles
CLOSE_K = 1e-4                  # two weights a turn this near (relative) are the same
CLOSE_KP = 1e-9
STEPS = {1: "first", 2: "resting", 3: "the base again", 4: "kept", 5: "the other way", 6: "both ways"}


class Channel(ctypes.Structure):
    _fields_ = [(name, ctypes.c_float) for name in ("kp", "ki", "kd", "least_rps", "most_rps")]


class Config(ctypes.Structure):
    _fields_ = [("coarse", Channel), ("fine", Channel), ("coarse_threshold", ctypes.c_float),
                ("backoff_enable", ctypes.c_bool), ("backoff_turns", ctypes.c_float), ("backoff_rps", ctypes.c_float),
                ("settle_s", ctypes.c_float), ("lag_count", ctypes.c_int),
                ("lag_flow_per_s", ctypes.c_float * LAG_POINTS), ("lag_s", ctypes.c_float * LAG_POINTS),
                ("acceleration_rps2", ctypes.c_float)]


def config_of(profile, records, acceleration_rps2: float) -> Config:
    """The C's config from the Python's profile and step 1's records."""
    lag = list(records.lag)[:LAG_POINTS]
    flows = (ctypes.c_float * LAG_POINTS)(*[flow for flow, _ in lag])
    lags = (ctypes.c_float * LAG_POINTS)(*[seconds for _, seconds in lag])
    return Config(Channel(profile.coarse_kp, profile.coarse_ki, profile.coarse_kd, profile.coarse_min_rps,
                          profile.coarse_max_rps),
                  Channel(profile.fine_kp, profile.fine_ki, profile.fine_kd, records.fine_least_rps,
                          profile.fine_max_rps),
                  profile.coarse_stop_threshold, profile.backoff_enable, profile.backoff_turns,
                  profile.backoff_speed_rps, records.settle_s, len(lag), flows, lags, acceleration_rps2)


def build_library() -> None:
    """The controller and the tuner built for this PC as a library."""
    LIBRARY.parent.mkdir(exist_ok=True)
    command = compiler() + ["-shared", "-std=c11", "-Wall", "-Wextra", "-O1", "-fPIC", "-I", str(REPO / "src"),
                            str(REPO / "tests" / "host" / "charge_lib.c"), str(REPO / "src" / "charge_controller.c"),
                            str(REPO / "src" / "charge_tuner.c"), "-o", str(LIBRARY)]
    built = subprocess.run(command, capture_output=True, text=True)
    if built.returncode != 0 or built.stderr.strip():
        raise SystemExit(built.stdout + built.stderr + "\nVERDICT FAIL the controller did not build cleanly")


def load_library() -> ctypes.CDLL:
    lib = ctypes.CDLL(str(LIBRARY))
    p, d, i = ctypes.c_void_p, ctypes.c_double, ctypes.c_int
    pd, pi = ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_int)
    for name, args, result in (("lib_charge_new", [], p), ("lib_charge_free", [p], None), ("lib_charge_size", [], i),
                               ("lib_charge_config", [p, ctypes.POINTER(Config)], None),
                               ("lib_charge_start", [p, d, d], i), ("lib_charge_tick", [p, d], None),
                               ("lib_charge_reading", [p, d, d], None), ("lib_charge_out", [p, pd, pd], i),
                               ("lib_charge_state", [p, pd, pd, pi, pi, pi], i),
                               ("lib_charge_verdict_after_s", [p], d), ("lib_charge_at_rest", [p, d], None),
                               ("lib_charge_carried", [p, pd], i), ("lib_tuner_new", [d], p),
                               ("lib_tuner_free", [p], None), ("lib_tuner_throw", [p, d, d, d, pd], i)):
        getattr(lib, name).argtypes = args
        getattr(lib, name).restype = result
    return lib


class CController:
    """The C controller with the Python's face: start, on_tick and on_reading, then coarse_rps, fine_rps and done."""

    def __init__(self, lib, config: Config):
        self.lib = lib
        self.handle = lib.lib_charge_new()
        self.configure(config)
        self.coarse_rps = self.fine_rps = 0.0
        self.done = True

    def configure(self, config: Config) -> None:
        self.lib.lib_charge_config(self.handle, ctypes.byref(config))

    def _out(self) -> None:
        coarse, fine = ctypes.c_double(), ctypes.c_double()
        self.done = bool(self.lib.lib_charge_out(self.handle, ctypes.byref(coarse), ctypes.byref(fine)))
        self.coarse_rps, self.fine_rps = coarse.value, fine.value

    def start(self, target: float, now_ms: float) -> None:
        if not self.lib.lib_charge_start(self.handle, target, now_ms):
            raise RuntimeError("the C refused its config")
        self._out()

    def on_tick(self, now_ms: float) -> None:
        self.lib.lib_charge_tick(self.handle, now_ms)
        self._out()

    def on_reading(self, now_ms: float, weight: float) -> None:
        self.lib.lib_charge_reading(self.handle, now_ms, weight)
        self._out()

    @property
    def verdict_after_s(self) -> float:
        return self.lib.lib_charge_verdict_after_s(self.handle)

    def at_rest(self, reading: float) -> None:
        self.lib.lib_charge_at_rest(self.handle, reading)

    def carried(self):
        k = ctypes.c_double()
        return k.value if self.lib.lib_charge_carried(self.handle, ctypes.byref(k)) else None

    def state(self) -> dict:
        k, unseen = ctypes.c_double(), ctypes.c_double()
        parcels, rows, cut = ctypes.c_int(), ctypes.c_int(), ctypes.c_int()
        known = self.lib.lib_charge_state(self.handle, ctypes.byref(k), ctypes.byref(unseen), ctypes.byref(parcels),
                                          ctypes.byref(rows), ctypes.byref(cut))
        return {"k": k.value if known else None, "unseen": unseen.value, "parcels": parcels.value, "rows": rows.value,
                "cut": bool(cut.value)}


class Paired:
    """The Python's Step2 driving the twin, the C given the same calls: each call's answers compared."""

    def __init__(self, python, c: CController):
        self.python, self.c = python, c
        self.calls = 0

    def start(self, target: float, now_ms: float) -> None:
        self.python.start(target, now_ms)
        self.c.start(target, now_ms)
        self.throw = {"rate": 0.0, "k_diff": 0.0, "py_stop": None, "c_stop": None, "first": None}
        self._compare(now_ms)

    def _compare(self, now_ms: float) -> None:
        py, c, throw = self.python, self.c, self.throw
        if py.done and throw["py_stop"] is None:
            throw["py_stop"] = now_ms
        if c.done and throw["c_stop"] is None:
            throw["c_stop"] = now_ms
        if py.done or c.done:
            return
        rate = max(abs(py.coarse_rps - c.coarse_rps), abs(py.fine_rps - c.fine_rps))
        throw["rate"] = max(throw["rate"], rate)
        c_k = c.state()["k"]
        if (py.k is None) != (c_k is None):
            throw["k_diff"] = float("inf")
        elif py.k is not None:
            throw["k_diff"] = max(throw["k_diff"], abs(py.k - c_k) / py.k)
        if throw["first"] is None and (rate > CLOSE_RPS or throw["k_diff"] > CLOSE_K):
            throw["first"] = f"at {now_ms:.0f} ms: speeds {py.coarse_rps:.5f}/{py.fine_rps:.5f} against " \
                             f"{c.coarse_rps:.5f}/{c.fine_rps:.5f}, k {py.k} against {c_k}"
        self.calls += 1

    def on_tick(self, now_ms: float) -> None:
        self.python.on_tick(now_ms)
        self.c.on_tick(now_ms)
        self._compare(now_ms)

    def on_reading(self, now_ms: float, weight: float) -> None:
        self.python.on_reading(now_ms, weight)
        self.c.on_reading(now_ms, weight)
        self._compare(now_ms)

    @property
    def coarse_rps(self) -> float:
        return self.python.coarse_rps

    @property
    def fine_rps(self) -> float:
        return self.python.fine_rps

    @property
    def done(self) -> bool:
        return self.python.done


def judged_session(job: tuple) -> list:
    """One session of the Python's throws, the C given the same calls: a dict a throw."""
    rig_name, records, seed = job
    rig = rigs.get_rig(rig_name)
    profile = AR2209_PICOW_168
    lib = load_library()
    python = step2.Step2(profile, records, rig.acceleration_rps2, before="last")
    c = CController(lib, config_of(profile, records, rig.acceleration_rps2))
    paired = Paired(python, c)
    session = Session(rig, seed)
    out = []
    for _ in range(step2.THROWS_PER_SESSION):
        after = python.verdict_after_s
        result = session.throw(paired, TARGET, profile.fine_stop_threshold, verdict_after_s=after)
        python.at_rest(result.reading)
        c.at_rest(result.reading)
        py_k, c_k = python.carried(), c.carried()
        throw = dict(paired.throw)
        throw["verdict_after"] = abs(after - c.verdict_after_s)
        throw["carried"] = (0.0 if py_k is None and c_k is None else
                            float("inf") if (py_k is None) != (c_k is None) else abs(py_k - c_k) / py_k)
        throw.update(c.state())
        throw["timeout"] = result.verdict == "timeout"
        out.append(throw)
    return out


def alone_session(job: tuple) -> tuple:
    """One session thrown by the C alone and one by the Python on the same seed: [(error, time, verdict)] each."""
    rig_name, records, seed = job
    rig = rigs.get_rig(rig_name)
    profile = AR2209_PICOW_168
    c = CController(load_library(), config_of(profile, records, rig.acceleration_rps2))
    session = Session(rig, seed)
    mine = []
    for _ in range(step2.THROWS_PER_SESSION):
        result = session.throw(c, TARGET, profile.fine_stop_threshold, verdict_after_s=c.verdict_after_s)
        c.at_rest(result.reading)
        mine.append((result.true_error, result.elapsed_s, result.verdict))
    theirs = step2.run_session((rig_name, "step2:last", TARGET, profile, records, seed))
    return mine, theirs


def tuner_case(job: tuple) -> dict:
    """The Python's throws tuned by the Python's tuner from a start's gains, the C tuner given the same throws."""
    rig_name, records, start, throws, seed = job
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(AR2209_PICOW_168, **step2_tuner.STARTS[start])
    lib = load_library()
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0)
    c_tuner = lib.lib_tuner_new(profile.fine_stop_threshold)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last")
    session = Session(rig, seed)
    errors, times, windows, mismatch = [], [], 0, None
    for _ in range(throws):
        controller.profile = profile
        result = step2.throw_one(session, controller, TARGET, profile.fine_stop_threshold)
        errors.append(result.true_error)
        times.append(result.elapsed_s)
        next_kp = ctypes.c_double()
        step = lib.lib_tuner_throw(c_tuner, profile.fine_kp, result.true_error, result.elapsed_s,
                                   ctypes.byref(next_kp))
        if len(errors) < step2_tuner.WINDOW:
            if step != 0 and mismatch is None:
                mismatch = f"window {windows}: the C closed a window early"
            continue
        thrown = profile.fine_kp
        profile, note = tuner(profile, errors, times)
        errors, times = [], []
        said = STEPS.get(step, "none")
        if mismatch is None and (said not in note or abs(next_kp.value - profile.fine_kp) > CLOSE_KP * profile.fine_kp):
            mismatch = (f"window {windows} (kp {thrown:.4f}): Python {note!r} to kp {profile.fine_kp:.6f}, "
                        f"the C {said!r} to kp {next_kp.value:.6f}")
        windows += 1
    lib.lib_tuner_free(c_tuner)
    return {"rig": rig_name, "start": start, "windows": windows, "kp": profile.fine_kp, "mismatch": mismatch}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", nargs="+", default=list(step2.DEFAULT_RIGS))
    parser.add_argument("--sessions", type=int, default=2, help="sessions of 25 throws a rig, judged and alone")
    parser.add_argument("--tuner-throws", type=int, default=300)
    parser.add_argument("--starts", nargs="+", default=["owner", "kp/4"])
    args = parser.parse_args()
    build_library()
    print(f"the controller: {load_library().lib_charge_size()} bytes")
    failures = []
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        print("The controller, judged: the C given the Python's calls on the Python's throws")
        for rig_name in args.rigs:
            jobs = [(rig_name, made[rig_name], 5000 + s) for s in range(args.sessions)]
            throws = [t for batch in pool.map(judged_session, jobs) for t in batch]
            same_stop = sum(t["py_stop"] == t["c_stop"] for t in throws)
            worst_rate = max(t["rate"] for t in throws)
            worst_k = max(t["k_diff"] for t in throws)
            worst_carry = max(t["carried"] for t in throws)
            most = max(t["parcels"] for t in throws)
            rows = max(t["rows"] for t in throws)
            cut = sum(t["cut"] for t in throws)
            print(f"  {rig_name:36s} {len(throws)} throws: the same stop {same_stop}, speeds within {worst_rate:.1e} "
                  f"rev/s, k within {worst_k:.1e}, carried within {worst_carry:.1e}; room: {most} parcels, {rows} "
                  f"readings; the own's room passed in {cut}")
            for n, t in enumerate(throws):
                if t["py_stop"] != t["c_stop"] or t["first"] or t["carried"] > CLOSE_K or t["verdict_after"] > 1e-6:
                    failures.append(f"{rig_name} throw {n}: stop {t['py_stop']} against {t['c_stop']} ms; "
                                    f"{t['first'] or ''} carried {t['carried']:.1e}, cut {t['cut']}")
        print("The controller, alone: the C throwing on the twin, the Python on the same seeds (printed)")
        for rig_name in args.rigs:
            jobs = [(rig_name, made[rig_name], 6000 + s) for s in range(args.sessions)]
            pairs = pool.map(alone_session, jobs)
            mine = [r for m, _ in pairs for r in m]
            theirs = [r for _, t in pairs for r in t]
            tol = AR2209_PICOW_168.fine_stop_threshold
            print(f"  {rig_name:36s} C      {step2.summary(mine, tol)}")
            print(f"  {'':36s} Python {step2.summary(theirs, tol)}")
        print("The tuner, judged: the C given the Python's throws window for window")
        jobs = [(rig_name, made[rig_name], start, args.tuner_throws, 7000) for rig_name in args.rigs
                for start in args.starts]
        for case in pool.map(tuner_case, jobs):
            said = case["mismatch"] or "the same steps"
            print(f"  {case['rig']:36s} from {case['start']:6s} {case['windows']} windows, kp {case['kp']:.3f}: {said}")
            if case["mismatch"]:
                failures.append(f"{case['rig']} from {case['start']}: {case['mismatch']}")
    for failure in failures[:20]:
        print("  " + failure)
    if failures:
        print(f"VERDICT FAIL {len(failures)} differences")
        return 1
    print("VERDICT PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
