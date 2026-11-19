"""The controllers under comparison, each made for a rig: name -> function(rig) -> (controller, verdict tolerance).

The verdict's tolerance is the firmware's for every controller, so they are judged alike: over at 0.03 gr or more
above the target on the reading a second after the stop.
"""

from calibrate import calibrate
from flow_dose import DoseSettings, FlowDose
from legacy_pid import AR2209_PICO2W, LegacyPid
from throw import Session

VERDICT_TOLERANCE_GR = 0.03
CALIBRATION_SEED = 4242


def make_legacy(rig) -> tuple:
    """Today's loop with the bench unit's stored AR2209 profile, whatever powder or scale the rig has."""
    return LegacyPid(AR2209_PICO2W), VERDICT_TOLERANCE_GR


def make_flow_dose(rig, commit: bool = False) -> tuple:
    """The candidate, calibrated on the rig as a unit would be: it knows the powder only by its kernel mass."""
    calibration = calibrate(Session(rig, CALIBRATION_SEED))
    return FlowDose(calibration, DoseSettings(kernel_gr=rig.powder.kernel_gr, commit=commit)), VERDICT_TOLERANCE_GR


def make_flow_dose_commit(rig) -> tuple:
    """The candidate with an account of the powder on its way and a retract. Tried 2026-11-19: no better."""
    return make_flow_dose(rig, commit=True)


CONTROLLERS = {"legacy": make_legacy, "flow_dose": make_flow_dose, "flow_dose_commit": make_flow_dose_commit}
