"""Gate N, rows G1 and G2 (ADR-009, numbers.md): the fallback before any calibration, on scales it was not set on.

The PID on a forecast it learns (`legacy_pid.LearntPid`, the firmware's charge_pid_learnt.c) with the owner's .168
gains, as the rig threw it. Its scale numbers are the A&D's, set by hand (lag 0.9 s, rest 2.5 s), and its prior
figure 2.0 gr a turn was raised after the rig's dry throws. Each is tried as built, at half and at twice, on both
records and the five stand-in scales of `scales_like.py`, at both bands, 10 gr, sessions of 25 throws from a fresh
figure.

The pass, written before the run (numbers.md, "The research for each row"): the fallback's numbers hold on any scale
when, as built, on every stand-in scale its first three throws of a session are over no more often than on the
measured scale, within the noise (two SDs of the difference of two shares of that many throws). Where they are over
more often, the numbers are this scale's and must be measured on each unit (a check of the scale without powder, or
the calibration).

    python fallback_study.py [--throws 3200] [--rigs ...] [--scales ...] [--out fit/...]
"""

import argparse
import dataclasses
import math
from pathlib import Path

import controllers
import mc
import rigs
import scales_like
from legacy_pid import LearntPid
from workers import Workers

BUILT = {"lag_s": 0.9, "rest_s": 2.5, "gr_turn_prior": 2.0}
BANDS = ((0.02, controllers.VERDICT_TOLERANCE_GR), (0.04, controllers.LOOSE_TOLERANCE_GR))
TARGET_GR = 10.0
FIRST = 3                       # the first throws of a session, from a fresh figure


def cases() -> list:
    out = [("built", None, None)]
    for name, value in BUILT.items():
        out += [(f"{name} x0.5", name, value * 0.5), (f"{name} x2", name, value * 2.0)]
    return out


def _make(name, value, band, tolerance):
    def make(rig, seed=0):
        kw = dict(BUILT)
        if name is not None:
            kw[name] = value
        return LearntPid(controllers.AR2209_PICOW_168_RIG, rig.scale.digit_gr, rig.scale.report_s, band,
                         acceleration_rps2=rig.acceleration_rps2, **kw), tolerance
    return make


def case_name(label, band) -> str:
    return f"fallback:{label}@{band}"


for _band, _tol in BANDS:
    for _label, _name, _value in cases():
        controllers.CONTROLLERS[case_name(_label, _band)] = _make(_name, _value, _band, _tol)


def share(results, test) -> float:
    return sum(1 for r in results if test(r)) / max(1, len(results))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=3200)
    parser.add_argument("--rigs", default="pico2w_ar2209,pico2w_bm2")
    parser.add_argument("--scales", default=",".join(scales_like.SCALES))
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    lines = [f"{args.throws} throws a case ({args.throws // mc.THROWS_PER_SESSION} sessions) at {TARGET_GR:.0f} gr; "
             f"the PID on a learnt forecast, the owner's gains; 'first' = the first {FIRST} throws of each session"]
    with Workers() as pool:
        for rig_name in args.rigs.split(","):
            for band, tolerance in BANDS:
                measured_first = None
                for scale in args.scales.split(","):
                    rig = scales_like.like(rigs.get_rig(rig_name), scale)
                    for label, _name, _value in cases():
                        jobs = mc.session_jobs(rig, case_name(label, band), TARGET_GR, args.throws, 1)
                        sessions = pool.map(mc.run_session, jobs)
                        throws = [r for s in sessions for r in s]
                        first = [r for s in sessions for r in s[:FIRST]]
                        s = mc.summarise(throws, tolerance)
                        f = mc.summarise(first, tolerance)
                        verdict = ""
                        if label == "built":
                            if scale == "measured":
                                measured_first = f["over_true"]
                            else:
                                n = len(first)
                                p = max(measured_first, 1.0 / n)
                                bar = 2.0 * math.sqrt(2.0 * p * (1.0 - p) / n)
                                worse = f["over_true"] - measured_first > bar
                                verdict = f"  first-over vs measured {(f['over_true'] - measured_first) * 100:+.1f} " \
                                          f"(noise {bar * 100:.1f}) {'MORE' if worse else 'ok'}"
                        lines.append(f"{rig_name:14s}{scale:11s}{band:5.2f} {label:18s} good {s['good_true'] * 100:5.1f}% "
                                     f"over {s['over_true'] * 100:5.1f}% median {s['time_median']:5.1f} s; first "
                                     f"{FIRST}: good {f['good_true'] * 100:5.1f}% over {f['over_true'] * 100:5.1f}%"
                                     + verdict)
                        print(lines[-1], flush=True)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
