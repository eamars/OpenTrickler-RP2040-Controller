"""The firmware's charge controller (src/charge_control.c) in the simulation: built as a library, loaded by ctypes.

ADR-009, gate 1: the simulation runs the very C code the unit runs, so a change to the controller is tried at the
desk on the firmware's own code, and the PC checks the port against its reference (`staged.py`,
`check_firmware_controller.py`). The library is built with the PC's C compiler that toolchain.env names (Zig, used
only as `zig cc`), into `build_host/` at the repository's root, whenever a source is newer than it. It is built in
the unit's float, and, for the check of the port's logic, in double as the reference computes
(`CHARGE_CONTROL_DOUBLE`).

`FirmwareStaged` has the interface of `staged.Staged` (start, on_tick, on_reading, count, moved; coarse_rps,
fine_rps, done, move_turns, phase, log, stop_room), so every harness that throws `staged.Staged` throws it too.
"""

import ctypes
import os
import shutil
import subprocess
import sys
from pathlib import Path

import staged

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
SOURCES = [REPO / "src" / "charge_control.c", HERE / "c" / "charge_control_shim.c"]
HEADERS = [REPO / "src" / "charge_control.h", REPO / "src" / "charge_rules.h"]
BUILD = REPO / "build_host"

FINISHES = {"glide": 0, "jog": 1}
RESULTS = {0: "none", 1: "good", 2: "over", 3: "short", 4: "limit", 5: "cup", 6: "no_powder"}
PHASES = ("feed", "rest", "move")
COARSE_STOPS_MAX = 4
FINE_STOPS_MAX = 8
MOVES = 3
OUTPUTS = 21
RULES = ("floor_rps", "stop_room_gr", "aim_gr", "glide_room_gr", "glide_s", "coarse_room_gr", "coarse_glide_s",
         "rest_s", "fine_top_rps", "coarse_top_rps", "coarse_floor_rps", "creep_rps", "backoff_speed_rps", "backoff_s",
         "backoff_gr", "backoff_fallback")


def _types(real) -> tuple:
    """The C structs as ctypes Structures, in the precision the library was built with."""

    class Stop(ctypes.Structure):
        _fields_ = [(name, real) for name in ("speed_rps", "unseen_gr", "spread_gr", "quiet_s", "lag_s")]

    class Move(ctypes.Structure):
        _fields_ = [(name, real) for name in ("turns", "mean_gr", "most_gr", "wait_s")]

    class Calibration(ctypes.Structure):
        _fields_ = ([(name, real) for name in ("digit_gr", "interval_s", "coarse_gr_turn", "fine_gr_turn")]
                    + [(name, ctypes.c_uint8) for name in ("coarse_stop_count", "fine_stop_count", "move_count",
                                                           "finish")]
                    + [("coarse_stops", Stop * COARSE_STOPS_MAX), ("fine_stops", Stop * FINE_STOPS_MAX),
                       ("moves", Move * MOVES)]
                    + [(name, real) for name in ("watch_s", "coarse_share", "band_gr", "floor_rps", "stop_room_gr",
                                                 "aim_gr", "glide_room_gr", "glide_s", "coarse_room_gr",
                                                 "coarse_glide_s", "rest_s", "fine_top_rps", "coarse_top_rps",
                                                 "coarse_floor_rps", "creep_rps", "backoff_unseen_gr",
                                                 "backoff_spread_gr", "backoff_turns", "backoff_rps",
                                                 "backoff_speed_rps", "backoff_s", "backoff_gr", "backoff_fallback")]
                    + [("learnt", ctypes.c_uint8)])

    class Constants(ctypes.Structure):
        _fields_ = ([(name, real) for name in (
            "move_rps", "parcel_s", "coarse_least_rps", "coarse_most_rps", "fine_least_rps", "fine_most_rps",
            "user_backoff_turns", "user_backoff_rps", "coarse_threshold_gr", "gate_coarse_s", "dribble_digits_s",
            "coarse_spreads", "creep_band_share", "creep_most_s", "learn_share", "learn_most_digits",
            "learn_dead_digits", "learn_fine_prior", "learn_fine_most", "learn_fine_split", "still_least_s")]
                    + [("most_moves", ctypes.c_uint8)]
                    + [(name, real) for name in ("throw_most_s", "fine_most_times", "zero_margin_gr", "over_tolerances")]
                    + [("speed_law", ctypes.c_uint8), ("pid_envelope", ctypes.c_uint8)])

    return Stop, Move, Calibration, Constants


def _library_path(double: bool) -> Path:
    name = "charge_control_double" if double else "charge_control"
    return BUILD / (f"{name}.dll" if sys.platform == "win32" else f"lib{name}.so")


def _zig() -> str:
    """The Zig that toolchain.env names: where the setup scripts put it, or on the path."""
    version = next(line.split("=", 1)[1].strip() for line in (REPO / "toolchain.env").read_text().splitlines()
                   if line.startswith("ZIG="))
    for candidate in (Path(os.environ.get("USERPROFILE", "")) / ".pico-sdk" / "zig" / version / "zig.exe",
                      REPO / ".toolchain" / f"zig-{version}" / "zig"):
        if candidate.exists():
            return str(candidate)
    found = shutil.which("zig")
    if found:
        return found
    raise RuntimeError(f"Zig {version} is not found: run setup_toolchain.ps1 (Windows) or setup_toolchain.sh (Linux)")


def build(double: bool = False) -> Path:
    """The library, built again when a source or a header is newer than it."""
    library_file = _library_path(double)
    newest = max(path.stat().st_mtime for path in SOURCES + HEADERS)
    if library_file.exists() and library_file.stat().st_mtime >= newest:
        return library_file
    BUILD.mkdir(exist_ok=True)
    command = [_zig(), "cc", "-shared", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror", "-ffp-contract=off"]
    if sys.platform == "win32":
        command += ["-target", "x86_64-windows-gnu", "-Wl,--export-all-symbols"]
    if double:
        command += ["-DCHARGE_CONTROL_DOUBLE"]
    command += ["-I", str(REPO / "src"), "-o", str(library_file)] + [str(source) for source in SOURCES]
    subprocess.run(command, check=True, cwd=BUILD)
    return library_file


class Library:
    """The library of one precision, its functions typed, and its structs."""

    def __init__(self, double: bool):
        real = ctypes.c_double if double else ctypes.c_float
        self.Stop, self.Move, self.Calibration, self.Constants = _types(real)
        lib = ctypes.CDLL(str(build(double)))
        for name, struct in (("controller", None), ("calibration", self.Calibration), ("constants", self.Constants)):
            getattr(lib, f"shim_sizeof_{name}").restype = ctypes.c_size_t
            if struct is not None and getattr(lib, f"shim_sizeof_{name}")() != ctypes.sizeof(struct):
                raise RuntimeError(f"firmware_controller's {name} does not match charge_control.h")
        cal_p, k_p = ctypes.POINTER(self.Calibration), ctypes.POINTER(self.Constants)
        lib.charge_control_set_rules.argtypes = [cal_p, ctypes.c_int, real, k_p]
        lib.charge_control_learnt_calibration.argtypes = [cal_p, real, real, real, real, real, k_p]
        lib.charge_control_place_stop.argtypes = [cal_p, ctypes.POINTER(real), ctypes.c_uint16]
        lib.charge_control_init.argtypes = [ctypes.c_void_p, cal_p, k_p, real, ctypes.c_bool]
        lib.charge_control_start.argtypes = [ctypes.c_void_p, real, ctypes.c_uint32]
        lib.charge_control_count.argtypes = [ctypes.c_void_p, ctypes.c_uint32, real, real]
        lib.charge_control_tick.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        lib.charge_control_reading.argtypes = [ctypes.c_void_p, ctypes.c_uint32, real]
        lib.charge_control_moved.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        lib.charge_control_precharge.argtypes = [ctypes.c_void_p, real, real, ctypes.c_uint32]
        lib.charge_control_default_constants.argtypes = [k_p]
        lib.shim_outputs.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double)]
        lib.shim_parcels.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                                     ctypes.POINTER(ctypes.c_double)]
        lib.shim_parcels.restype = ctypes.c_uint16
        lib.shim_set_fine_learning.argtypes = [ctypes.c_void_p] + [ctypes.c_double] * 6
        lib.shim_set_pid.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double)]
        self.lib = lib

    def default_constants(self, limits=None):
        """The firmware's constants, with each tube's limits a user's (ADR-009, gate F.2): the simulation's stand-in
        user (`staged.sim_limits()`) unless others are given, as the charge gives the unit's from its settings."""
        k = self.Constants()
        self.lib.charge_control_default_constants(ctypes.byref(k))
        limits = limits or staged.sim_limits()
        k.coarse_least_rps, k.coarse_most_rps = limits.coarse_least, limits.coarse_most
        k.fine_least_rps, k.fine_most_rps = limits.fine_least, limits.fine_most
        k.user_backoff_turns, k.user_backoff_rps = limits.backoff_turns, limits.backoff_rps
        k.coarse_threshold_gr, k.gate_coarse_s = limits.coarse_threshold_gr, limits.gate_coarse_s
        return k


_libraries = {}


def library(double: bool = False) -> Library:
    if double not in _libraries:
        _libraries[double] = Library(double)
    return _libraries[double]


def to_c(cal: staged.Calibration, double: bool = False):
    """A `staged.Calibration` as the firmware holds one, rules and all."""
    lib = library(double)
    if (len(cal.coarse_stops) > COARSE_STOPS_MAX or len(cal.fine_stops) > FINE_STOPS_MAX
            or len(cal.moves) not in ((0, MOVES) if cal.learnt else (MOVES,))):
        raise ValueError("the calibration has more stops or moves than the firmware holds")
    out = lib.Calibration(digit_gr=cal.digit_gr, interval_s=cal.interval_s, coarse_gr_turn=cal.coarse_gr_turn,
                          fine_gr_turn=cal.fine_gr_turn, coarse_stop_count=len(cal.coarse_stops),
                          fine_stop_count=len(cal.fine_stops), move_count=len(cal.moves),
                          finish=FINISHES[cal.finish], watch_s=cal.watch_s, coarse_share=cal.coarse_share,
                          band_gr=cal.band_gr, floor_rps=cal.floor_rps, stop_room_gr=cal.stop_room_gr,
                          aim_gr=cal.aim_gr, glide_room_gr=cal.glide_room_gr, glide_s=cal.glide_s,
                          coarse_room_gr=cal.coarse_room_gr, coarse_glide_s=cal.coarse_glide_s, rest_s=cal.rest_s,
                          fine_top_rps=cal.fine_top_rps, coarse_top_rps=cal.coarse_top_rps,
                          coarse_floor_rps=cal.coarse_floor_rps, creep_rps=cal.creep_rps,
                          backoff_unseen_gr=cal.backoff_unseen_gr, backoff_spread_gr=cal.backoff_spread_gr,
                          backoff_turns=cal.backoff_turns, backoff_rps=cal.backoff_rps,
                          backoff_speed_rps=cal.backoff_speed_rps, backoff_s=cal.backoff_s, backoff_gr=cal.backoff_gr,
                          backoff_fallback=cal.backoff_fallback, learnt=1 if cal.learnt else 0)
    for i, stop in enumerate(cal.coarse_stops):
        out.coarse_stops[i] = lib.Stop(stop.speed, stop.unseen_gr, stop.spread_gr, stop.quiet_s,
                                       -1.0 if stop.lag_s is None else stop.lag_s)
    for i, stop in enumerate(cal.fine_stops):
        out.fine_stops[i] = lib.Stop(stop.speed, stop.unseen_gr, stop.spread_gr, stop.quiet_s,
                                     -1.0 if stop.lag_s is None else stop.lag_s)
    for i, move in enumerate(cal.moves):
        out.moves[i] = lib.Move(move.turns, move.mean_gr, move.most_gr, move.wait_s)
    return out


def rules_in_c(measured: staged.Calibration, finish: str, band_gr: float, double: bool = False, limits=None) -> dict:
    """The rules the firmware makes from a measured calibration, by name, as `staged.set_rules` names them."""
    cal = to_c(measured, double)
    k = library(double).default_constants(limits)
    library(double).lib.charge_control_set_rules(ctypes.byref(cal), FINISHES[finish], band_gr, ctypes.byref(k))
    return {name: getattr(cal, name) for name in RULES}


def learnt_in_c(digit_gr: float, interval_s: float, band_gr: float, lag_s: float, rest_s: float, double: bool = False,
                limits=None) -> dict:
    """The learnt source's calibration as the firmware makes it (charge_control_learnt_calibration, ADR-009 gate F.4):
    its rules by name as `staged.set_rules` names them, and its stops' lag and quiet."""
    lib = library(double)
    cal = lib.Calibration()
    k = lib.default_constants(limits)
    lib.lib.charge_control_learnt_calibration(ctypes.byref(cal), digit_gr, interval_s, band_gr, lag_s, rest_s,
                                              ctypes.byref(k))
    out = {name: getattr(cal, name) for name in RULES}
    out.update(learnt=cal.learnt, move_count=cal.move_count, coarse_lag=cal.coarse_stops[0].lag_s,
               fine_lag=cal.fine_stops[0].lag_s, coarse_gr_turn=cal.coarse_gr_turn, fine_gr_turn=cal.fine_gr_turn)
    return out


class FirmwareStaged:
    """`staged.Staged`'s interface on the firmware's controller."""

    def __init__(self, cal: staged.Calibration, acceleration_rps2: float, rehearsal: bool = False,
                 double: bool = False, limits=None):
        lib = library(double)
        self._lib = lib.lib
        self._c = ctypes.create_string_buffer(self._lib.shim_sizeof_controller())
        self._cal = to_c(cal, double)
        self._k = lib.default_constants(limits)
        self._acceleration = acceleration_rps2
        self._rehearsal = rehearsal
        self._lib.charge_control_init(self._c, ctypes.byref(self._cal), ctypes.byref(self._k), acceleration_rps2,
                                      rehearsal)
        self._out = (ctypes.c_double * OUTPUTS)()
        self.cal = cal
        self.name = cal.finish + (" (firmware, double)" if double else " (firmware)")
        self.rehearsal = rehearsal
        self.log = []
        self._logged = False
        self._read()

    def set_pid(self, law) -> None:
        """The PID speed law (`staged.PidLaw`) in place of the glide's: the controller made again with
        speed_law 1 and the law's gains. Before any learnt state is carried in."""
        self._k.speed_law = 1
        self._k.pid_envelope = 1 if getattr(law, "envelope", True) else 0
        self._lib.charge_control_init(self._c, ctypes.byref(self._cal), ctypes.byref(self._k), self._acceleration,
                                      self._rehearsal)
        gains = [law.coarse_kp, law.coarse_ki, law.coarse_kd, law.coarse_min_rps, law.coarse_max_rps, law.fine_kp,
                 law.fine_ki, law.fine_kd, law.fine_max_rps]
        self._lib.shim_set_pid(self._c, (ctypes.c_double * 9)(*gains))
        self.name += " PID law"
        self._read()

    def set_fine_prior(self, prior: float) -> None:
        """The fine figure's learning on (`staged.LEARN_FINE_PRIOR_ON`) or off (0): the controller made again with
        it. Before the law is set and before any learnt state is carried in."""
        self._k.learn_fine_prior = prior
        self._lib.charge_control_init(self._c, ctypes.byref(self._cal), ctypes.byref(self._k), self._acceleration,
                                      self._rehearsal)
        self._read()

    def set_zero_margin(self, margin: float) -> None:
        """The user's zero mean margin (A9), as the charge gives it: the controller made again with it. Before the
        law is set and before any learnt state is carried in."""
        self._k.zero_margin_gr = margin
        self._lib.charge_control_init(self._c, ctypes.byref(self._cal), ctypes.byref(self._k), self._acceleration,
                                      self._rehearsal)
        self._read()

    def set_fine_learning(self, n: int, total: float, current: float, stream_n: int = 0, stream_total: float = 0.0,
                          stream: float = None) -> None:
        """The fine tube's learnt state carried from another controller (the check's lockstep)."""
        self._lib.shim_set_fine_learning(self._c, float(n), total, current, float(stream_n), stream_total,
                                         current if stream is None else stream)

    def _read(self) -> None:
        self._lib.shim_outputs(self._c, self._out)
        out = self._out
        self.coarse_rps = out[0]
        self.fine_rps = out[1]
        self.move_turns = out[2] if out[2] >= 0.0 else None
        self.done = out[3] != 0.0
        self.result = RESULTS[int(out[4])]
        self.phase = PHASES[int(out[5])]
        self.rest_weight = out[6]
        self.stop_room = out[7]
        self.first_error = out[9] if out[8] else None
        self.moves = int(out[10])
        self.streams = int(out[11])
        self.unseen = out[12]
        self.coarse_unseen = out[13]
        self.reading = out[14]
        self.turned = {"coarse": out[15], "fine": out[16]}
        self.speed = {"coarse": out[17], "fine": out[18]}
        self.glide = {"coarse": out[19], "fine": out[20]}
        # a throw that came to rest, as the reference logs it: not one ended by a limit or a fault (gate F.1)
        if self.done and not self._logged and self.result in ("good", "over", "short"):
            self.log.append((self.first_error, self.moves, self.streams))
            self._logged = True

    def glide_speed(self, tube: str) -> float:
        """What the glide's stream law asks of a tube now (charge_control_glide_rps)."""
        return self.glide[tube]

    def parcels(self, tube: str) -> list:
        """(due, grains) of a tube's powder on its way: for a test."""
        due, grains = (ctypes.c_double * 256)(), (ctypes.c_double * 256)()
        count = self._lib.shim_parcels(self._c, 0 if tube == "coarse" else 1, due, grains)
        return [(due[i], grains[i]) for i in range(count)]

    @staticmethod
    def _ms(now_ms: float) -> int:
        return int(round(now_ms))

    def start(self, target_gr: float, now_ms: float) -> None:
        self._lib.charge_control_start(self._c, target_gr, self._ms(now_ms))
        self._logged = False
        self._read()

    def count(self, now_ms: float, turns: dict) -> None:
        self._lib.charge_control_count(self._c, self._ms(now_ms), turns["coarse"], turns["fine"])
        self._read()

    def on_tick(self, now_ms: float) -> None:
        self._lib.charge_control_tick(self._c, self._ms(now_ms))
        self._read()

    def on_reading(self, now_ms: float, weight: float) -> None:
        self._lib.charge_control_reading(self._c, self._ms(now_ms), weight)
        self._read()

    def precharge(self, grains: float, speed_rps: float, now_ms: float) -> None:
        self._lib.charge_control_precharge(self._c, grains, speed_rps, self._ms(now_ms))
        self._read()

    def moved(self, now_ms: float) -> None:
        self._lib.charge_control_moved(self._c, self._ms(now_ms))
        self._read()
