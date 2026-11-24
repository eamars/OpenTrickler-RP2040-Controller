"""Where a throw's time goes, the rig's series against their replay, phase by phase (ADR-009, gate S).

The phases are bench/phase_times.py's: the bulk (to the first reading within 0.7 gr of the target), the approach (to
the fine tube's first stop), and the rest and top-ups (to the verdict). Each series is replayed as its unit ran it
(replay.py). The day's series only, the model's source.

    python phase_study.py [--rig pico2w_ar2209] [--throws 200] [--out ...]
"""

import argparse
import json
import statistics
import sys
from pathlib import Path

import controllers
import replay      # noqa: F401  registers replay:<folder>, in every worker
import replay_study
import rigs
from throw import Session
from workers import Workers

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "bench"))
import phase_times     # noqa: E402

NEAR_GR = 0.7
SESSION = 25


def simulated_phases(job: tuple) -> list:
    """(rig name, controller, target, throws, seed): each throw's phases, cut as the rig's are from its readings."""
    rig_name, name, target, throws, seed = job
    rig = rigs.get_rig(rig_name)
    made, tolerance = controllers.CONTROLLERS[name](rig, seed)
    session = Session(rig, seed)
    out = []
    for _ in range(throws):
        result = session.throw(made, target, tolerance, record=True)
        rows = [[t_s * 1000.0, reading, None, coarse, fine] for t_s, reading, _, coarse, fine in result.rows
                if reading is not None]
        p = phase_times.phases({"rows": rows, "elapsed": result.elapsed_s}, target, NEAR_GR)
        if p:
            out.append(p)
    return out


def medians(rows: list) -> list:
    return [statistics.median(r[i] for r in rows) for i in range(3)] if rows else [float("nan")] * 3


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="pico2w_ar2209")
    parser.add_argument("--throws", type=int, default=200)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    lines = [f"{args.rig}: medians of bulk / approach / rest, the rig's against the replay's ({args.throws} throws a "
             f"series); the replay less the rig"]
    print(lines[0], flush=True)
    totals = {"bulk": [], "approach": [], "rest": []}
    with Workers() as pool:
        for label, folders, names in replay_study.groups(False):
            series = json.loads((HERE / "recordings" / folders[0] / "series.json").read_text(encoding="utf-8"))
            rig_rows = [p for t in series["throws"] for p in [phase_times.phases(t, series["target_gr"], NEAR_GR)] if p]
            jobs = [(args.rig, names[0], series["target_gr"], SESSION, seed) for seed in range(1, args.throws // SESSION + 1)]
            sim_rows = [p for chunk in pool.map(simulated_phases, jobs) for p in chunk]
            rm, sm = medians(rig_rows), medians(sim_rows)
            for key, a, b in zip(totals, rm, sm):
                totals[key].append(b - a)
            lines.append(f"  {label:28s} rig {rm[0]:5.2f} / {rm[1]:5.2f} / {rm[2]:5.2f}   sim {sm[0]:5.2f} / {sm[1]:5.2f} / "
                         f"{sm[2]:5.2f}   difference {sm[0] - rm[0]:+5.2f} / {sm[1] - rm[1]:+5.2f} / {sm[2] - rm[2]:+5.2f}")
            print(lines[-1], flush=True)
    lines.append("  median difference over the series: " + " / ".join(
        f"{key} {statistics.median(values):+.2f} s" for key, values in totals.items()))
    print(lines[-1])
    if args.out:
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
