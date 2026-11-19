"""The controllers under comparison, each made for a rig: name -> function(rig) -> (controller, verdict tolerance).

The verdict's tolerance is the firmware's for every controller, so they are judged alike: over at 0.03 gr or more
above the target on the reading a second after the stop.
"""

import dataclasses

from calibrate import calibrate
from flow_dose import DoseSettings, FlowDose
from legacy_pid import AR2209_PICO2W, LegacyPid
from throw import Session

VERDICT_TOLERANCE_GR = 0.03
CALIBRATION_SEED = 4242


def make_legacy(rig) -> tuple:
    """Today's loop with the bench unit's stored AR2209 profile, whatever powder or scale the rig has."""
    return LegacyPid(AR2209_PICO2W), VERDICT_TOLERANCE_GR


def make_flow_dose(rig, commit: bool = False, allocation: str = "parallel") -> tuple:
    """The candidate, calibrated on the rig as a unit would be: it knows the powder only by its kernel mass."""
    calibration = calibrate(Session(rig, CALIBRATION_SEED))
    return FlowDose(calibration, DoseSettings(kernel_gr=rig.powder.kernel_gr, commit=commit,
                                              allocation=allocation)), VERDICT_TOLERANCE_GR


def make_flow_dose_commit(rig) -> tuple:
    """The candidate with an account of the powder on its way and a retract. Tried 2026-11-19: no better."""
    return make_flow_dose(rig, commit=True)


def make_flow_dose_sequential(rig) -> tuple:
    """The candidate with the fine tube waiting for the coarse to stop."""
    return make_flow_dose(rig, allocation="sequential")


def make_flow_dose_split(rig) -> tuple:
    """The candidate with one demand split between the tubes."""
    return make_flow_dose(rig, allocation="split")


def make_legacy_kp(fine_kp: float):
    """Today's loop with another fine Kp, to set against PR #113's measured fall after the stop."""
    def make(rig) -> tuple:
        return LegacyPid(dataclasses.replace(AR2209_PICO2W, fine_kp=fine_kp)), VERDICT_TOLERANCE_GR
    return make


CONTROLLERS = {"legacy": make_legacy, "flow_dose": make_flow_dose, "flow_dose_commit": make_flow_dose_commit,
               "flow_dose_sequential": make_flow_dose_sequential, "flow_dose_split": make_flow_dose_split,
               "legacy_kp3": make_legacy_kp(3.0), "legacy_kp6": make_legacy_kp(6.0)}
