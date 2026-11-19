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
# caps what it can pass however fast it turns. Priors: 5 gr held and 0.45 gr delivered a turn while the inlet keeps
# up, a ceiling of 0.8 gr/s (0.1 to 0.8 by a hopper-orifice estimate, the low end for sticks).
SMALL_LOW_FLOW_TUBE = Tube(name="small low flow", out_per_rev=0.09, in_per_rev=0.15, held_max_gr=8.0,
                           max_inflow_gr_s=0.8, critical_rps=critical_rps(3.74), centrifuge_power=6.0, ripple=0.30,
                           slump_deg=3.0, roll_rps=0.2, start_turns=0.2, retreat_turns=0.5, trail_kernels=0.5,
                           trail_ref_rps=0.2, trail_tau_s=0.30, drop_mm=50.0)

# AR2209 (H4350): a short-cut stick. Kernel mass and clumping are priors until the powder research and the bench.
AR2209 = Powder(name="AR2209", kernel_gr=0.022, kernel_cv=0.12, clump_mean=1.3, regularity=2.0)

# Stand-ins for the other shape classes, to see what a controller does when the powder changes under it.
FINE_BALL = Powder(name="fine ball", kernel_gr=0.004, kernel_cv=0.25, clump_mean=3.0, regularity=4.0, flow_scale=1.5)
LONG_STICK = Powder(name="long stick", kernel_gr=0.040, kernel_cv=0.10, clump_mean=1.8, regularity=1.2, flow_scale=0.7)

POWDERS = {"ar2209": AR2209, "ball": FINE_BALL, "long_stick": LONG_STICK}


def make_rig(powder: str = "ar2209", scale: str = "and_fast") -> Rig:
    """The ADR-009 unit with a powder and a scale by their short names."""
    return Rig(powder=POWDERS[powder], coarse=LARGE_TUBE, fine=SMALL_LOW_FLOW_TUBE, scale=SCALES[scale])
