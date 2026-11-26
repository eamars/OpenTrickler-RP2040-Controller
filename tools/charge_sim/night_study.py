"""The night of 2026-11-22's ABBA runs replayed in the simulation, each arm judged against the rig's (ADR-009, gate S).

The rig's arms are read from their recordings (bench/abba_figures.py, fit/abba_night_20261122.json). Each is played
here at the night's settings: 10 gr, the fast mode's 0.04 gr, the fine top at 1.0 rev/s as every arm's `ccon` had it,
the fine figure not learnt. The simulation's controller calibrates its own rig, as a unit does.

An arm is reproduced when the simulation's good, over and median, counted for 20 throws, lie within the rig arm's
own and the spread like runs showed on the rig: 4 good, 4 over and 0.7 s (adr009_controller's progress.md, 2026-11-23,
20:32).

    python night_study.py [--throws 400] [--rig pico2w_ar2209] [--out fit/study_20261123/night.txt]
"""

import argparse
import json
from pathlib import Path

import controllers
import mc
import offer_study  # noqa: F401  registers the offer and the seeds' controllers
import rigs
import staged
from workers import Workers

HERE = Path(__file__).resolve().parent

# The night's settings, in every worker: this module is imported by each
NIGHT_FINE_TOP_RPS = 1.0            # every arm's `ccon fine_top_rps=1.0`
staged.FINE_TOP_RPS = NIGHT_FINE_TOP_RPS
TARGET_GR = 10.0

# The rig's spread among like runs: the bar
SPREAD_GOOD = 4
SPREAD_OVER = 4
SPREAD_MEDIAN_S = 0.7

# Each arm of the night, and the simulation's controller of it
ARMS = (
    ("abba/glide", "glide_loose"), ("abba/other", "lpid_rig_fast"),
    ("night/glide", "glide_loose"), ("night/other", "lpid_floor_fast"),
    ("coarse/glide", "glide_loose"), ("coarse/other", "lpid_coarse_fast"),
    ("frame/glide", "glide_loose"), ("frame/other", "frame_pid_fast"),
    ("ownstop/glide", "glide_loose"), ("ownstop/other", "offer:seed: the bench unit's stored"),
    ("offer/glide", "glide_loose"), ("offer/other", "offer:offer, fine 0.6 x glide gain"),
    ("offer2/glide", "glide_loose"), ("offer2/other", "offer:offer, fine 0.6 x glide gain"),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=400)
    parser.add_argument("--rig", default="pico2w_ar2209")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    rig_arms = json.loads((HERE / "fit" / "abba_night_20261122.json").read_text())
    rig = rigs.get_rig(args.rig)
    lines = [f"{args.rig}, {args.throws} throws a controller, sessions of {mc.THROWS_PER_SESSION}, 10 gr at 0.04 gr, "
             f"the fine top {NIGHT_FINE_TOP_RPS} rev/s; the simulation's figures counted for 20 throws",
             f"{'arm':15s} {'rig good/over/median':>22s} {'sim good/over/median':>22s}  verdict"]
    print("\n".join(lines), flush=True)
    summaries = {}
    passed = 0
    with Workers() as pool:
        for arm, name in ARMS:
            if name not in summaries:
                results = mc.run_many(rig, name, TARGET_GR, args.throws, args.seed, pool)
                summaries[name] = mc.summarise(results, controllers.LOOSE_TOLERANCE_GR)
            s = summaries[name]
            r = rig_arms[arm]
            good, over, median = 20 * s["good_true"], 20 * s["over_true"], s["time_median"]
            misses = [what for what, off in (("good", abs(good - r["good"]) > SPREAD_GOOD),
                                             ("over", abs(over - r["over"]) > SPREAD_OVER),
                                             ("median", abs(median - r["median_s"]) > SPREAD_MEDIAN_S)) if off]
            passed += not misses
            lines.append(f"{arm:15s} {r['good']:8d} / {r['over']:2d} / {r['median_s']:5.1f} s "
                         f"{good:8.1f} / {over:4.1f} / {median:5.1f} s  "
                         f"{'reproduced' if not misses else 'missed: ' + ', '.join(misses)}")
            print(lines[-1], flush=True)
    lines.append(f"{passed} of {len(ARMS)} arms reproduced")
    print(lines[-1])
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
