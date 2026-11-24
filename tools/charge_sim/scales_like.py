"""Other scales to try a controller on: the measured scale made slower. Priors, not any make.

No scale but the bench's A&D has been measured, and the owner will not measure each of the nine the firmware reads
(2026-11-21): a controller must find a scale's delays by its own calibration. These stand in for scales it has
never met. Each takes a rig's record and stretches every delay in it: the delay all readings share, the further
lag that depends on the flow, how long a single small change is held back, and how often and how late a reading
comes. The controller is calibrated again on the stretched rig, as a unit would be on that scale.
"""

import dataclasses

# name: (every lag times this, a small change held this long in s, a reading every this many s, each this old in s)
SCALES = {
    "measured": None,
    # the measured scale with every small change held 2.5 s: the record's scale shows nine small moves in ten
    # within 2 to 2.5 s, the bench's took 4 s (ADR-009's legacy/progress.md, 2026-11-21), and this is later than the
    # bench's
    "late_small": (1.0, 2.5, None, None),
    # as late as measured for a stream, but a single small change held 5 s; four readings a second, each a quarter
    # second old: the owner's account of his G&G (a stream is shown far sooner than one small change)
    "holds": (1.0, 5.0, 0.25, (0.235, 0.250)),
    "slow": (2.0, 3.0, 0.25, (0.235, 0.250)),       # twice as late; four readings a second, each a quarter second old
    "slower": (3.0, 5.0, 0.50, (0.016, 0.036)),     # three times as late; a small change held 5 s; two readings a second
}


def like(rig, name: str):
    """The rig with its scale's delays stretched as the named prior says."""
    if SCALES[name] is None:
        return rig
    factor, hold_s, report_s, link = SCALES[name]
    scale = dataclasses.replace(rig.scale, dead_s=rig.scale.dead_s * factor, hold_s=hold_s)
    if report_s is not None:
        scale = dataclasses.replace(scale, report_s=report_s, link_min_s=link[0], link_max_s=link[1])
    tubes = {tube: dataclasses.replace(getattr(rig, tube), late_s=tuple((speed, late * factor)
                                                                        for speed, late in getattr(rig, tube).late_s))
             for tube in ("coarse", "fine")}
    return dataclasses.replace(rig, scale=scale, **tubes)
