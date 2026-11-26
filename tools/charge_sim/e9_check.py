"""E9's second and the verdict (ADR-009, gate F.5; the manager, 2026-11-24): how often a throw's verdict changes its
class (good, over, short) in the second after it is given, against how often a settled reading changes class by noise.

Every stand-in scale (`f4_cases.stand_in`), a calibrated unit each (`staged.calibrate`), the controller as the unit runs
it since F.5 (the PID law on the default AR2209 profile's gains), sessions of throws at 10 gr. For each throw:
- the verdict: the controller's own, on its settled reading, given at once (E9 gone);
- a second later: the reading then, judged by the same edge;
- the baseline: the readings one and two seconds after, judged alike: what a settled reading's noise alone changes.

Pass, written first (adr009_controller's progress.md, 2026-11-24): the verdict changes class within its second no more
often than the baseline beyond the noise (two SDs of the difference of two shares). Else the verdict waits for the
frame's rest rule.

    python e9_check.py [--sessions 6] [--throws 20]
"""

import argparse
import math
import sys

import f4_cases
import staged
from throw import Session
from workers import Workers

TARGET_GR = 10.0


def reading_at(rows, t_s):
    """The last reading the scale gave at or before t_s."""
    last = None
    for t, reading, *_ in rows:
        if t > t_s:
            break
        if reading is not None:
            last = reading
    return last


def job(args: tuple) -> list:
    name, seed, count, stages = args
    rig = f4_cases.stand_in(name)
    digit = rig.scale.digit_gr
    band = f4_cases.TOLERANCE_GR - digit / 2.0
    if stages:
        # L (gate 3): the forecast in the stages its calibration fits, the verdict waiting for its powder
        import l_study
        cal, tables = l_study.calibrated_by_flow(rig, seed, band)
        controller = l_study.StagedByFlow(cal, rig.acceleration_rps2, tables)
    else:
        cal = staged.calibrate(rig, seed, "glide", band)
        controller = staged.Staged(cal, rig.acceleration_rps2)
    controller.pid = staged.profile_pid()
    session = Session(rig, seed + 300)
    edge = cal.band_gr + staged.BAND_EDGE_DIGITS * digit

    def cls(weight):
        error = weight - TARGET_GR
        return "over" if error > edge else "short" if -error > edge else "good"

    out = []
    for _ in range(count):
        r = session.throw(controller, TARGET_GR, f4_cases.TOLERANCE_GR, record=True)
        if controller.result not in ("good", "over", "short"):
            continue
        stop = r.elapsed_s
        one, two = reading_at(r.rows, stop + 1.0), reading_at(r.rows, stop + 2.0)
        out.append((controller.result, cls(one), cls(two)))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sessions", type=int, default=6)
    parser.add_argument("--throws", type=int, default=20)
    parser.add_argument("--stages", action="store_true", help="L's forecast in stages and its verdict (gate 3)")
    args = parser.parse_args()
    jobs = [(name, seed, args.throws, args.stages) for name in f4_cases.names() for seed in range(args.sessions)]
    with Workers() as pool:
        results = pool.map(job, jobs)
    total_changed = total_noise = total = 0
    per_scale_fail = []
    for name in f4_cases.names():
        rows = [row for j, r in zip(jobs, results) if j[0] == name for row in r]
        changed = sum(1 for v, one, _ in rows if v != one)
        noise = sum(1 for _, one, two in rows if one != two)
        kinds = {}
        for v, one, _ in rows:
            if v != one:
                kinds[f"{v}>{one}"] = kinds.get(f"{v}>{one}", 0) + 1
        heavier = sum(1 for v, one, _ in rows if (v, one) in (("good", "over"), ("short", "good"), ("short", "over")))
        noise_heavier = sum(1 for _, one, two in rows if (one, two) in (("good", "over"), ("short", "good"),
                                                                         ("short", "over")))
        n = max(1, len(rows))
        p = max(noise_heavier / n, 1.0 / n)
        bar_scale = 2.0 * math.sqrt(2.0 * p * (1.0 - p) / n)
        scale_ok = (heavier - noise_heavier) / n <= bar_scale
        per_scale_fail.extend([] if scale_ok else [name])
        print(f"{name:10s} {len(rows)} throws: the verdict's class changed in its second {changed} ({kinds}); "
              f"a settled reading's, second to second, {noise}; to a heavier class {heavier} against the noise's "
              f"{noise_heavier}: {'ok' if scale_ok else 'MORE'}", flush=True)
        total_changed += changed
        total_noise += noise
        total += len(rows)
    p = max(total_noise / total, 1.0 / total)
    bar = 2.0 * math.sqrt(2.0 * p * (1.0 - p) / total)
    worse = (total_changed - total_noise) / total > bar
    print(f"all: {total} throws; changed in the verdict's second {total_changed} ({100 * total_changed / total:.1f}%), "
          f"by noise {total_noise} ({100 * total_noise / total:.1f}%); bar {100 * bar:.1f} points")
    print(f"per scale, one-way (to a heavier class) beyond the noise on: {per_scale_fail or 'none'}")
    print("VERDICT PASS: E9's removal stands" if not worse else
          "VERDICT FAIL: the verdict changes class measurably in its second; it waits for the frame's rest rule")
    return 0 if not worse else 1


if __name__ == "__main__":
    sys.exit(main())
