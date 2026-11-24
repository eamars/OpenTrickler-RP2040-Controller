"""ADR-009, step 1: the calibration's method, each part run alone, on any bench.

A bench is the simulation's (`bench_twin.TwinBench`) or a unit's from the PC (`bench/trial.py`'s `UnitBench`): it turns
a tube for a time and reads the scale (`creep_stop`), lets time pass (`advance`), and has a clock (`now`) and the cup's
load (`cup.landed_gr`).

Three parts, each run alone, each using what the others stored or their defaults (the ADR's "Owner's direction"):
- the cup: its capacity, given by the user, and its empty weight (the unit's screens; given here as numbers);
- the scale: how long a small change takes to show and settle, by its size, and how far the reading runs behind a
  steady flow, by flow rate;
- the speeds: each tube's least and most speed at which the scale registers the powder consistently. The least: the
  slowest speed whose runs register alike (the spread of their powder, as a share of it), every run the same turns;
  the most: the fastest from the least up whose stops land alike (the spread of what lands after them). Each within
  CONSISTENT_TIMES the best speed's.

No retries and no pass criteria: a part that cannot measure an item says so, and the item keeps its default.

Usage, with the repository's venv, from this folder:
    python calibration.py [--rig pico2w_ar2209_night] [--seeds 3] [--capacity 150]
"""

import argparse
import math
import statistics
import sys
from dataclasses import dataclass, field

import rigs
from bench_twin import TwinBench
from staged import behind

# The defaults a part uses for what another part never measured
DEFAULT_RESPONSE_S = 3.0        # a small change settled within this, on the slow side
DEFAULT_LAG = ((0.1, 1.0), (10.0, 1.0))     # (flow gr/s, lag s): one second at every flow
DEFAULT_CUP_CAPACITY_GR = 100.0

# The method's own numbers
PRIME_RUNS_MOST = 5             # each tube turned at most this many times until two runs pour alike
PRIME_ALIKE_SHARE = 0.1         # two runs pour alike within this share of the larger
PRIME_TURNS = {"coarse": 1.0, "fine": 3.0}
RESPONSE_MOVES_TURNS = (0.05, 0.1, 0.25)    # the fine tube's small moves, by size
RESPONSE_MOVES = 4              # moves of each size
RESPONSE_MOVE_RPS = 0.25        # the speed a small move is made at
RESPONSE_WATCH_S = 10.0         # each move watched this long: longer than any scale's answer to a small change
LAG_RUNS = {"fine": ((0.05, 4.0), (0.1, 3.0), (0.25, 2.0), (0.5, 2.0)), "coarse": ((0.25, 1.5), (1.0, 1.0))}
LAG_RUNS_EACH = 2               # runs at each speed
LAST_S = 0.6                    # the reading at rest is the median of the watch's last stretch
SPEED_LADDER = 6                # speeds a tube is stopped from, spaced evenly in ratio between its motor's limits
SPEED_STOPS = 4                 # stops at each speed
SPEED_RUN_TURNS = 0.1           # every stop's run turns this much, so runs at every speed register alike powder,
SPEED_RUN_LEAST_S = 0.5         # and lasts at least this long,
SPEED_RUN_MOST_S = 10.0         # and never longer than this
CONSISTENT_TIMES = 3.0          # a speed is consistent with a spread within this many times the best speed's
POUR_GR_A_TURN_MOST = 12.0      # a run's most pour for the cup's check, before the tube has poured at all


@dataclass
class Cup:
    """The cup's two numbers, and the powder poured since it was last emptied."""

    capacity_gr: float = DEFAULT_CUP_CAPACITY_GR
    weight_gr: float = 0.0
    emptied_at_gr: float = 0.0
    pauses: int = 0


@dataclass
class Report:
    """What a part found, what it could not, and what it cost."""

    part: str
    records: dict = field(default_factory=dict)
    failures: list = field(default_factory=list)
    seconds: float = 0.0
    poured_gr: float = 0.0


def table(points: tuple, x: float) -> float:
    """A measured table read at x, straight between two points and the end point's outside them."""
    if x <= points[0][0]:
        return points[0][1]
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return points[-1][1]


def cup_room(bench, cup: Cup, most_gr: float) -> None:
    """Before a run: when its most pour would pass the cup's capacity, the calibration pauses for the cup to be
    emptied (on the unit, the user empties it; here the count starts again), then goes on."""
    if bench.cup.landed_gr - cup.emptied_at_gr + most_gr > cup.capacity_gr:
        cup.pauses += 1
        cup.emptied_at_gr = bench.cup.landed_gr


def settled(after: list, watch_s: float) -> tuple:
    """The reading at rest after a stop, when it first moved, and when it last moved away from its rest.

    Args:
        after: (seconds after the stop, reading) through the watch.
        watch_s: the watch's length.

    Returns:
        (the reading at rest, the first move's time or None, the settle time: the first reading after the last one
        that differed from the rest, 0 when it never moved).
    """
    rest = statistics.median(w for t, w in after if t >= watch_s - LAST_S)
    start = after[0][1]
    first = next((t for t, w in after if w != start), None)
    last_away = max((i for i, (t, w) in enumerate(after) if w != rest), default=None)
    settle = after[last_away + 1][0] if last_away is not None and last_away + 1 < len(after) else 0.0
    return rest, first, settle


def run_and_stop(bench, cup: Cup, tube: str, speed: float, run_s: float, watch_s: float, gr_turn: float) -> dict:
    """One steady turn and a stop, from rest to rest: its powder, what landed after the stop, and when it settled."""
    cup_room(bench, cup, speed * run_s * gr_turn)
    trial = bench.creep_stop(tube, speed, run_s, watch_s, "held")
    before, at_stop = trial["during"][0][1], trial["during"][-1][1]
    rest, first, settle = settled(trial["after"], watch_s)
    return {"gain": rest - before, "rise": rest - at_stop, "first_s": first, "settle_s": settle,
            "trial": trial, "before": before, "rest": rest}


def prime(bench, cup: Cup, tube: str, speed: float) -> float:
    """Turn a tube until two runs pour alike. Returns its grains a turn from the last run, for the cup's check only."""
    last = None
    gain = 0.0
    for _ in range(PRIME_RUNS_MOST):
        gain = run_and_stop(bench, cup, tube, speed, PRIME_TURNS[tube] / speed, 2.0, POUR_GR_A_TURN_MOST)["gain"]
        if last is not None and abs(gain - last) <= PRIME_ALIKE_SHARE * max(gain, last):
            break
        last = gain
    return max(gain, 0.0) / PRIME_TURNS[tube]


def scale_part(bench, cup: Cup) -> Report:
    """The scale: its response time to small changes by their size, and its lag behind a steady flow by flow rate.

    Args:
        bench: the bench (above).
        cup: the cup's numbers, the user's or the defaults.

    Returns:
        A Report whose records are `response` ((grains landed, first shown s, settled s) by move size) and `lag`
        ((flow gr/s, lag s) by speed), each sorted, and the failures in words.
    """
    report = Report("scale")
    start_s, start_gr = bench.now, bench.cup.landed_gr
    fine_turn = prime(bench, cup, "fine", 0.5)
    if fine_turn <= 0.0:
        report.failures.append("the fine tube poured nothing: the scale's response time and lag keep their defaults")
        report.seconds, report.poured_gr = bench.now - start_s, bench.cup.landed_gr - start_gr
        return report
    response = []
    for turns in RESPONSE_MOVES_TURNS:
        runs = [run_and_stop(bench, cup, "fine", RESPONSE_MOVE_RPS, turns / RESPONSE_MOVE_RPS, RESPONSE_WATCH_S,
                             fine_turn) for _ in range(RESPONSE_MOVES)]
        shown = [run for run in runs if run["gain"] > 0.0 and run["first_s"] is not None]
        if not shown:
            report.failures.append(f"a move of {turns:g} turn showed nothing in {RESPONSE_MOVES} tries")
            continue
        response.append((statistics.fmean(run["gain"] for run in shown),
                         statistics.median(run["first_s"] for run in shown),
                         statistics.median(run["settle_s"] for run in shown)))
    watch_s = (max(row[2] for row in response) if response else DEFAULT_RESPONSE_S) + 1.0
    lag = []
    for tube, runs in LAG_RUNS.items():
        gr_turn = fine_turn if tube == "fine" else prime(bench, cup, tube, 1.0)
        for speed, run_s in runs:
            made = [run_and_stop(bench, cup, tube, speed, run_s, watch_s, gr_turn) for _ in range(LAG_RUNS_EACH)]
            made = [run for run in made if run["gain"] > 0.0]
            if not made:
                report.failures.append(f"the {tube} tube at {speed:g} rev/s poured nothing")
                continue
            flow = statistics.fmean(run["gain"] / run_s for run in made)
            # how long the poured powder took, on average, from turned out to shown
            lags = [behind(run["trial"], run["before"], run["rest"], run["settle_s"] or watch_s) / run["gain"]
                    for run in made]
            lag.append((flow, max(0.0, statistics.fmean(lags))))
    report.records = {"response": sorted(response), "lag": sorted(lag)}
    report.seconds, report.poured_gr = bench.now - start_s, bench.cup.landed_gr - start_gr
    return report


def speeds_part(bench, cup: Cup, tube: str, least_rps: float, most_rps: float, scale: dict = None) -> Report:
    """A tube's least and most speed at which what lands after a stop is consistent.

    Args:
        bench: the bench (above).
        cup: the cup's numbers, the user's or the defaults.
        tube: "coarse" or "fine".
        least_rps, most_rps: the tube's motor's own limits: the ladder runs between them.
        scale: the scale part's records, or None for the defaults.

    Returns:
        A Report whose records are `least_rps`, `most_rps` and `stops` ((speed, registered powder's spread as a share,
        mean landed after the stop gr, its spread gr) by speed), and the failures in words.
    """
    report = Report(f"speeds {tube}")
    start_s, start_gr = bench.now, bench.cup.landed_gr
    response = (scale or {}).get("response") or ((0.0, 0.0, DEFAULT_RESPONSE_S),)
    watch_s = max(row[2] for row in response) + 1.0
    gr_turn = prime(bench, cup, tube, math.sqrt(least_rps * most_rps))
    if gr_turn <= 0.0:
        report.failures.append(f"the {tube} tube poured nothing: its least and most speed keep their values")
        report.seconds, report.poured_gr = bench.now - start_s, bench.cup.landed_gr - start_gr
        return report
    ratio = (most_rps / least_rps) ** (1.0 / (SPEED_LADDER - 1))
    stops = []
    for step in range(SPEED_LADDER):
        speed = least_rps * ratio ** step
        run_s = min(SPEED_RUN_MOST_S, max(SPEED_RUN_LEAST_S, SPEED_RUN_TURNS / speed))
        runs = [run_and_stop(bench, cup, tube, speed, run_s, watch_s, gr_turn) for _ in range(SPEED_STOPS)]
        if all(run["gain"] <= 0.0 for run in runs):
            report.failures.append(f"the {tube} tube at {speed:.3g} rev/s poured nothing")
            continue
        gains = [run["gain"] for run in runs]
        rises = [run["rise"] for run in runs]
        share = statistics.stdev(gains) / statistics.fmean(gains) if statistics.fmean(gains) > 0.0 else math.inf
        stops.append((speed, share, statistics.fmean(rises), statistics.stdev(rises)))
    records = {"stops": stops}
    if stops:
        best_share = min(row[1] for row in stops)
        least = min(row[0] for row in stops if row[1] <= CONSISTENT_TIMES * best_share)
        upper = [row for row in stops if row[0] >= least]
        best_spread = min(row[3] for row in upper)
        records["least_rps"] = least
        records["most_rps"] = max(row[0] for row in upper if row[3] <= CONSISTENT_TIMES * best_spread)
    report.records = records
    report.seconds, report.poured_gr = bench.now - start_s, bench.cup.landed_gr - start_gr
    return report


def say(report: Report) -> None:
    """A part's report in words."""
    print(f"  {report.part}: {report.seconds / 60.0:.1f} min, {report.poured_gr:.1f} gr")
    for name, value in report.records.items():
        if isinstance(value, list):
            for row in value:
                print(f"    {name}: " + ", ".join(f"{x:.3f}" for x in row))
        else:
            print(f"    {name}: {value:.3f}")
    for failure in report.failures:
        print(f"    FAILED: {failure}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rig", default="pico2w_ar2209_night")
    parser.add_argument("--seeds", type=int, default=3)
    parser.add_argument("--capacity", type=float, default=DEFAULT_CUP_CAPACITY_GR, help="the cup's capacity, gr")
    parser.add_argument("--fine", default="0.01,5", help="the fine motor's least and most speed, rev/s")
    parser.add_argument("--coarse", default="0.08,5", help="the coarse motor's least and most speed, rev/s")
    args = parser.parse_args()
    rig = rigs.get_rig(args.rig)
    limits = {"fine": tuple(float(x) for x in args.fine.split(",")),
              "coarse": tuple(float(x) for x in args.coarse.split(","))}
    for seed in range(args.seeds):
        print(f"seed {seed}:")
        # each part alone, on a bench of its own: the speeds first on the scale's defaults, then the scale
        for tube in ("fine", "coarse"):
            cup = Cup(capacity_gr=args.capacity)
            report = speeds_part(TwinBench(rig, seed), cup, tube, *limits[tube])
            say(report)
            print(f"    the cup emptied {cup.pauses} times")
        cup = Cup(capacity_gr=args.capacity)
        report = scale_part(TwinBench(rig, seed), cup)
        say(report)
        print(f"    the cup emptied {cup.pauses} times")
    return 0


if __name__ == "__main__":
    sys.exit(main())
