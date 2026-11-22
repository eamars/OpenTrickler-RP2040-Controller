"""The controllers under comparison, each made for a rig: name -> function(rig) -> (controller, verdict tolerance).

The verdict's tolerance is the firmware's for every controller, so they are judged alike: over at 0.03 gr or more
above the target on the reading a second after the stop.
"""

import rigs
import staged
from calibrate import calibrate
from flow_dose import DoseSettings, FlowDose
from legacy_pid import AR2209_PICO2W, AR2209_PICOW_168, PICO2W_KP08, PICO2W_SLOW_FINISH, ForecastPid, GuardedPid, LegacyPid
from throw import Session

VERDICT_TOLERANCE_GR = 0.03
LOOSE_TOLERANCE_GR = 0.05     # within 0.04 gr, on a digit of 0.02 gr
CALIBRATION_SEED = 4242


def make_legacy(rig, seed: int = 0) -> tuple:
    """Today's loop with the bench unit's stored AR2209 profile, whatever powder or scale the rig has."""
    return LegacyPid(AR2209_PICO2W), VERDICT_TOLERANCE_GR


def make_flow_dose(rig, seed: int = 0, commit: bool = False, allocation: str = "parallel") -> tuple:
    """The candidate, calibrated on the rig as a unit would be: it knows the powder only by its kernel mass."""
    calibration = calibrate(Session(rig, CALIBRATION_SEED))
    return FlowDose(calibration, DoseSettings(kernel_gr=rig.powder.kernel_gr, commit=commit,
                                              allocation=allocation)), VERDICT_TOLERANCE_GR


def make_flow_dose_commit(rig, seed: int = 0) -> tuple:
    """The candidate with an account of the powder on its way and a retract. Tried 2026-11-19: no better."""
    return make_flow_dose(rig, commit=True)


def make_flow_dose_sequential(rig, seed: int = 0) -> tuple:
    """The candidate with the fine tube waiting for the coarse to stop."""
    return make_flow_dose(rig, allocation="sequential")


def make_flow_dose_split(rig, seed: int = 0) -> tuple:
    """The candidate with one demand split between the tubes."""
    return make_flow_dose(rig, allocation="split")


def make_legacy_kp08(rig, seed: int = 0) -> tuple:
    """Today's loop with the fine gain the bench's measurements give, as thrown on 2026-11-20."""
    return LegacyPid(PICO2W_KP08), VERDICT_TOLERANCE_GR


def make_legacy_slow_finish(rig, seed: int = 0) -> tuple:
    """Today's loop with that gain and a slow finish, as thrown on 2026-11-20."""
    return LegacyPid(PICO2W_SLOW_FINISH), VERDICT_TOLERANCE_GR


def make_glide(rig, seed: int = 0) -> tuple:
    """Proposal 2 of 2026-11-21: the fine tube glides to a creep and stops on a forecast; calibrated on the rig."""
    return staged.make(rig, seed, "glide", 0.02), VERDICT_TOLERANCE_GR


def make_glide_loose(rig, seed: int = 0) -> tuple:
    """Proposal 2 for the looser band, 0.04 gr."""
    return staged.make(rig, seed, "glide", 0.04), LOOSE_TOLERANCE_GR


def make_jog(rig, seed: int = 0) -> tuple:
    """Proposal 1 of 2026-11-21: the fine tube stops short from a dribble and small moves add the rest."""
    return staged.make(rig, seed, "jog", 0.02), VERDICT_TOLERANCE_GR


def make_jog_loose(rig, seed: int = 0) -> tuple:
    """Proposal 1 for the looser band, 0.04 gr."""
    return staged.make(rig, seed, "jog", 0.04), LOOSE_TOLERANCE_GR


def other_powder(rig):
    """The bench's other powder on the same scale: the record a scale's part is borrowed from in a trial."""
    return rigs.get_rig("pico2w_ar2209" if "BM2" in rig.powder.name else "pico2w_bm2")


def make_glide_borrowed(rig, seed: int = 0) -> tuple:
    """Proposal 2 with the scale's lag table measured with the bench's other powder."""
    return staged.make(rig, seed, "glide", 0.02, other_powder(rig)), VERDICT_TOLERANCE_GR


def make_jog_borrowed(rig, seed: int = 0) -> tuple:
    """Proposal 1 with the scale's lag table measured with the bench's other powder."""
    return staged.make(rig, seed, "jog", 0.02, other_powder(rig)), VERDICT_TOLERANCE_GR


def make_glide_pc_lags(rig, seed: int = 0) -> tuple:
    """Proposal 2 on a calibration whose lags are 0.055 s long, as a PC's through the console were (2026-11-22)."""
    return staged.make(rig, seed, "glide", 0.02, lag_shift_s=0.055), VERDICT_TOLERANCE_GR


def make_legacy_picow(rig, seed: int = 0) -> tuple:
    """Today's loop with the owner's hand-tuned AR2209 profile from his Pico W (192.168.2.168)."""
    return LegacyPid(AR2209_PICOW_168), VERDICT_TOLERANCE_GR


def make_legacy_picow_loose(rig, seed: int = 0) -> tuple:
    return LegacyPid(AR2209_PICOW_168), LOOSE_TOLERANCE_GR


# Proposal 2 on calibrations off as the rig's throws found the trial's (2026-11-22): the coarse tube's grains a turn
# 6% low, the fine tube's 15% high at the finish, the lags 0.055 s long, and all three at once
def make_glide_coarse_low(rig, seed: int = 0) -> tuple:
    return staged.make(rig, seed, "glide", 0.02, gr_turn_factors=(0.94, 1.0)), VERDICT_TOLERANCE_GR


def make_glide_fine_high(rig, seed: int = 0) -> tuple:
    return staged.make(rig, seed, "glide", 0.02, gr_turn_factors=(1.0, 1.15)), VERDICT_TOLERANCE_GR


def make_glide_all_off(rig, seed: int = 0) -> tuple:
    return staged.make(rig, seed, "glide", 0.02, lag_shift_s=0.055, gr_turn_factors=(0.94, 1.15)), VERDICT_TOLERANCE_GR


def make_forecast_pid_picow(rig, seed: int = 0) -> tuple:
    """The owner's Pico W gains on the staged controller's forecast, its calibration's open-loop part measured on the rig."""
    return ForecastPid(AR2209_PICOW_168, staged.measure(rig, seed), rig.acceleration_rps2), VERDICT_TOLERANCE_GR


def guarded_cal(rig, seed: int):
    return staged.for_finish(staged.measure(rig, seed), "glide", 0.02)


def make_guarded_pid(rig, seed: int = 0) -> tuple:
    """The talk of 2026-11-22: the owner's PID stream, the forecast's early stop, a top-up at the profile's least speed."""
    return GuardedPid(AR2209_PICOW_168, guarded_cal(rig, seed), rig.acceleration_rps2), VERDICT_TOLERANCE_GR


def make_guarded_pid_creep(rig, seed: int = 0) -> tuple:
    """The same, its top-up at the calibration's creep in place of the profile's least speed."""
    cal = guarded_cal(rig, seed)
    return GuardedPid(AR2209_PICOW_168, cal, rig.acceleration_rps2, creep_rps=cal.floor_rps), VERDICT_TOLERANCE_GR


def make_glide_quick_rest(rig, seed: int = 0) -> tuple:
    """The talk of 2026-11-22: proposal 2 with its rest judged when the forecast holds nothing to show."""
    return staged.make_quick_rest(rig, seed, "glide", 0.02), VERDICT_TOLERANCE_GR


def make_glide_loose_quick_rest(rig, seed: int = 0) -> tuple:
    return staged.make_quick_rest(rig, seed, "glide", 0.04), LOOSE_TOLERANCE_GR


def make_guarded_pid_forecast(rig, seed: int = 0) -> tuple:
    """The talk of 2026-11-22: the owner's PID gains on the forecast, its early stop, a top-up at the least speed."""
    return GuardedPid(AR2209_PICOW_168, guarded_cal(rig, seed), rig.acceleration_rps2, on_forecast=True),         VERDICT_TOLERANCE_GR


def make_mode_fast(rig, seed: int = 0) -> tuple:
    """The talk of 2026-11-22: the fast mode, judged at its band of 0.04 gr."""
    return staged.make_mode(rig, seed, "fast"), LOOSE_TOLERANCE_GR


def make_mode_accurate(rig, seed: int = 0) -> tuple:
    """The talk of 2026-11-22: the accurate mode, judged at its band of 0.02 gr."""
    return staged.make_mode(rig, seed, "accurate"), VERDICT_TOLERANCE_GR


# The talk of 2026-11-22: the fine tube's grains a turn learnt over a session, and the powder owed by top-ups.
# "_wrong": the calibration's fine figure 15% high, as the trial's was at the finish
def _session(band, tol, learn, owed, factor=1.0):
    return lambda rig, seed=0: (staged.make_session(rig, seed, band, learn, owed, factor), tol)


SESSION_CONTROLLERS = {}
for _band, _tol, _b in ((0.04, LOOSE_TOLERANCE_GR, "fast"), (0.02, VERDICT_TOLERANCE_GR, "acc")):
    for _factor, _f in ((1.0, ""), (1.15, "_wrong")):
        SESSION_CONTROLLERS[f"{_b}{_f}"] = _session(_band, _tol, False, False, _factor)
        SESSION_CONTROLLERS[f"{_b}{_f}_learn"] = _session(_band, _tol, True, False, _factor)
        SESSION_CONTROLLERS[f"{_b}{_f}_owed"] = _session(_band, _tol, False, True, _factor)
        SESSION_CONTROLLERS[f"{_b}{_f}_both"] = _session(_band, _tol, True, True, _factor)


CONTROLLERS = {"mode_fast": make_mode_fast, "mode_accurate": make_mode_accurate,
               "guarded_pid_forecast": make_guarded_pid_forecast, "glide_quick_rest": make_glide_quick_rest, "glide_loose_quick_rest": make_glide_loose_quick_rest,
               "guarded_pid": make_guarded_pid, "guarded_pid_creep": make_guarded_pid_creep,
               "forecast_pid_picow": make_forecast_pid_picow, "legacy_picow": make_legacy_picow, "legacy_picow_loose": make_legacy_picow_loose,
               "glide_coarse_low": make_glide_coarse_low, "glide_fine_high": make_glide_fine_high,
               "glide_all_off": make_glide_all_off,
               "glide_pc_lags": make_glide_pc_lags, "glide_borrowed": make_glide_borrowed, "jog_borrowed": make_jog_borrowed, "glide": make_glide, "glide_loose": make_glide_loose, "jog": make_jog, "jog_loose": make_jog_loose,
               "legacy": make_legacy, "legacy_kp08": make_legacy_kp08, "legacy_slow_finish": make_legacy_slow_finish,
               "flow_dose": make_flow_dose, "flow_dose_commit": make_flow_dose_commit,
               "flow_dose_sequential": make_flow_dose_sequential, "flow_dose_split": make_flow_dose_split}
CONTROLLERS.update(SESSION_CONTROLLERS)
