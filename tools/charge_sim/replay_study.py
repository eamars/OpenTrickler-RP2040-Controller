"""Bench series replayed on a rig as the unit ran them, each judged against what the rig gave (ADR-009, gate S).

`--day`: the series of 2026-11-22 before 18:00, the model's own source: which record of the rig reproduces them.
`--night`: the night's ABBA arms, judged once on the record the day chose (adr009_controller's progress.md, 2026-11-23).

A series or an arm is reproduced when the simulation's good, over and median, counted for 20 throws, lie within the
rig's own and the spread like runs showed on the rig: 4 good, 4 over, 0.7 s (adr009_controller's progress.md,
2026-11-23, 20:32). `pid_learnt`'s arms have no calibration to replay: they are the simulation's own controllers of
them.

    python replay_study.py --day --rigs pico2w_ar2209,pico2w_ar2209_1122 [--throws 400] [--out ...]
"""

import argparse
import json
import sys
from pathlib import Path

import controllers
import mc
import offer_study  # noqa: F401  registers the offer's controllers, in every worker
import replay      # noqa: F401  registers replay:<folder>, in every worker
import rigs
from workers import Workers

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "bench"))
import abba_figures    # noqa: E402

SPREAD_GOOD, SPREAD_OVER, SPREAD_MEDIAN_S = 4, 4, 0.7

DAY = ("115105_series_glide002_c_lagshift", "121340_series_glide002_d_yields", "122716_series_glide004_e",
       "145047_series_mvp_accurate10", "145720_series_mvp_fast10", "150305_series_mvp_accurate20",
       "151219_series_mvp_fast20", "154649_series_fine5_learn_off", "155323_series_fine5_learn_on",
       "162339_series_fine5_split", "163000_series_fine5_off2", "170508_series_fresh5_learn_off",
       "171113_series_fresh5_learn_on", "173413_series_carry5_s1_fresh", "174023_series_carry5_s2_carried",
       "174616_series_carry5_s3_fresh")
# pid_learnt's arms: the simulation's own controllers of them
NIGHT_OWN = {"abba": "lpid_rig_fast", "night": "lpid_floor_fast", "coarse": "lpid_coarse_fast"}


def folder_of(stamp_name: str) -> str:
    return f"20261122_{stamp_name}_pico2w_ar2209"


def groups(night: bool) -> list:
    """(label, the rig's series folders, the simulation's controller of each folder or one for the group)."""
    if not night:
        return [(name.split("_series_")[1], [folder_of(name)], [f"replay:{folder_of(name)}"]) for name in DAY]
    out = []
    for run in abba_figures.RUNS:
        found = {f.name.split("_series_")[1].split("_")[1]: f.name
                 for f in sorted((HERE / "recordings").glob(f"20261122_*_series_{run}_[gp][12]_pico2w_ar2209"))}
        for arm, blocks in (("glide", ("g1", "g2")), ("other", ("p1", "p2"))):
            folders = [found[b] for b in blocks]
            if arm == "other" and run in NIGHT_OWN:
                names = [NIGHT_OWN[run]]
            else:
                names = [f"replay:{folder}" for folder in folders]
            out.append((f"{run}/{arm}", folders, names))
    return out


def rig_figures(folders: list) -> dict:
    throws, target, band = [], None, None
    for folder in folders:
        series = json.loads((HERE / "recordings" / folder / "series.json").read_text(encoding="utf-8"))
        throws += series["throws"]
        target, band = series["target_gr"], series["band_gr"]
    abba_figures.BAND_GR = band
    return {**abba_figures.arm_figures(throws, target), "target": target, "band": band}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--day", action="store_true")
    parser.add_argument("--night", action="store_true")
    parser.add_argument("--rigs", default="pico2w_ar2209")
    parser.add_argument("--throws", type=int, default=400)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if args.day == args.night:
        parser.error("one of --day and --night")
    lines = [f"{'--night' if args.night else '--day'}: {args.throws} throws a series, sessions of "
             f"{mc.THROWS_PER_SESSION}, judged at each series' band; counted for 20 throws"]
    print(lines[0], flush=True)
    with Workers() as pool:
        for rig_name in args.rigs.split(","):
            rig = rigs.get_rig(rig_name)
            reproduced, total = 0, 0
            lines.append(f"{rig_name}: {'series':30s} {'rig good/over/median':>21s} {'sim good/over/median':>21s}  "
                         f"verdict")
            print(lines[-1], flush=True)
            for label, folders, names in groups(args.night):
                r = rig_figures(folders)
                scale = 20.0 / r["throws"]
                tolerance = controllers.LOOSE_TOLERANCE_GR if r["band"] >= 0.04 else controllers.VERDICT_TOLERANCE_GR
                results = []
                for name in names:
                    results += mc.run_many(rig, name, r["target"], args.throws // len(names), args.seed, pool)
                s = mc.summarise(results, tolerance)
                good, over, median = 20 * s["good_true"], 20 * s["over_true"], s["time_median"]
                rg, ro = r["good"] * scale, r["over"] * scale
                misses = [w for w, off in (("good", abs(good - rg) > SPREAD_GOOD), ("over", abs(over - ro) > SPREAD_OVER),
                                           ("median", abs(median - r["median_s"]) > SPREAD_MEDIAN_S)) if off]
                reproduced += not misses
                total += 1
                lines.append(f"{'':14s}{label:30s} {rg:6.1f} / {ro:4.1f} / {r['median_s']:5.1f} s "
                             f"{good:6.1f} / {over:4.1f} / {median:5.1f} s  "
                             f"{'reproduced' if not misses else 'missed: ' + ', '.join(misses)}")
                print(lines[-1], flush=True)
            lines.append(f"{rig_name}: {reproduced} of {total} reproduced")
            print(lines[-1], flush=True)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
