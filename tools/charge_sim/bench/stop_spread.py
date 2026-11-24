"""How fast the fine tube can be stopped from with no wider spread (ADR-009, the research of 2026-11-24).

A one-shot finish stops the fine tube once, out of a steady flow. What then arrives after the stop is forecast by its
mean; its spread from stop to stop is what the final weight scatters by. This measures that spread at several speeds,
with enough stops at each to tell them apart.

Each stop is the calibration's own (`staged.run_stop`'s form): from rest a steady turn, the tube stopped and held,
the cup watched to rest. The speeds are taken in turn, one of each and then again, so a drift of the rig falls on all
of them alike. With `--reverse` the tube is turned back at its stop, as the coarse tube's back-off is.

Usage, with the repository's venv, the unit on the probe:
    python stop_spread.py --repo <checkout> --elf build_fix_2w\\app.elf --speeds 0.05,0.1,0.2,0.3 --count 20
                          --most-pour 60 [--cup-emptied "<the owner's words>"] [--reverse <turns>,<rev/s>]
"""

import argparse
import json
import statistics
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import trial    # noqa: E402  (beside this file: the unit's session and its bench)

WATCH_S = 3.5               # the A&D shows a small change within 2.8 s (the night's top-ups, 2026-11-24)
RUN_LEAST_S = 3.0           # a steady flow before the stop,
RUN_LEAST_TURNS = 0.25      # and at least this much of a turn at a slow speed
PRIME = (0.5, 4.0)          # a tube that stood still gives nothing at first: rev/s and seconds, not measured
LAST_S = 0.6                # the reading at rest is the median of the watch's last stretch
RULE_SHARE = 1.3            # the stop speed: the fastest whose spread is within this share of the smallest


def one_stop(bench, speed: float, reverse) -> dict:
    """A steady turn of the fine tube and its stop: the powder shown at the stop, and what arrived after it."""
    run_s = max(RUN_LEAST_S, RUN_LEAST_TURNS / speed)
    if reverse is None:
        made = bench.creep_stop("fine", speed, run_s, WATCH_S, "held")
    else:
        made = bench.creep_stop("fine", speed, run_s, WATCH_S, "backoff", backoff=reverse)
    before, at_stop = made["during"][0][1], made["during"][-1][1]
    end = statistics.median(w for t, w in made["after"] if t >= WATCH_S - LAST_S)
    return {"speed": speed, "turns": made["turns"], "gain": end - before, "at_stop": at_stop - before,
            "rise": end - at_stop}


def summed(stops: list) -> dict:
    rises = [s["rise"] for s in stops]
    yields = [s["gain"] / s["turns"] for s in stops if s["turns"] > 0]
    return {"stops": len(stops), "rise_mean": statistics.mean(rises), "rise_sd": statistics.pstdev(rises),
            "rise_least": min(rises), "rise_most": max(rises),
            "gr_turn_mean": statistics.mean(yields), "gr_turn_sd": statistics.pstdev(yields)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--board", default="pico2_w")
    parser.add_argument("--elf", help="the image on the unit, when it is not the board's own build folder's")
    parser.add_argument("--name", default="stop_spread")
    parser.add_argument("--speeds", default="0.05,0.1,0.2,0.3", help="the fine tube's speeds, rev/s")
    parser.add_argument("--count", type=int, default=20, help="stops at each speed")
    parser.add_argument("--reverse", help="turned back at the stop: <turns>,<rev/s>")
    parser.add_argument("--most-pour", type=float, required=True, help="the most this pours into the real cup, gr")
    parser.add_argument("--cup-emptied", help="the owner's words that the cup was emptied")
    args = parser.parse_args()
    speeds = [float(s) for s in args.speeds.split(",")]
    reverse = tuple(float(x) for x in args.reverse.split(",")) if args.reverse else None

    import record   # beside this file
    record.CUP_EMPTIED = args.cup_emptied
    record.ELF = args.elf
    unit = trial.RealUnit(args.repo, args.name, args.board, 0.02, 0.048, 50.0, args.most_pour)
    stops = []
    try:
        unit.ready()
        bench = trial.UnitBench(unit)
        bench.creep_stop("fine", PRIME[0], PRIME[1], WATCH_S, "held")   # the prime, not measured
        for round_no in range(args.count):
            for speed in speeds:
                stop = one_stop(bench, speed, reverse)
                stops.append(stop)
                print(f"round {round_no + 1:2d} {speed:5.3f} rev/s: at the stop {stop['at_stop']:+.3f}, after it "
                      f"{stop['rise']:+.3f}, {stop['gain'] / stop['turns']:.3f} gr a turn; the cup {bench.last:.2f}",
                      flush=True)
    finally:
        unit.stop()
        out = {"reverse": reverse, "watch_s": WATCH_S, "stops": stops,
               "by_speed": {f"{v:g}": summed([s for s in stops if s["speed"] == v]) for v in speeds
                            if any(s["speed"] == v for s in stops)}}
        (unit.folder / "stop_spread.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
        verdict = unit.close()

    print(f"\n{'rev/s':>6} {'stops':>5} {'after the stop: mean':>21} {'spread':>7} {'least':>7} {'most':>7} "
          f"{'gr a turn':>10} {'its spread':>10}")
    for v in speeds:
        f = out["by_speed"].get(f"{v:g}")
        if f:
            print(f"{v:6.3f} {f['stops']:5d} {f['rise_mean']:21.3f} {f['rise_sd']:7.3f} {f['rise_least']:7.3f} "
                  f"{f['rise_most']:7.3f} {f['gr_turn_mean']:10.3f} {f['gr_turn_sd']:10.3f}")
    spreads = {v: out["by_speed"][f"{v:g}"]["rise_sd"] for v in speeds if f"{v:g}" in out["by_speed"]}
    if spreads:
        least = min(spreads.values())
        chosen = max(v for v, sd in spreads.items() if sd <= RULE_SHARE * least)
        print(f"the smallest spread {least:.3f} gr; the fastest speed within {RULE_SHARE:g} of it: {chosen:g} rev/s")
    print(f"kept in {unit.folder}")
    return verdict


if __name__ == "__main__":
    sys.exit(main())
