"""ADR-009 step 2 on the twin: each throw's reading against time, drawn as the bench's sheet is (bench/step2_traces.py).

The owner (2026-11-26): "Can you plot the result of the simulation of charge weight versus time diagram similar to
what you plotted last night?" Two sheets:

- `ab`: the bench's A/B of 2026-11-26 thrown on a rig of the twin (kp 2 and 1.33 in ABBA order from the bench's
  stored gains, the seed on, the tuner off), to lay beside the bench's own sheet;
- `tuner`: the tuner as it ships (kp and kd, the share modelled) from the bench's stored gains, its first window
  against its last after a session of throws, a row a rig;
- `cut`: what stops the fine tube (step2_cut_study.py): the forecast (as built), the reading (never cut early), and
  no forecast at all, from fixed gains, a row a start (the bench's stored gains, the owner's).

The twin's reading is its scale's (its rhythm, its digit and its lag), as the A&D's on the bench.

    venv\\Scripts\\python tools\\charge_sim\\step2_twin_traces.py ab --rig pico2w_ar2209_f2 --out <png>
    venv\\Scripts\\python tools\\charge_sim\\step2_twin_traces.py tuner --rigs pico2w_ar2209_f2 pico2w_ar2209_night --out <png>
"""

import argparse
import dataclasses
import statistics
import sys
from pathlib import Path

import rigs
import step2
import step2_tuner
from legacy_pid import AR2209_PICOW_168
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


def cut_case(job: tuple) -> tuple:
    """A session from fixed gains with one of step2.Step2's stops (`cut`)."""
    rig_name, profile, cut, throws, seed, records = job
    rig = rigs.get_rig(rig_name)
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none", cut=cut)
    return cut, [thrown(session, controller) for _ in range(throws)]


STARTS = {"bench": BENCH, "owner": AR2209_PICOW_168}


def tuner_case(job: tuple) -> tuple:
    """The product as it ships (the throw never cut on the forecast, the tuner on kp and kd) from a start's gains."""
    rig_name, throws, seed, records, start = job
    rig = rigs.get_rig(rig_name)
    profile = STARTS[start]
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none",
                             cut="threshold")
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0, axes="kp+kd", share="counted",
                                    objective="tolerance")
    windows, current, errors, times = [], [], [], []
    for _ in range(throws):
        controller.profile = profile
        one = thrown(session, controller)
        current.append(one)
        errors.append(one[2] - TARGET)
        times.append(one[1])
        if len(current) == step2_tuner.WINDOW:
            windows.append(((profile.fine_kp, profile.fine_kd), current))
            profile, _ = tuner(profile, errors, times, k=controller.carried(),
                               lag=statistics.median(lag for _, lag in records.lag))
            current, errors, times = [], [], []
    return f"{rig_name} from the {start}'s gains", windows, (profile.fine_kp, profile.fine_kd)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sheet", choices=("ab", "tuner", "cut"))
    parser.add_argument("--rig", default="pico2w_ar2209_f2")
    parser.add_argument("--rigs", nargs="+", default=["pico2w_ar2209_f2", "pico2w_ar2209_night"])
    parser.add_argument("--throws", type=int, default=600, help="the tuner's session")
    parser.add_argument("--starts", nargs="+", default=["bench"], choices=list(STARTS))
    parser.add_argument("--seed", type=int, default=9400)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    if args.sheet == "cut":
        records = step2.rig_records(args.rig)
        starts = {"bench's gains": BENCH, "owner's gains": AR2209_PICOW_168}
        cuts = {"forecast": "cut on the forecast (as built)", "reading": "never cut early: the reading",
                "none": "no forecast"}
        with Workers() as pool:
            results = pool.map(cut_case, [(args.rig, profile, cut, 40, args.seed, records)
                                          for profile in starts.values() for cut in cuts])
        names = [f"{start}, {said}" for start in starts for said in cuts.values()]
        panels = [(name, drawn) for name, (_, drawn) in zip(names, results)]
        step2_traces.draw(panels, args.out, f"ADR-009 step 2 on the twin ({args.rig}): what stops the fine tube, 10 gr "
                                            "throws (dashed: slower than the median)", rows=len(starts),
                          ylabel="the twin's scale, gr")
        print(f"drawn into {args.out}")
        return
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
        results = pool.map(tuner_case, [(rig, args.throws, args.seed, made[rig], start) for rig in args.rigs
                                        for start in args.starts])
    panels = []
    for rig_name, windows, kept in results:
        (kp0, kd0), first = windows[0]
        (kp1, kd1), last = windows[-1]
        panels.append((f"{rig_name}, window 1 on kp {kp0:.2f} kd {kd0:.0f}", first))
        panels.append((f"window {len(windows)} on kp {kp1:.2f} kd {kd1:.0f}", last))
        print(f"{rig_name}: {len(windows)} windows, the kept gains at the end kp {kept[0]:.3f}, kd {kept[1]:.0f}")
    step2_traces.draw(panels, args.out, "ADR-009 step 2 on the twin as it ships (never cut early, the tuner on kp "
                                        f"and kd): its first window and its last of {args.throws} throws "
                                        "(dashed: slower than the median)", rows=len(results),
                      ylabel="the twin's scale, gr")
    print(f"drawn into {args.out}")


if __name__ == "__main__":
    main()
