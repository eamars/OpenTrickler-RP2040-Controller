"""A powder with no calibration, in the frame (ADR-009, gate F.4): the learnt source's cases and its study.

The pass, written in `progress.md` before the build (2026-11-24, 00:55). Every case throws the learnt source
(`staged.learnt_calibration`, the PID law on the default AR2209 profile's gains) on every stand-in scale:
- `measured`: the rig of record (`records/pico2w_ar2209_night.json`), the bench's A&D FX-300i;
- `late_small`, `holds`, `slow`, `slower`: that record with its delays stretched (`scales_like.py`);
- `gng`, `strain`: another make's rhythm and filter (`scale.py`'s GNG_JJB and SLOW_STRAIN_GAUGE, estimates never
  measured) on the record's tubes.
No stand-in models another make from its own record or data sheet: the lag and rest found here are held at gate 7.

The cases:
- uncalibrated: 3 sessions of 5 throws at 2, 6, 10 and 30 gr from no figure: every throw ends good, over or short,
  each tube inside the user's limits; and the same with the digit taken as the tolerance (no step seen yet);
- coarse_before_figure: the most the coarse tube poured past its share (the target less A6's room) while it had no
  figure, against A6's room;
- c19_before_figure: no throw ends "limit" before its fine figure exists;
- no_powder_before_figure: an empty hopper ends "no_powder";
- keeps_short: the powder falls to a third once the figure is born: the throw ends short or good, never past C18;
The study of the assumed lag and rest is `f4_landscape.py`'s (progress.md, 2026-11-24, 01:17: no pair passes).

Usage, with the repository's venv, from this folder:
    python f4_cases.py [--sessions 3] [--lag 1.0 --rest 4.0]
"""

import argparse
import collections
import dataclasses
import math
import sys

import check_firmware_controller
import firmware_controller
import rigs
import safety_cases
import scale as scale_models
import scales_like
import staged
from throw import Session
from workers import Workers

RECORD = "pico2w_ar2209_night"
TOLERANCE_GR = 0.03
FIRST = 3
STUDY_TARGET_GR = 10.0
TARGETS = (2.0, 6.0, 10.0, 30.0)
LAGS = (0.5, 1.0, 1.5, 2.0, 3.0, 4.0)
RESTS = (2.0, 3.0, 4.0, 6.0, 8.0)
ESTIMATES = {"gng": scale_models.GNG_JJB, "strain": scale_models.SLOW_STRAIN_GAUGE}
TIMEOUT_S = 130.0               # past C18's 120 s: a throw the controller does not end shows as a timeout
# The user's limits a case throws with: the simulation's stand-in user (`staged.sim_limits()`: coarse 0.3 to 2, fine
# 0.01 to 0.5 rev/s, the plant's modelled range), or the firmware's default powder profile's (src/profile.c: coarse
# 0.1 to 5, fine 0.08 to 2; the motors' 5 rev/s), the user a unit never calibrated has. Set in each worker
LIMITS = {"sim": None, "profile": (0.1, 5.0, 0.08, 2.0)}


def use_limits(name: str) -> None:
    if LIMITS[name] is not None:
        staged.COARSE_FLOOR_RPS, staged.COARSE_TOP_RPS, staged.FINE_LEAST_RPS, staged.FINE_TOP_RPS = LIMITS[name]


def names() -> list:
    return list(scales_like.SCALES) + list(ESTIMATES)


def stand_in(name: str, flow: float = 1.0):
    """A stand-in scale on the record's tubes; `flow` the share of each tube's powder that pours."""
    base = rigs.get_rig(RECORD)
    rig = dataclasses.replace(base, scale=ESTIMATES[name]) if name in ESTIMATES else scales_like.like(base, name)
    if flow != 1.0:
        rig = dataclasses.replace(rig, **{tube: dataclasses.replace(getattr(rig, tube), gr_per_turn=tuple(
            (speed, gr * flow) for speed, gr in getattr(rig, tube).gr_per_turn)) for tube in ("coarse", "fine")})
    return rig


class Watched(staged.Staged):
    """The reference, the speeds it asks kept, and the coarse tube's powder released while it had no figure."""

    def bind(self, session) -> None:
        self.session = session

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.asked = {"coarse": [], "fine": []}
        self.released_at_start = self.session.coarse.released_gr
        self.unfigured_released = None if self._figured("coarse") else 0.0
        self.fine_unfigured_end = None

    def _watch(self) -> None:
        for tube, speed in (("coarse", self.coarse_rps), ("fine", self.fine_rps)):
            if speed != 0.0:
                self.asked[tube].append(speed)
        if self.unfigured_released is not None and not self._figured("coarse") and self.coarse_rps > 0.0:
            self.unfigured_released = self.session.coarse.released_gr - self.released_at_start
        if self.done and self.result == "limit" and not self._figured("fine"):
            self.fine_unfigured_end = True

    def on_tick(self, now_ms: float) -> None:
        super().on_tick(now_ms)
        self._watch()

    def on_reading(self, now_ms: float, weight: float) -> None:
        super().on_reading(now_ms, weight)
        self._watch()


def learnt(rig, lag: float, rest: float, digit: float = None) -> Watched:
    digit = rig.scale.digit_gr if digit is None else digit
    limits = staged.sim_limits()
    band = TOLERANCE_GR - digit / 2.0
    controller = Watched(staged.learnt_calibration(digit, rig.scale.report_s, band, lag, rest, limits),
                         rig.acceleration_rps2)
    controller.pid = staged.profile_pid(limits)
    return controller


def session_job(job: tuple) -> list:
    """One session from no figure (or a calibrated unit's): each throw's end, true error, time, and what was watched."""
    name, kind, lag, rest, target, seed, count, flow = job[:8]
    use_limits(job[8] if len(job) > 8 else "sim")
    rig = stand_in(name)
    if kind == "calibrated":
        band = TOLERANCE_GR - rig.scale.digit_gr / 2.0
        controller = Watched(staged.calibrate(rig, seed, "glide", band), rig.acceleration_rps2)
    elif kind == "learnt_scale":
        # the scale's lag and rest as a calibration on this scale measures them (another seed, another powder's
        # session): what the unit's record holds once gate 3 measures it. The lag, one for every speed: the fine
        # stops' longest; the rest the calibration's
        measured = staged.for_finish(staged.measure(rig, seed + 50), "glide", TOLERANCE_GR - rig.scale.digit_gr / 2.0)
        scale_lag = max(lag_ for _, lag_ in staged.lag_table(measured.fine_stops, measured.fine_gr_turn))
        controller = learnt(rig, scale_lag, measured.rest_s)
    else:
        controller = learnt(rig, lag, rest, TOLERANCE_GR if kind == "learnt_tolerance" else None)
    session = Session(rig, seed + 100)
    controller.bind(session)
    limits = staged.sim_limits()
    out = []
    for i in range(count):
        if flow != 1.0 and i == 1:      # keeps_short: the powder falls once the first throw has made the figures
            weak = stand_in(name, flow)
            session.coarse.tube, session.fine.tube = weak.coarse, weak.fine
        r = session.throw(controller, target, TOLERANCE_GR, timeout_s=TIMEOUT_S)
        inside = all(limits.coarse_least - 1e-9 <= s <= limits.coarse_most + 1e-9 or s < 0.0
                     for s in controller.asked["coarse"]) and \
            all(limits.fine_least - 1e-9 <= s <= limits.fine_most + 1e-9 for s in controller.asked["fine"])
        past_share = None
        if controller.unfigured_released is not None and controller.coarse_ran:
            past_share = controller.unfigured_released - max(0.0, target - controller.cal.coarse_room_gr)
        out.append({"result": controller.result if controller.done else "timeout", "error": r.true_error,
                    "elapsed": r.elapsed_s, "inside": inside, "past_share": past_share,
                    "unfigured_released": controller.unfigured_released,
                    "limit_unfigured": bool(controller.fine_unfigured_end)})
    return out


class LearntLockstep(check_firmware_controller.Lockstep):
    """The lockstep, its speeds' tolerance made from the figures in use: none before a figure is born, when both
    tubes are at a speed of the rules' alone."""

    def __init__(self, reference, port):
        self.reference = reference
        self.port = port
        self.events = {"reference": [], "port": []}
        self.last = {"reference": None, "port": None}
        self.speed_off = None
        self.now_ms = 0.0
        self.tolerance = {}

    def _compare(self) -> None:
        cal = self.reference.cal
        for tube, figure, glide in (("coarse", cal.coarse_gr_turn, cal.coarse_glide_s), ("fine", cal.fine_gr_turn,
                                                                                        cal.glide_s)):
            self.tolerance[tube] = (check_firmware_controller.ROOM_GR / (figure * glide) if figure > 0.0 else 1e-9)
        super()._compare()


def lockstep_job(job: tuple) -> list:
    """One session from no figure, the reference driving and the firmware's C in double beside it, one controller each
    for the whole session as a charge has: each throw's difference (None when none) and both figures after it."""
    name, lag, rest, target, seed, count = job[:6]
    use_limits(job[6] if len(job) > 6 else "sim")
    rig = stand_in(name)
    digit = rig.scale.digit_gr
    limits = staged.sim_limits()
    cal = staged.learnt_calibration(digit, rig.scale.report_s, TOLERANCE_GR - digit / 2.0, lag, rest, limits)
    reference = staged.Staged(cal, rig.acceleration_rps2)
    reference.pid = staged.profile_pid(limits)
    port = firmware_controller.FirmwareStaged(cal, rig.acceleration_rps2, double=True, limits=limits)
    port.set_zero_margin(staged.ZERO_MARGIN)       # before the law: each makes the controller again
    port.set_pid(reference.pid)
    session = Session(rig, seed + 200)
    out = []
    for _ in range(count):
        twin = LearntLockstep(reference, port)
        session.throw(twin, target, TOLERANCE_GR, timeout_s=TIMEOUT_S)
        out.append((twin.difference(), reference.result, reference.cal.coarse_gr_turn, reference.cal.fine_gr_turn))
    return out


def over_share(sessions: list) -> tuple:
    first = [t for s in sessions for t in s[:FIRST]]
    return sum(t["error"] >= TOLERANCE_GR for t in first) / len(first), len(first)


def cases(pool, sessions: int, lag: float, rest: float, limits: str = "sim") -> list:
    failures = []
    use_limits(limits)
    # the learnt source's rules, the C's against the reference's
    for name in names():
        rig = stand_in(name)
        digit = rig.scale.digit_gr
        band = TOLERANCE_GR - digit / 2.0
        for user in (staged.sim_limits(), dataclasses.replace(staged.sim_limits(), backoff_turns=0.25, backoff_rps=1.0)):
            expected = staged.learnt_calibration(digit, rig.scale.report_s, band, lag, rest, user)
            in_c = firmware_controller.learnt_in_c(digit, rig.scale.report_s, band, lag, rest, limits=user)
            off = [rule for rule in firmware_controller.RULES
                   if abs(in_c[rule] - getattr(expected, rule)) > 1e-4 * max(1e-3, abs(getattr(expected, rule)))]
            if off or in_c["learnt"] != 1 or in_c["move_count"] != 0 or in_c["coarse_gr_turn"] != 0.0:
                failures.append(f"learnt rules {name}: the C differs in {off or 'its head'}")
    # the throw-for-throw check of the learnt source (the calibrated one's is check_firmware_controller.py's)
    jobs = [(name, lag, rest, target, seed, 5, limits) for name in names() for target in (2.0, 10.0, 30.0)
            for seed in range(sessions)]
    differing = 0
    total = 0
    for job, throws in zip(jobs, pool.map(lockstep_job, jobs)):
        for i, (difference, result, coarse, fine) in enumerate(throws):
            total += 1
            if difference:
                differing += 1
                if differing <= 5:
                    print(f"lockstep {job[0]} {job[3]:g} gr seed {job[4]} throw {i}: {difference}", flush=True)
    print(f"lockstep: {total - differing} of {total} throws with no difference", flush=True)
    if differing > total * (1.0 - check_firmware_controller.LOCKSTEP_SHARE):
        failures.append(f"lockstep: {differing} of {total} throws differ")
    jobs = [(name, kind, lag, rest, target, seed, 5, 1.0, limits) for name in names()
            for kind in ("learnt", "learnt_tolerance") for target in TARGETS for seed in range(sessions)]
    results = pool.map(session_job, jobs)
    by = collections.defaultdict(list)
    for job, throws in zip(jobs, results):
        by[(job[0], job[1], job[4])].append(throws)
    most_past = {}
    for (name, kind, target), sessions_ in sorted(by.items()):
        throws = [t for s in sessions_ for t in s]
        ends = collections.Counter(t["result"] for t in throws)
        errors = sorted(round(t["error"] / 0.02) for t in throws)
        times = sorted(t["elapsed"] for t in throws)
        past = [t["past_share"] for t in throws if t["past_share"] is not None]
        if kind == "learnt" and past:
            most_past[name] = max(most_past.get(name, -math.inf), max(past))
        print(f"uncalibrated {name:10s} {kind:16s} {target:4.0f} gr: {dict(ends)}; error (digits of 0.02) "
              f"{errors[0]}..{errors[len(errors) // 2]}..{errors[-1]}; time {times[len(times) // 2]:.1f} s median, "
              f"{times[-1]:.1f} most" + (f"; coarse past its share with no figure {max(past):+.2f}" if past else ""),
              flush=True)
        bad = {end for end in ends if end not in ("good", "over", "short")}
        if bad:
            failures.append(f"uncalibrated {name} {kind} {target:g} gr: ends {sorted(bad)}")
        if not all(t["inside"] for t in throws):
            failures.append(f"uncalibrated {name} {kind} {target:g} gr: a tube outside the user's limits")
        if any(t["limit_unfigured"] for t in throws):
            failures.append(f"c19_before_figure: {name} {kind} {target:g} gr ended limit with no fine figure")
    room = staged.sim_limits().coarse_threshold_gr
    for name, most in most_past.items():
        print(f"coarse_before_figure {name:10s}: the most past its share {most:+.2f} gr, against A6's {room:g}")
        if most >= room:
            failures.append(f"coarse_before_figure: {name} poured {most:.2f} gr past its share with no figure")

    # no powder before a figure: the hopper empty from the start
    for name in names():
        for target in (2.0, 10.0):
            rig = stand_in(name)
            controller = learnt(rig, lag, rest)
            session = safety_cases.FaultSession(rig, 7)
            controller.bind(session)
            alone = safety_cases.Alone(controller)
            result = session.throw_with(alone, target, TOLERANCE_GR, safety_cases.Fault("no_powder", when="start"),
                                        timeout_s=TIMEOUT_S)
            if result != "no_powder":
                failures.append(f"no_powder_before_figure: {name} {target:g} gr ended {result}")
    print("no_powder_before_figure: done", flush=True)

    # keeps short: the powder falls to a third once the figures are made
    jobs = [(name, "learnt", lag, rest, 10.0, seed, 4, 1.0 / 3.0, limits) for name in names() for seed in range(sessions)]
    results = pool.map(session_job, jobs)
    for job, throws in zip(jobs, results):
        for t in throws[1:]:
            if t["result"] not in ("short", "good") or t["elapsed"] > staged.THROW_MOST_S:
                failures.append(f"keeps_short: {job[0]} seed {job[5]} ended {t['result']} at {t['elapsed']:.1f} s")
    ends = collections.Counter(t["result"] for throws in results for t in throws[1:])
    print(f"keeps_short: the throws after the fall end {dict(ends)}", flush=True)
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sessions", type=int, default=3)
    parser.add_argument("--lag", type=float, default=staged.ASSUMED_LAG_S)
    parser.add_argument("--rest", type=float, default=staged.ASSUMED_REST_S)
    parser.add_argument("--limits", choices=tuple(LIMITS), default="profile",
                        help="the user's limits: the default powder profile's (a unit never calibrated) or the "
                             "simulation's stand-in user's")
    args = parser.parse_args()
    with Workers() as pool:
        failures = cases(pool, args.sessions, args.lag, args.rest, args.limits)
    for failure in failures:
        print("FAIL", failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
