"""The rigs the research runs on: the tubes ADR-009 uses, the powders, and their combinations.

Tube geometry is measured from the hardware repository's STLs (large_rotary_tube.stl, small_rotary_tube_low_flow.stl).
Every flow figure is a prior, marked as such; the bench's open-loop runs replace them.
"""

import math

from plant import GRAVITY_M_S2, Powder, Tube
from scale import SCALES
from throw import Rig


def critical_rps(bore_radius_mm: float) -> float:
    """Speed at which the wall's centripetal acceleration equals gravity: sqrt(g / R) / (2 pi)."""
    return math.sqrt(GRAVITY_M_S2 / (bore_radius_mm / 1000.0)) / (2.0 * math.pi)


# The large tube: bore radius 7.5 mm in the grooves, open at the rear, so its inlet keeps up with any speed. Priors:
# about 20 gr held and 12 gr delivered a turn (the firmware's own simulated scale says 5); near its 5 rev/s limit
# the bed is thrown across the bore and the flow per turn falls.
LARGE_TUBE = Tube(name="large", out_per_rev=0.6, in_per_rev=0.6, held_max_gr=40.0, max_inflow_gr_s=math.inf,
                  critical_rps=critical_rps(7.5), centrifuge_power=6.0, ripple=0.10, slump_deg=3.0, roll_rps=0.2,
                  start_turns=0.5, retreat_turns=0.5, trail_kernels=6.0, trail_ref_rps=0.5, trail_tau_s=0.30,
                  drop_mm=50.0)

# The small low-flow tube: bore radius 3.74 mm, fed once a turn through one window about 3 mm by 55 degrees, which
# caps what it can pass however fast it turns. 5 gr held, 1.35 gr delivered a turn while the inlet keeps up, and a
# ceiling of 2.4 gr/s: three times the first priors, set so that today's loop falls after its stop as PR #113
# measured on a unit (pr113_check.py: fine Kp 6 gives +0.19 gr here, +0.156 measured; Kp 3 +0.04, 0 to +0.03).
SMALL_LOW_FLOW_TUBE = Tube(name="small low flow", out_per_rev=0.27, in_per_rev=0.15, held_max_gr=8.0,
                           max_inflow_gr_s=2.4, critical_rps=critical_rps(3.74), centrifuge_power=6.0, ripple=0.30,
                           slump_deg=3.0, roll_rps=0.2, start_turns=0.2, retreat_turns=0.5, trail_kernels=0.5,
                           trail_ref_rps=0.2, trail_tau_s=0.30, drop_mm=50.0)

# AR2209 (H4350): a short-cut stick. Kernel mass and clumping are priors until the powder research and the bench.
AR2209 = Powder(name="AR2209", kernel_gr=0.022, kernel_cv=0.12, clump_mean=1.3, regularity=2.0)

# Stand-ins for the other shape classes, to see what a controller does when the powder changes under it.
FINE_BALL = Powder(name="fine ball", kernel_gr=0.004, kernel_cv=0.25, clump_mean=3.0, regularity=4.0, flow_scale=1.5)
LONG_STICK = Powder(name="long stick", kernel_gr=0.040, kernel_cv=0.10, clump_mean=1.8, regularity=1.2, flow_scale=0.7)

POWDERS = {"ar2209": AR2209, "ball": FINE_BALL, "long_stick": LONG_STICK}

# --- The bench's unit as measured on 2026-11-19 and 20 (fit/bench_pico2w.json; `python fit_report.py` sets the
# simulation beside every figure). Pico 2 W, the large tube and the low-flow small tube, the owner's AR2209 long
# exposed to air, the A&D FX-300i in grains at FAST. Each number below is fitted to those recordings.
BENCH_LARGE_TUBE = Tube(name="large, bench", out_per_rev=0.38, in_per_rev=0.6, held_max_gr=40.0,
                        max_inflow_gr_s=math.inf, critical_rps=critical_rps(7.5), centrifuge_power=6.0, ripple=0.10,
                        slump_deg=3.0, roll_rps=0.2, start_turns=0.5, retreat_turns=0.5, trail_kernels=6.0,
                        trail_ref_rps=0.5, trail_tau_s=0.30, drop_mm=50.0)
BENCH_SMALL_TUBE = Tube(name="small low flow, bench", out_per_rev=0.0975, in_per_rev=10.0, held_max_gr=8.0,
                        max_inflow_gr_s=math.inf, critical_rps=critical_rps(3.74), centrifuge_power=6.0, ripple=0.15,
                        slump_deg=30.0, roll_rps=0.05, start_turns=0.2, retreat_turns=0.5, trail_kernels=1.5,
                        trail_ref_rps=0.08, trail_tau_s=0.30, drop_mm=50.0, roll_tau_s=1.5, trail_full_turns=0.05)
BENCH_AR2209 = Powder(name="AR2209, long in the air", kernel_gr=0.021, kernel_cv=0.12, clump_mean=1.15,
                      regularity=2.0)
BENCH_PICO2W = Rig(powder=BENCH_AR2209, coarse=BENCH_LARGE_TUBE, fine=BENCH_SMALL_TUBE, scale=SCALES["and_bench"])


def make_rig(powder: str = "ar2209", scale: str = "and_fast") -> Rig:
    """The ADR-009 unit with a powder and a scale by their short names."""
    return Rig(powder=POWDERS[powder], coarse=LARGE_TUBE, fine=SMALL_LOW_FLOW_TUBE, scale=SCALES[scale])


# The rigs by name. "reference" is the research's first plant, made before any bench session: kept to show what the
# refit changed, not to be quoted.
RIGS = {"reference": make_rig(), "bench_pico2w": BENCH_PICO2W}
