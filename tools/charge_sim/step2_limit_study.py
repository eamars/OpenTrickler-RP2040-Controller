"""The tuner's time limit, on the whole throw or on the fine tube's part of it (the PR's review, T1, 2026-11-27).

The limit as first built (10 s on the whole throw's median time) bit sooner the heavier the charge: at 60 gr the
tuner put back every slower, more accurate trial as "past the time". Here the shipped tuner throws 500 times from the
bench's stored gains at 10, 42.5 and 60 gr, timed four ways: the whole throw against 10 s, and the fine tube's part
(from the coarse tube's stop to the throw's) against 10, 8 and 6 s. Each line: the first window, the last 100 throws
of four sessions, and the gains each session ended on. Kept: 8 s on the fine tube's part, the same gains as the first
build at 10 gr.

    venv\\Scripts\\python tools\\charge_sim\\step2_limit_study.py
"""
import statistics

import rigs
import step2
import step2_tuner
from step2_variants import BENCH
from throw import Session
from workers import Workers

TOL = 0.03


def run(job):
    rig_name, seed, target, limit, part, throws = job
    rig = rigs.get_rig(rig_name)
    records = step2.rig_records(rig_name)
    profile = BENCH
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none",
                             cut="threshold")
    step2_tuner.TIME_MOST_S = limit
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0, axes="kp+kd", share="counted",
                              objective="tolerance")
    lag = statistics.median(lag for _, lag in records.lag)
    s = Session(rig, seed)
    errors, times, whole, fine, out = [], [], [], [], []
    for _ in range(throws):
        controller.profile = profile
        r = step2.throw_one(s, controller, target, TOL)
        errors.append(r.error)
        f = controller.fine_part_s(r.elapsed_s)
        times.append(f if part == "fine" else r.elapsed_s)
        whole.append(r.elapsed_s)
        fine.append(f)
        if len(errors) == step2_tuner.WINDOW:
            good = sum(abs(e) < TOL - 1e-9 for e in errors) / len(errors)
            out.append((good, statistics.median(whole), statistics.median(fine), profile.fine_kp, profile.fine_kd))
            profile, _ = tuner(profile, errors, times, k=controller.carried(), lag=lag,
                               coarse_k=records.coarse_gr_turn)
            errors, times, whole, fine = [], [], [], []
    return job, out


if __name__ == "__main__":
    throws = 500
    jobs = [(rig, seed, target, limit, part, throws)
            for rig in ("pico2w_ar2209_f2", "pico2w_ar2209_night")
            for target in (10.0, 42.5, 60.0)
            for limit, part in ((10.0, "whole"), (10.0, "fine"), (8.0, "fine"), (6.0, "fine"))
            for seed in range(4)]
    with Workers() as pool:
        results = pool.map(run, jobs)
    keyed = {}
    for (rig, seed, target, limit, part, _), out in results:
        keyed.setdefault((rig, target, limit, part), []).append(out)
    for (rig, target, limit, part), runs in keyed.items():
        first = [r[0] for r in runs]
        last = [w for r in runs for w in r[-4:]]            # each session's last 100 throws
        print(f"{rig[-5:]:>5} {target:5.1f} gr, limit {limit:4.1f} s on the {part:5}: first window good "
              f"{statistics.mean(w[0] for w in first):4.0%} whole {statistics.mean(w[1] for w in first):4.1f} s "
              f"fine {statistics.mean(w[2] for w in first):4.1f} s | last 100: good "
              f"{statistics.mean(w[0] for w in last):4.0%} whole {statistics.mean(w[1] for w in last):4.1f} s fine "
              f"{statistics.mean(w[2] for w in last):4.1f} s | kp at the end "
              + " ".join(f"{r[-1][3]:.2f}/{r[-1][4]:.0f}" for r in runs))
