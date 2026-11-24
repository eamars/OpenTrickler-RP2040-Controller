"""The controller's safety cases in the simulation (ADR-009, gate F.1).

Each case throws on the rig of record (`records/pico2w_ar2209_night.json`) with one fault put in by the harness, the
reference (`staged.Staged`) driving the rig. A case passes when its pass, written in `progress.md` before the build
(2026-11-23, 21:58, on branch adr009_controller), holds for the reference. On that branch the firmware's C ran in
lockstep beside it; the restart of ADR-009 left the C out, and the reference is thrown alone:

- none: every throw ends good, over or short, none "cup", "no_powder" or "limit";
- cup_lifted: the cup lifted (the pan reads the cup's weight under zero) at mid-coarse, and at the creep: the throw
  ends "cup" on the first reading below the cup test's line, both tubes asked to stop by it;
- light_cup: the same with the lightest real pan, 5 g (77 gr), in grains;
- light_cup_limit: a cup of 5 gr (0.32 g), lighter than any pan: the cup test's limit shown, not judged. A lift is
  caught when the cup outweighs what pours onto the bare pan within the scale's lag (README, F.1);
- grams: the rig's scale reading grams (target, digit, tolerance and the cup in grams): `none`, `cup_lifted`, a cup
  of 5 g (`light_cup`: the cup E7's fixed 10 missed on a gram scale) and `far_over`;
- drift: the reading drifting down a digit over the first 15 s of a throw: no throw ends "cup";
- no_powder: the hopper empty from the start, and the fine tube jammed at the creep: the throw ends "no_powder", both
  tubes stopped; the time from the fault to the stop is printed;
- no_powder_drift: the hopper empty from the start on a scale drifting down a digit: the throw ends "no_powder";
- coarse_jam: the coarse tube jammed mid-coarse, the fine tube pouring: said as found (no pass was written for it);
- third: the fine tube pouring a third of the calibration's grains a turn: no throw ends "limit" or "no_powder";
- far_over: the pan pushed to 100 tolerances and a digit over the target mid-coarse: the throw ends "limit" at once.

Usage, with the repository's venv, from this folder:
    python safety_cases.py [--cases none,cup_lifted] [--throws 10]
"""

import argparse
import dataclasses
import sys

import bench_twin
import rigs
import staged
import throw
from controllers import VERDICT_TOLERANCE_GR
from throw import DT_S, Session

RECORD = "pico2w_ar2209_night"
SEED = 4242
TARGET_GR = 20.0
CUP_GR = 150.0                  # the cup of the unit's fault cases on adr009_controller (robust_staged.py), in grains
GRAINS_A_GRAM = 15.4323584
LIGHT_CUP_G = 5.0               # the lightest real pan, in grams (E7's old 10 missed it on a gram scale)
LIMIT_CUP_GR = 5.0              # a cup lighter than any pan, in grains: the cup test's limit
LIFT_S = 2.0
ZERO_MARGIN = 0.02              # A9, the zero's mean margin: the user's, at its default
CASES = ("none", "cup_lifted", "light_cup", "light_cup_limit", "grams", "drift", "no_powder", "no_powder_drift",
         "coarse_jam", "third", "far_over")


class GramScaleState:
    """A scale reading grams: the rig's scale with its digit made a gram scale's (0.001 g), its readings in grams."""

    def __init__(self, model, rng, dt_s):
        model = dataclasses.replace(model, digit_gr=0.001 * GRAINS_A_GRAM)
        self.inner = GramScaleState.inner_class(model, rng, dt_s)

    def step(self, now_s, pan_gr):
        reading = self.inner.step(now_s, pan_gr)
        return None if reading is None else reading / GRAINS_A_GRAM


class Fault:
    """What the harness does to a throw: the pan's load, and which tube's powder never lands."""

    def __init__(self, name: str, unit: float = 1.0, digit: float = 0.02, tolerance: float = 0.03,
                 cup: float = CUP_GR, when: str = "coarse", drift: bool = False, target: float = TARGET_GR):
        self.name, self.unit, self.digit, self.tolerance, self.cup, self.when = name, unit, digit, tolerance, cup, when
        self.drift = drift or name == "drift"
        self.target = target
        self.landed_at_lift = None  # a lifted cup takes its powder with it: only what lands on the bare pan shows
        self.at = None              # when the fault was put in, in seconds
        self.stopped_at = None      # when the controller first asked both tubes to stop after it

    def due(self, now: float, controller, landed_gr: float) -> bool:
        """The fault's moment: mid-coarse, or the creep (the coarse tube off, the fine tube turning, near target)."""
        if self.when == "start":
            return True
        if self.when == "coarse":
            return controller.coarse_rps > 0.0 and landed_gr >= 5.0
        return (controller.phase == "feed" and controller.coarse_rps == 0.0 and controller.fine_rps > 0.0
                and landed_gr / self.unit >= controller.target - 1.0 / self.unit)

    def pan(self, now: float, pan_gr: float) -> float:
        """The pan's load in grains, as the fault makes it."""
        if self.at is None:
            return pan_gr
        since = now - self.at
        if self.name in ("cup_lifted", "light_cup") and since < LIFT_S:
            if self.landed_at_lift is None:
                self.landed_at_lift = pan_gr
            return pan_gr - self.landed_at_lift - self.cup * self.unit
        if self.name in ("cup_lifted", "light_cup") and self.landed_at_lift is not None:
            return pan_gr               # put back, with what landed on the pan meanwhile
        if self.drift:
            pan_gr -= self.digit * self.unit * min(1.0, since / 15.0)
        if self.name == "far_over":
            return max(pan_gr, (self.target + 100.0 * self.tolerance + self.digit) * self.unit)
        return pan_gr

    def lands(self, tube: str) -> bool:
        """Whether this tube's powder reaches the cup now."""
        if self.at is None:
            return not (self.name == "no_powder" and self.when == "start")
        if self.name == "no_powder":
            return False if self.when == "start" else tube != "fine"
        if self.name == "coarse_jam":
            return tube != "coarse"
        return True


class FaultSession(Session):
    """`throw.Session` with a fault put in: the pan's load and the tubes' landings go through it."""

    def throw_with(self, controller, target: float, tolerance: float, fault: Fault, timeout_s: float = 90.0):
        coarse, fine = self.coarse, self.fine
        cup = throw.Cup()
        scale = throw.ScaleState(self.rig.scale, self.rng, DT_S)
        lost = []                   # what a tube releases while it does not land
        now = 0.0
        controller.start(target, 0.0)
        if fault.when == "start":
            fault.at = 0.0
        while not controller.done and now < timeout_s:
            now += DT_S
            controller.on_tick(now * 1000.0)
            coarse.set_speed(controller.coarse_rps)
            fine.set_speed(controller.fine_rps)
            coarse.step(now, DT_S, cup.landings if fault.lands("coarse") else lost)
            fine.step(now, DT_S, cup.landings if fault.lands("fine") else lost)
            if fault.at is None and fault.due(now, controller.reference, cup.landed_gr):
                fault.at = now
            reading = scale.step(now, fault.pan(now, cup.step(now, DT_S)))
            if reading is not None:
                controller.on_reading(now * 1000.0, reading)
                coarse.set_speed(controller.coarse_rps)
                fine.set_speed(controller.fine_rps)
            if (fault.at is not None and fault.stopped_at is None and controller.coarse_rps == 0.0
                    and controller.fine_rps == 0.0):
                fault.stopped_at = now
        coarse.set_speed(0.0)
        fine.set_speed(0.0)
        for _ in range(int(3.0 / DT_S)):        # the tubes run down before the next throw
            now += DT_S
            coarse.step(now, DT_S, lost)
            fine.step(now, DT_S, lost)
        return controller.reference.result if controller.done else "timeout"


def calibrated(rig, digit: float, band: float):
    """`staged.calibrate` with the scale's digit given: a gram scale's is not the rig's model's."""
    cal = staged.measure_on(staged.TwinBench(rig, SEED), digit, rig.scale.report_s)
    staged.set_rules(cal, "glide", band)
    staged.rehearse(cal, Session(rig, SEED + 1), rig.acceleration_rps2)
    return cal


class Alone:
    """A controller thrown alone, with the interface FaultSession reads (`reference`)."""

    def __init__(self, controller):
        self.reference = controller

    def __getattr__(self, name):
        return getattr(self.reference, name)


def throws(cal, rig, fault_of, count: int, target: float, tolerance: float, zero_margin: float):
    """Each throw of the reference with its own fault. Returns [(result, fault)]."""
    reference = staged.Staged(cal, rig.acceleration_rps2)
    reference.zero_margin = zero_margin
    session = FaultSession(rig, SEED + 3)
    out = []
    for _ in range(count):
        fault = fault_of()
        result = session.throw_with(Alone(reference), target, tolerance, fault)
        out.append((result, fault))
    return out


def judge(name: str, rows: list, want, refuse=()) -> list:
    """The case's failures, in words; each throw's line printed."""
    failures = []
    for i, (result, fault) in enumerate(rows):
        delay = (f", stopped {fault.stopped_at - fault.at:.3f} s after the fault"
                 if fault.at is not None and fault.stopped_at is not None else "")
        print(f"  {name} {i}: {result}{delay}")
        if want and result not in want:
            failures.append(f"{name} {i}: ended {result}, wanted {'/'.join(want)}")
        if result in refuse:
            failures.append(f"{name} {i}: ended {result}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cases", default=",".join(CASES))
    parser.add_argument("--throws", type=int, default=10)
    args = parser.parse_args()
    cases = args.cases.split(",")
    rig = rigs.get_rig(RECORD)
    tolerance = VERDICT_TOLERANCE_GR
    digit = rig.scale.digit_gr
    band = tolerance - digit / 2.0
    cal = calibrated(rig, digit, band)
    ordinary = ("good", "over", "short")
    failures = []

    def run(name, fault_of, want, refuse=(), rig_=rig, cal_=cal, target=TARGET_GR, tol=tolerance, count=None,
            margin=ZERO_MARGIN):
        print(f"{name}:")
        failures.extend(judge(name, throws(cal_, rig_, fault_of, count or args.throws, target, tol, margin),
                              want, refuse))

    if "none" in cases:
        run("none", lambda: Fault("none"), ordinary)
    if "cup_lifted" in cases:
        run("cup_lifted coarse", lambda: Fault("cup_lifted", when="coarse"), ("cup",))
        run("cup_lifted creep", lambda: Fault("cup_lifted", when="creep"), ("cup",))
    if "light_cup" in cases:
        run("light_cup", lambda: Fault("light_cup", cup=LIGHT_CUP_G * GRAINS_A_GRAM), ("cup",))
    if "light_cup_limit" in cases:
        run("light_cup_limit", lambda: Fault("light_cup", cup=LIMIT_CUP_GR), ())
    if "drift" in cases:
        run("drift", lambda: Fault("drift", digit=digit), (), refuse=("cup",))
    if "no_powder" in cases:
        run("no_powder empty", lambda: Fault("no_powder", when="start"), ("no_powder",))
        run("no_powder creep", lambda: Fault("no_powder", when="creep"), ("no_powder",))
    if "no_powder_drift" in cases:
        run("no_powder_drift", lambda: Fault("no_powder", when="start", digit=digit, drift=True), ("no_powder",))
    if "coarse_jam" in cases:
        run("coarse_jam", lambda: Fault("coarse_jam", when="coarse"), ())
    if "third" in cases:
        fine = rig.fine
        thinner = dataclasses.replace(fine, gr_per_turn=tuple((s, g / 3.0) for s, g in fine.gr_per_turn))
        run("third", lambda: Fault("none"), (), refuse=("limit", "no_powder"), rig_=dataclasses.replace(rig, fine=thinner))
    if "far_over" in cases:
        run("far_over", lambda: Fault("far_over", digit=digit, tolerance=tolerance), ("limit",))
    if "grams" in cases:
        # Every weight the controller sees in grams: the scale's digit 0.001 g, the target, the tolerance, the cup
        original = (throw.ScaleState, bench_twin.ScaleState)
        GramScaleState.inner_class = original[0]
        throw.ScaleState = bench_twin.ScaleState = GramScaleState
        try:
            g_digit, g_tol = 0.001, round(tolerance / GRAINS_A_GRAM, 3)
            g_cal = calibrated(rig, g_digit, g_tol - g_digit / 2.0)
            g_target = round(TARGET_GR / GRAINS_A_GRAM, 3)
            run("grams none", lambda: Fault("none", unit=GRAINS_A_GRAM), ordinary, cal_=g_cal, target=g_target,
                tol=g_tol)
            run("grams cup_lifted", lambda: Fault("cup_lifted", unit=GRAINS_A_GRAM, cup=CUP_GR / GRAINS_A_GRAM),
                ("cup",), cal_=g_cal, target=g_target, tol=g_tol)
            run("grams light_cup", lambda: Fault("light_cup", unit=GRAINS_A_GRAM, cup=LIGHT_CUP_G), ("cup",), cal_=g_cal,
                target=g_target, tol=g_tol)
            run("grams far_over", lambda: Fault("far_over", unit=GRAINS_A_GRAM, digit=g_digit, tolerance=g_tol,
                                                target=g_target),
                ("limit",), cal_=g_cal, target=g_target, tol=g_tol)
        finally:
            throw.ScaleState, bench_twin.ScaleState = original
    for failure in failures:
        print("FAIL", failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
