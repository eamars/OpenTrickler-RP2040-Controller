"""The auto-tuner's offer: the PID law's gains made from the calibration, against the seeds a user may have
(ADR-009, 2026-11-22 night). The frame and its envelope as built; 300 throws a case at 10 gr, judged at 0.04 gr.

The offer's coarse tube: kp 1 / (coarse grains a turn x coarse glide time), the frame's coarse floor and top (as options
1 and 2 had it). Its fine tube, three rules tried: the glide's own gain, 1 / (fine grains a turn x glide time); that
gain over the longest fine lag in place of the glide time (0.6 of it); and half the glide's gain. ki and kd 0.

    python offer_study.py [--rigs ...] [--out fit/...]
"""

import argparse
import dataclasses
from pathlib import Path

import bad_gains_study as bg
import controllers
import mc
import rigs
import staged
from workers import Workers

FINE_RULES = {
    "glide gain": lambda cal: 1.0 / (cal.fine_gr_turn * cal.glide_s),
    "0.6 x glide gain": lambda cal: 0.6 / (cal.fine_gr_turn * cal.glide_s),
    "half the glide gain": lambda cal: 0.5 / (cal.fine_gr_turn * cal.glide_s),
}


def offer(cal, fine_rule: str) -> staged.PidLaw:
    """The auto-tuner's gains for a calibration."""
    return staged.PidLaw(coarse_kp=1.0 / (cal.coarse_gr_turn * cal.coarse_glide_s), coarse_ki=0.0, coarse_kd=0.0,
                         coarse_min_rps=staged.COARSE_FLOOR_RPS, coarse_max_rps=staged.COARSE_TOP_RPS,
                         fine_kp=FINE_RULES[fine_rule](cal), fine_ki=0.0, fine_kd=0.0,
                         fine_max_rps=staged.FINE_TOP_RPS)


def _make(law_of):
    def make(rig, seed=0):
        cal = staged.calibrate(rig, seed, "glide", 0.04)
        controller = staged.Staged(cal, rig.acceleration_rps2)
        controller.pid = law_of(cal)
        return controller, controllers.LOOSE_TOLERANCE_GR
    return make


CASES = [("seed: the owner's", "good (option 2)"), ("seed: the bench unit's stored", "the bench unit's stored"),
         ("seed: sluggish", "sluggish")]
for _label, _set in CASES:
    controllers.CONTROLLERS[f"offer:{_label}"] = _make(bg.GAIN_SETS[_set])
for _rule in FINE_RULES:
    controllers.CONTROLLERS[f"offer:offer, fine {_rule}"] = _make(lambda cal, r=_rule: offer(cal, r))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", default="pico2w_ar2209,pico2w_bm2")
    parser.add_argument("--targets", default="10,5")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    lines = ["300 throws a case, judged at 0.04 gr; the PID law in the staged frame, the envelope on"]
    names = ["glide_loose"] + [f"offer:{label}" for label, _ in CASES] + [f"offer:offer, fine {r}" for r in FINE_RULES]
    with Workers() as pool:
        for rig_name in args.rigs.split(","):
            rig = rigs.get_rig(rig_name)
            cal = staged.calibrate(rig, 0, "glide", 0.04)
            lines.append(f"{rig_name}: the offer's gains on its calibration (seed 0): " + ", ".join(
                f"fine kp {FINE_RULES[r](cal):.2f} ({r})" for r in FINE_RULES)
                + f"; coarse kp {1.0 / (cal.coarse_gr_turn * cal.coarse_glide_s):.3f}")
            print(lines[-1], flush=True)
            for target in (float(t) for t in args.targets.split(",")):
                for name in names:
                    s = mc.summarise(mc.run_many(rig, name, target, 300, 1, pool), controllers.LOOSE_TOLERANCE_GR)
                    label = "(the glide)" if name == "glide_loose" else name.split(":", 1)[1]
                    lines.append(f"{rig_name:15s}{target:4.0f}  {label:36s} good {s['good_true'] * 100:5.1f}% "
                                 f"over {s['over_true'] * 100:5.1f}% median {s['time_median']:5.1f} s "
                                 f"95% {s['time_p95']:5.1f} s")
                    print(lines[-1], flush=True)
    if args.out:
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
