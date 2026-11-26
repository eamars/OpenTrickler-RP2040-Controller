"""The deadlock's root fix tried on the twin: the carry seeded with step 1's own weight a turn (the prime's stream rate,
which the powder part measures and today drops), on the bench's baseline profile. Against no seed, as built."""
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import calibration  # noqa: E402
import rigs  # noqa: E402
import step2  # noqa: E402
from bench_twin import TwinBench  # noqa: E402
from step2_nocarry import BENCH, RECORDS  # noqa: E402
from throw import Session  # noqa: E402
from workers import Workers  # noqa: E402

THROWS = 50


def stream_k(rig_name):
    """Step 1's fine tube's weight a turn where it streams, as its powder part measures it on a fresh bench."""
    report = calibration.speeds_part(TwinBench(rigs.get_rig(rig_name), 0), calibration.Cup(), "fine", 0.01, 5.0)
    return report.records.get("stream_gr_turn")


def run(job):
    rig_name, target, seed_k, seed = job
    rig = rigs.get_rig(rig_name)
    session = Session(rig, seed)
    c = step2.Step2(BENCH, RECORDS, rig.acceleration_rps2, before="last")
    if seed_k is not None:
        c.history = [seed_k]
    rows = []
    for _ in range(THROWS):
        had = c.carried() is not None
        result = step2.throw_one(session, c, target, BENCH.fine_stop_threshold)
        rows.append((result.true_error, result.elapsed_s, had, bool(c.rows)))
    return job, rows


def summary(rows, tol):
    errors = [e for e, _, _, _ in rows]
    times = sorted(t for _, t, _, _ in rows)
    good = sum(abs(e) < tol for e in errors) / len(errors)
    over = sum(e >= tol for e in errors) / len(errors)
    under = sum(e <= -tol for e in errors) / len(errors)
    median = times[len(times) // 2]
    cost = median / good if good else float("inf")
    return (f"good {good * 100:3.0f}%  over {over * 100:3.0f}%  under {under * 100:3.0f}%  mean {statistics.fmean(errors):+.3f}"
            f"  sd {statistics.pstdev(errors):.3f}  time {median:4.1f} s  cost {cost:5.1f} s  "
            f"with a weight a turn {sum(h for _, _, h, _ in rows)}/{len(rows)}, quiet reached {sum(q for *_, q in rows)}")


def main():
    names = ("pico2w_ar2209_night", "pico2w_ar2209_f2", "pico2w_bm2")
    with Workers() as pool:
        ks = dict(zip(names, pool.map(stream_k, list(names))))
        jobs = [(rig, target, seed_k, 31) for rig in names for target in (10.0, 30.0)
                for seed_k in (None, ks[rig])]
        for (rig, target, seed_k, _), rows in pool.map(run, jobs):
            truth = rigs.get_rig(rig).fine.gr_per_turn
            label = "as built      " if seed_k is None else f"seeded {seed_k:.3f}  "
            print(f"{rig:22s} {target:4.0f} gr  {label} {summary(rows, BENCH.fine_stop_threshold)}"
                  + ("" if seed_k is None else f"   (the rig's own: {truth})"))


if __name__ == "__main__":
    main()
