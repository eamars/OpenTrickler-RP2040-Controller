"""The PID law in the staged frame with gains that are bad to begin with (ADR-009, 2026-11-22 night; the owner: "consider
what if the PID value is bad to begin with").

Each set is a powder profile's PID numbers as a user might have them; the frame (its predictor, floor, stop, rest,
moves and learning) is the same for all. 300 throws a case in sessions of 25, judged at the true weight at 0.04 gr.
`--envelope` runs them with the frame's speed envelope (staged.PID_ENVELOPE) on.

    python bad_gains_study.py [--rigs pico2w_ar2209,pico2w_bm2] [--target 10] [--envelope] [--out fit/...]
"""

import argparse
import dataclasses
from pathlib import Path

import controllers
import mc
import rigs
import staged
from workers import Workers

# (label, a function of the calibration giving the PidLaw)
GAIN_SETS = {
    "good (option 2)": lambda cal: staged.pid_law_from(cal),
    "the bench unit's stored": lambda cal: staged.PidLaw(coarse_kp=0.025, coarse_ki=0.0, coarse_kd=0.25,
                                                         coarse_min_rps=0.1, coarse_max_rps=5.0, fine_kp=2.0,
                                                         fine_ki=0.0, fine_kd=10.0, fine_max_rps=3.0),
    "aggressive": lambda cal: staged.PidLaw(coarse_kp=2.0, coarse_ki=0.0, coarse_kd=0.0, coarse_min_rps=0.3,
                                            coarse_max_rps=5.0, fine_kp=10.0, fine_ki=0.0, fine_kd=0.0,
                                            fine_max_rps=5.0),
    "sluggish": lambda cal: staged.PidLaw(coarse_kp=0.01, coarse_ki=0.0, coarse_kd=0.0, coarse_min_rps=0.1,
                                          coarse_max_rps=5.0, fine_kp=0.1, fine_ki=0.0, fine_kd=0.0,
                                          fine_max_rps=2.0),
    "integral wind-up": lambda cal: dataclasses.replace(staged.pid_law_from(cal), coarse_ki=0.01, fine_ki=0.05),
    "a large derivative": lambda cal: dataclasses.replace(staged.pid_law_from(cal), coarse_kd=100.0, fine_kd=1000.0),
}


def _make(label, envelope):
    def make(rig, seed=0):
        cal = staged.calibrate(rig, seed, "glide", 0.04)
        controller = staged.Staged(cal, rig.acceleration_rps2)
        controller.pid = GAIN_SETS[label](cal)
        controller.pid_envelope = envelope
        return controller, controllers.LOOSE_TOLERANCE_GR
    return make


for _label in GAIN_SETS:
    for _env in (False, True):
        controllers.CONTROLLERS[f"bad:{_label}:{_env}"] = _make(_label, _env)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", default="pico2w_ar2209,pico2w_bm2")
    parser.add_argument("--target", type=float, default=10.0)
    parser.add_argument("--throws", type=int, default=300)
    parser.add_argument("--envelope", action="store_true")
    parser.add_argument("--both", action="store_true", help="without and with the envelope")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    envelopes = (False, True) if args.both else (args.envelope,)
    lines = [f"{args.throws} throws a case at {args.target:g} gr, judged at 0.04 gr; the PID law in the staged frame",
             f"{'rig':15s}{'gains':26s}{'envelope':>9s}{'good':>7s}{'over':>7s}{'under':>7s}{'mean':>8s}{'sd':>7s}"
             f"{'median':>8s}{'95%':>7s}"]
    print("\n".join(lines))
    with Workers() as pool:
        for rig_name in args.rigs.split(","):
            rig = rigs.get_rig(rig_name)
            glide = mc.summarise(mc.run_many(rig, "glide_loose", args.target, args.throws, 1, pool),
                                 controllers.LOOSE_TOLERANCE_GR)
            lines.append(f"{rig_name:15s}{'(the glide)':26s}{'':>9s}{glide['good_true'] * 100:6.1f}%"
                         f"{glide['over_true'] * 100:6.1f}%{glide['under_true'] * 100:6.1f}%{glide['error_mean']:+8.3f}"
                         f"{glide['error_sd']:7.3f}{glide['time_median']:7.1f}s{glide['time_p95']:6.1f}s")
            print(lines[-1], flush=True)
            for label in GAIN_SETS:
                for envelope in envelopes:
                    s = mc.summarise(mc.run_many(rig, f"bad:{label}:{envelope}", args.target, args.throws, 1, pool),
                                     controllers.LOOSE_TOLERANCE_GR)
                    lines.append(f"{rig_name:15s}{label:26s}{'on' if envelope else 'off':>9s}"
                                 f"{s['good_true'] * 100:6.1f}%{s['over_true'] * 100:6.1f}%{s['under_true'] * 100:6.1f}%"
                                 f"{s['error_mean']:+8.3f}{s['error_sd']:7.3f}{s['time_median']:7.1f}s"
                                 f"{s['time_p95']:6.1f}s")
                    print(lines[-1], flush=True)
    if args.out:
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
