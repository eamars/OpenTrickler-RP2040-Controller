"""The night of 2026-11-22's ABBA runs as their recordings give them (ADR-009, gate S).

Each run is four blocks of 10 throws, glide, other, other, glide (g1, p1, p2, g2); an arm is a controller's two blocks,
20 throws. Read at each throw's settled weight (the next throw's base less its own, the series' tool), as the night
judged them, at the fast mode's 0.04 gr: good, over, and the median throw's seconds. Also each arm's first rests'
spread (the sd of first errors) and its median approach (from the coarse tube's stop to the fine tube's first stop,
from the rows).

    python bench/abba_figures.py [--recordings ../recordings] [--json out.json]
"""

import argparse
import json
import statistics
from pathlib import Path

HERE = Path(__file__).resolve().parent
RUNS = ("abba", "night", "coarse", "frame", "ownstop", "offer", "offer2")
BAND_GR = 0.04


def approach_s(rows) -> float:
    """From the coarse tube's stop to the fine tube's first stop, in seconds; None when either is not seen."""
    coarse_off = fine_off = None
    for i in range(1, len(rows)):
        t, _, _, coarse, fine = rows[i][:5]
        if coarse_off is None and rows[i - 1][3] > 0.0 and coarse == 0.0:
            coarse_off = t
        if coarse_off is not None and rows[i - 1][4] > 0.0 and fine == 0.0:
            fine_off = t
            break
    if coarse_off is None or fine_off is None:
        return None
    return (fine_off - coarse_off) / 1000.0


def arm_figures(throws, target) -> dict:
    settled = [t for t in throws if t.get("settled") is not None]
    good = sum(1 for t in settled if abs(t["settled"] - target) <= BAND_GR + 1e-6)
    over = sum(1 for t in settled if t["settled"] - target > BAND_GR + 1e-6)
    firsts = [t["first"] for t in throws if isinstance(t.get("first"), float) and t["first"] == t["first"]]
    approaches = [a for a in (approach_s(t["rows"]) for t in throws if t.get("rows")) if a is not None]
    return {"throws": len(throws), "settled": len(settled), "good": good, "over": over,
            "median_s": statistics.median(t["elapsed"] for t in throws),
            "first_mean": statistics.mean(firsts) if firsts else None,
            "first_sd": statistics.stdev(firsts) if len(firsts) > 1 else None,
            "approach_s": statistics.median(approaches) if approaches else None}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--recordings", type=Path, default=HERE.parent / "recordings")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    out = {}
    print(f"{'run':8s} {'arm':6s} {'n':>3s} {'good':>5s} {'over':>5s} {'median':>7s} {'first':>7s} {'sd':>6s} "
          f"{'approach':>9s}")
    for run in RUNS:
        blocks = {}
        for folder in sorted(args.recordings.glob(f"20261122_*_series_{run}_[gp][12]_pico2w_ar2209")):
            block = folder.name.split("_series_")[1].split("_")[1]
            blocks[block] = json.loads((folder / "series.json").read_text())
        for arm, names in (("glide", ("g1", "g2")), ("other", ("p1", "p2"))):
            throws = [t for name in names for t in blocks[name]["throws"]]
            target = blocks[names[0]]["target_gr"]
            fig = arm_figures(throws, target)
            fig["controller"] = blocks[names[0]].get("controller")
            fig["ccon"] = blocks[names[0]].get("ccon_asked")
            out[f"{run}/{arm}"] = fig
            print(f"{run:8s} {arm:6s} {fig['throws']:3d} {fig['good']:5d} {fig['over']:5d} {fig['median_s']:6.1f}s "
                  f"{fig['first_mean']:+7.3f} {fig['first_sd']:6.3f} "
                  f"{fig['approach_s'] if fig['approach_s'] is not None else float('nan'):8.2f}s")
    if args.json:
        args.json.write_text(json.dumps(out, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
