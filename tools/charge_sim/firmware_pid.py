"""The firmware's PID with learning (src/charge_pid_learnt.c) in the simulation, built and loaded as firmware_controller does.

ADR-009, 2026-11-22 evening (research/pid_learning_2026-11-22.md): the simulation throws the unit's own C beside its
Python reference (legacy_pid.LearntPid), so the port is checked at the desk before the rig. `FirmwarePid` has
LearntPid's interface; the fine tube's turns are reckoned from the speeds asked for, through the motors' ramp, where
the unit counts its steps.

    python firmware_pid.py [--throws 300]      the C against the Python, side by side
"""

import argparse
import ctypes
import math
import subprocess
import sys

import firmware_controller as fc
from legacy_pid import LearntPid, Profile

SOURCES = [fc.REPO / "src" / "charge_pid_learnt.c", fc.HERE / "c" / "charge_pid_shim.c"]
HEADERS = [fc.REPO / "src" / "charge_pid_learnt.h", fc.REPO / "src" / "charge_control.h"]
LIBRARY = fc.BUILD / ("charge_pid_learnt.dll" if sys.platform == "win32" else "libcharge_pid_learnt.so")
_lib = None


def library():
    global _lib
    if _lib is not None:
        return _lib
    newest = max(path.stat().st_mtime for path in SOURCES + HEADERS)
    if not LIBRARY.exists() or LIBRARY.stat().st_mtime < newest:
        fc.BUILD.mkdir(exist_ok=True)
        command = [fc._zig(), "cc", "-shared", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror", "-ffp-contract=off"]
        if sys.platform == "win32":
            command += ["-target", "x86_64-windows-gnu", "-Wl,--export-all-symbols"]
        command += ["-I", str(fc.REPO / "src"), "-o", str(LIBRARY)] + [str(source) for source in SOURCES]
        subprocess.run(command, check=True, cwd=fc.BUILD)
    lib = ctypes.CDLL(str(LIBRARY))
    doubles = ctypes.POINTER(ctypes.c_double)
    lib.shim_pid_new.restype = ctypes.c_void_p
    lib.shim_pid_new.argtypes = [doubles, doubles]
    lib.shim_pid_free.argtypes = [ctypes.c_void_p]
    lib.shim_pid_start.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_uint32]
    lib.shim_pid_step.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_double]
    lib.shim_pid_reading.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_double]
    lib.shim_pid_outputs.argtypes = [ctypes.c_void_p, doubles]
    _lib = lib
    return lib


class FirmwarePid:
    """The C controller with LearntPid's interface and constants."""

    name = "firmware_pid"

    def __init__(self, profile: Profile, digit_gr: float, interval_s: float, band_gr: float = 0.04,
                 lag_s: float = 0.9, gr_turn_prior: float = 1.0, rest_s: float = 2.5,
                 acceleration_rps2: float = 50.0, most_tops: int = 8, coarse_quiet_s: float = 1.0):
        self.lib = library()
        backoff_turns = profile.backoff_turns if profile.backoff_enable else 0.0
        settings = [profile.coarse_kp, profile.coarse_ki, profile.coarse_kd, profile.coarse_min_rps,
                    profile.coarse_max_rps, profile.fine_kp, profile.fine_ki, profile.fine_kd, profile.fine_min_rps,
                    profile.fine_max_rps, profile.coarse_stop_threshold, backoff_turns, profile.backoff_speed_rps,
                    digit_gr, interval_s, band_gr]
        constants = [lag_s, rest_s, gr_turn_prior, LearntPid.LEARN_SHARE, coarse_quiet_s, 1e9, 3.0, digit_gr,
                     most_tops]
        self.handle = self.lib.shim_pid_new((ctypes.c_double * 16)(*settings), (ctypes.c_double * 9)(*constants))
        self.acceleration = acceleration_rps2
        self.out = (ctypes.c_double * 8)()
        self._read()

    def __del__(self):
        if getattr(self, "handle", None):
            self.lib.shim_pid_free(self.handle)

    def _read(self):
        self.lib.shim_pid_outputs(self.handle, self.out)
        o = list(self.out)
        self.coarse_rps, self.fine_rps, self.done = o[0], o[1], bool(o[2])
        self.first_error = None if math.isnan(o[4]) else o[4]
        self.tops, self.gr_turn = int(o[5]), o[6]

    def start(self, target_gr: float, now_ms: float) -> None:
        self.lib.shim_pid_start(self.handle, target_gr, int(round(now_ms)))
        self.speed = 0.0
        self.turns = 0.0
        self.last_s = now_ms / 1000.0
        self._read()

    def on_tick(self, now_ms: float) -> None:
        now = now_ms / 1000.0
        dt = now - self.last_s
        self.last_s = now
        change = self.acceleration * dt
        v = self.speed
        v = min(self.fine_rps, v + change) if v < self.fine_rps else max(self.fine_rps, v - change)
        self.speed = v
        if v > 0.0:
            self.turns += v * dt
        self.lib.shim_pid_step(self.handle, int(round(now_ms)), self.turns)
        self._read()

    def on_reading(self, now_ms: float, weight: float) -> None:
        self.lib.shim_pid_reading(self.handle, int(round(now_ms)), weight)
        self._read()


def main() -> None:
    import controllers
    import mc
    import rigs
    from workers import Workers

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=300)
    args = parser.parse_args()
    with Workers() as pool:
        for rig_name in ("pico2w_ar2209", "pico2w_bm2"):
            rig = rigs.get_rig(rig_name)
            for target in (10.0, 5.0):
                for name in ("lpid_fc_picow_fast", "lpid_fw_picow_fast"):
                    s = mc.summarise(mc.run_many(rig, name, target, args.throws, 1, pool), controllers.LOOSE_TOLERANCE_GR)
                    print(f"{rig_name} {target:4.0f} {name:20s} good {s['good_true'] * 100:5.1f}% over "
                          f"{s['over_true'] * 100:5.1f}% mean {s['error_mean']:+.3f} sd {s['error_sd']:.3f} "
                          f"median {s['time_median']:5.1f} s", flush=True)


if __name__ == "__main__":
    main()
