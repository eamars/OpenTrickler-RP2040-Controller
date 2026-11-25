"""ADR-009 step 2 on the twin: each throw's reading against time, drawn as the bench's sheet is (bench/step2_traces.py).

The owner (2026-11-26): "Can you plot the result of the simulation of charge weight versus time diagram similar to
what you plotted last night?" Two sheets:

- `ab`: the bench's A/B of 2026-11-26 thrown on a rig of the twin (kp 2 and 1.33 in ABBA order from the bench's
  stored gains, the seed on, the tuner off), to lay beside the bench's own sheet;
- `tuner`: the tuner as it ships (kp and kd, the share modelled) from the bench's stored gains, its first window
  against its last after a session of throws, a row a rig.

The twin's reading is its scale's (its rhythm, its digit and its lag), as the A&D's on the bench.

    venv\\Scripts\\python tools\\charge_sim\\step2_twin_traces.py ab --rig pico2w_ar2209_f2 --out <png>
    venv\\Scripts\\python tools\\charge_sim\\step2_twin_traces.py tuner --rigs pico2w_ar2209_f2 pico2w_ar2209_night --out <png>
"""

import argparse
import dataclasses
import sys
from pathlib import Path

import rigs
import step2
import step2_tuner
from step2_variants import BENCH
from throw import Session
from workers import Workers

sys.path.insert(0, str(Path(__file__).parent / "bench"))

import step2_traces  # noqa: E402  (bench/step2_traces.py: the bench sheet's drawing)

TARGET = 10.0
KP = {"A": 2.0, "B": 1.3333}
ORDER = "ABBA"
STEP_S = 0.02                       # the trace kept every 20 ms (the twin's rows are a millisecond apart)


def thrown(session, controller) -> tuple:
    """One throw on the twin, step2.throw_one with its rows kept: (trace, the stop's s, the final weight, target)."""
    result = session.throw(controller, TARGET, BENCH.fine_stop_threshold, record=True,
                           verdict_after_s=controller.verdict_after_s)
    controller.at_rest(result.reading)
    trace, next_s = [], 0.0
    for t, reading, *_ in result.rows:
        if reading is not None and t >= next_s:
            trace.append((t, reading))
            next_s = t + STEP_S
    return trace, result.elapsed_s, result.reading, TARGET


def ab_case(job: tuple) -> list:
    rig_name, throws, seed, records = job
    rig = rigs.get_rig(rig_name)
    session = Session(rig, seed)
    controller = step2.Step2(BENCH, records, rig.acceleration_rps2, before="last", seed=True, hold="none")
    out = []
    for n in range(throws):
        arm = ORDER[n % len(ORDER)]
        controller.profile = dataclasses.replace(BENCH, fine_kp=KP[arm])
        out.append((arm, thrown(session, controller)))
    return out


def tuner_case(job: tuple) -> tuple:
    rig_name, throws, seed, records = job
    rig = rigs.get_rig(rig_name)
    profile = BENCH
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none")
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0, axes="kp+kd", share="modelled")
    windows, current, errors, times = [], [], [], []
    for _ in range(throws):
        controller.profile = profile
        one = thrown(session, controller)
        current.append(one)
        errors.append(one[2] - TARGET)
        times.append(one[1])
        if len(current) == step2_tuner.WINDOW:
            windows.append(((profile.fine_kp, profile.fine_kd), current))
            profile, _ = tuner(profile, errors, times, k=controller.carried())
            current, errors, times = [], [], []
    return rig_name, windows, (profile.fine_kp, profile.fine_kd)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sheet", choices=("ab", "tuner"))
    parser.add_argument("--rig", default="pico2w_ar2209_f2")
    parser.add_argument("--rigs", nargs="+", default=["pico2w_ar2209_f2", "pico2w_ar2209_night"])
    parser.add_argument("--throws", type=int, default=600, help="the tuner's session")
    parser.add_argument("--seed", type=int, default=9400)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    if args.sheet == "ab":
        records = step2.rig_records(args.rig)
        found = ab_case((args.rig, 48, args.seed, records))
        panels = [(f"fine kp {KP[arm]:g}", [one for a, one in found if a == arm]) for arm in KP]
        step2_traces.draw(panels, args.out, f"ADR-009 step 2 on the twin ({args.rig}): the bench's A/B, 10 gr throws "
                                            "(dashed: slower than the median)", ylabel="the twin's scale, gr")
        print(f"{len(found)} throws drawn into {args.out}")
        return
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        results = pool.map(tuner_case, [(rig, args.throws, args.seed, made[rig]) for rig in args.rigs])
    panels = []
    for rig_name, windows, kept in results:
        (kp0, kd0), first = windows[0]
        (kp1, kd1), last = windows[-1]
        panels.append((f"{rig_name}, window 1 on kp {kp0:.2f} kd {kd0:.0f}", first))
        panels.append((f"window {len(windows)} on kp {kp1:.2f} kd {kd1:.0f}", last))
        print(f"{rig_name}: {len(windows)} windows, the kept gains at the end kp {kept[0]:.3f}, kd {kept[1]:.0f}")
    step2_traces.draw(panels, args.out, "ADR-009 step 2 on the twin: the tuner as it ships, from the bench's stored "
                                        f"gains, its first window and its last of {args.throws} throws "
                                        "(dashed: slower than the median)", rows=len(results),
                      ylabel="the twin's scale, gr")
    print(f"drawn into {args.out}")


if __name__ == "__main__":
    main()
