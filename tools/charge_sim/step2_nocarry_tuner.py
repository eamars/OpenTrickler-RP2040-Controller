"""From the bench's baseline profile, the tuner's windows on the twin: does it reach the forecast by itself?"""
import dataclasses
import math
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import rigs  # noqa: E402
import step2  # noqa: E402
import step2_tuner  # noqa: E402
from step2_nocarry import BENCH, RECORDS  # noqa: E402
from throw import Session  # noqa: E402
from workers import Workers  # noqa: E402


def case(job):
    rig_name, target, throws, seed, start_kp = job
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(BENCH, fine_kp=start_kp)
    session = Session(rig, seed)
    c = step2.Step2(profile, RECORDS, rig.acceleration_rps2, before="last")
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0)
    errors, times, windows, carried_throws = [], [], [], 0
    for _ in range(throws):
        c.profile = profile
        result = step2.throw_one(session, c, target, profile.fine_stop_threshold)
        errors.append(result.true_error)
        times.append(result.elapsed_s)
        carried_throws += c.carried() is not None
        if len(errors) == step2_tuner.WINDOW:
            cost, good = step2_tuner.cost(errors, times, profile.fine_stop_threshold)
            over = sum(e >= profile.fine_stop_threshold for e in errors) / len(errors)
            thrown = profile.fine_kp
            profile, note = tuner(profile, errors, times)
            windows.append((thrown, cost, good, over, statistics.median(times), carried_throws, note))
            errors, times, carried_throws = [], [], 0
    return rig_name, target, start_kp, windows


def main():
    jobs = [(rig, target, 300, 21, kp) for rig in ("pico2w_ar2209_night", "pico2w_ar2209_f2") for target in (10.0,)
            for kp in (2.0, 0.5)]
    with Workers() as pool:
        for rig_name, target, start_kp, windows in pool.map(case, jobs):
            print(f"{rig_name}, {target:g} gr, from fine kp {start_kp} (the bench's profile otherwise):")
            for n, (kp, cost, good, over, time, carried, note) in enumerate(windows):
                c = "inf" if math.isinf(cost) else f"{cost:5.1f}"
                print(f"  window {n:2d} kp {kp:.3f}: {c} s a good charge, good {good * 100:3.0f}%, over "
                      f"{over * 100:3.0f}%, time {time:4.1f} s, throws with a carry {carried:2d}  ({note})")


if __name__ == "__main__":
    main()
