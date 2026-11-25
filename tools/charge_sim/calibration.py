"""ADR-009, step 1: the calibration's method, each part run alone, on any bench.

A bench is the simulation's (`bench_twin.TwinBench`) or a unit's from the PC (`bench/trial.py`'s `UnitBench`): it turns
a tube for a time and reads the scale (`creep_stop`), lets time pass (`advance`), and has a clock (`now`) and the cup's
load (`cup.landed_gr`).

Three parts, each run alone, each using what the others stored or their defaults (the ADR's "Owner's direction"):
- the cup: its capacity, given by the user, and its empty weight (the unit's screens; given here as numbers);
- the scale: how long a small change takes to show and settle, by its size, and how far the reading runs behind a
  steady flow, by flow rate;
- the speeds: each tube's least speed, LEAST_TIMES the slowest speed at which the tube pours: its runs together pour
  at least REGISTER_SHARE of its rate at the prime's speed, where it streams. A confident value, the measured minimum
  doubled. The ladder runs from the motor's least up to the prime's speed, no faster: nothing above it is judged. At
  each speed one run, the farthest from the speed's median, is rejected as a clump or a lull. `judge` makes it from
  runs live or kept. The most speed is not measured: the powder profile's maximum is left as the user has it.

No retries and no pass criteria: a part that cannot measure an item says so, and the item keeps its default.

Usage, with the repository's venv, from this folder:
    python calibration.py [--rig pico2w_ar2209_night] [--seeds 3] [--capacity 150]
"""

import argparse
import json
import math
import statistics
import sys
from dataclasses import dataclass, field
from pathlib import Path

import rigs
from bench_twin import TwinBench
from staged import behind

# The defaults a part uses for what another part never measured
DEFAULT_RESPONSE_S = 3.0        # a small change settled within this, on the slow side
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
SPEED_LADDER = 6                # speeds a tube is stopped from, spaced evenly in ratio from its motor's least up to
                                # the prime's speed (the motor's limits' middle in ratio), below it
SPEED_STOPS = 4                 # stops at each speed
SPEED_RUN_TURNS = 0.1           # every stop's run turns this much, so runs at every speed register alike powder,
SPEED_RUN_LEAST_S = 0.5         # and lasts at least this long,
SPEED_RUN_MOST_S = 10.0         # and never longer than this
LEAST_TIMES = 2.0               # the least speed is the measured minimum times this
REGISTER_SHARE = 0.5            # a speed registers when its runs pour at least this share of the tube's rate (gr a
                                # turn) at the prime's speed: by their sum, as one run of 0.1 turn is a few kernels
POUR_GR_A_TURN_MOST = 12.0      # a run's most pour for the cup's check, before the tube has poured at all


@dataclass
class Cup:
    """The cup's two numbers, the powder poured since it was last emptied, and what a full cup does: `on_full` is
    called before a run that would pass the capacity (on the unit, the pause that asks for the cup to be emptied); with
    none, the count of powder starts again, as when the cup has been emptied."""

    capacity_gr: float = DEFAULT_CUP_CAPACITY_GR
    weight_gr: float = 0.0
    emptied_at_gr: float = 0.0
    pauses: int = 0
    on_full: object = None


@dataclass
class Report:
    """What a part found, what it could not, and what it cost."""

    part: str
    records: dict = field(default_factory=dict)
    failures: list = field(default_factory=list)
    seconds: float = 0.0
    poured_gr: float = 0.0


def cup_room(bench, cup: Cup, most_gr: float) -> None:
    """Before a run: when its most pour would pass the cup's capacity, the calibration pauses for the cup to be
    emptied (on the unit, the user empties it; here the count starts again), then goes on."""
    if bench.cup.landed_gr - cup.emptied_at_gr + most_gr > cup.capacity_gr:
        cup.pauses += 1
        if cup.on_full is not None:
            cup.on_full(bench.cup.landed_gr - cup.emptied_at_gr, most_gr)
        cup.emptied_at_gr = bench.cup.landed_gr


def settled(after: list, watch_s: float) -> tuple:
    """The reading at rest after a stop, when it first moved, and when it last moved away from its rest.

    Args:
        after: (seconds after the stop, reading) through the watch.
        watch_s: the watch's length.

    Returns:
        (the reading at rest, the first move's time or None, the settle time: the first reading after the last one
        more than one step from the rest, 0 when it never moved). A step is the smallest change the readings make: a
        scale at rest flickers by one (the A&D by 0.02 gr, seconds after it settled), which is no move.
    """
    rest = statistics.median(w for t, w in after if t >= watch_s - LAST_S)
    start = after[0][1]
    first = next((t for t, w in after if w != start), None)
    changes = [abs(w1 - w0) for (_, w0), (_, w1) in zip(after, after[1:]) if w1 != w0]
    step = min(changes) if changes else 0.0
    last_away = max((i for i, (t, w) in enumerate(after) if abs(w - rest) > 1.5 * step), default=None)
    settle = after[last_away + 1][0] if last_away is not None and last_away + 1 < len(after) else 0.0
    return rest, first, settle


def run_and_stop(bench, cup: Cup, tube: str, speed: float, run_s: float, watch_s: float, gr_turn: float) -> dict:
    """One steady turn and a stop, from rest to rest: its powder, its turns, and when the reading moved and settled."""
    cup_room(bench, cup, speed * run_s * gr_turn)
    trial = bench.creep_stop(tube, speed, run_s, watch_s, "held")
    before = trial["during"][0][1]
    rest, first, settle = settled(trial["after"], watch_s)
    return {"gain": rest - before, "first_s": first, "settle_s": settle,
            "turns": trial.get("turns", speed * run_s), "trial": trial, "before": before, "rest": rest}


def prime(bench, cup: Cup, tube: str, speed: float) -> float:
    """Turn a tube until two runs pour alike. Returns its grains a turn from the last run: the cup's check, and the
    speeds part's measure of the tube's stream."""
    last = None
    run = {"gain": 0.0, "turns": PRIME_TURNS[tube]}
    for _ in range(PRIME_RUNS_MOST):
        run = run_and_stop(bench, cup, tube, speed, PRIME_TURNS[tube] / speed, 2.0, POUR_GR_A_TURN_MOST)
        if last is not None and abs(run["gain"] - last) <= PRIME_ALIKE_SHARE * max(run["gain"], last):
            break
        last = run["gain"]
    return max(run["gain"], 0.0) / run["turns"] if run["turns"] > 0.0 else 0.0


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
    """A tube's least speed.

    Args:
        bench: the bench (above).
        cup: the cup's numbers, the user's or the defaults.
        tube: "coarse" or "fine".
        least_rps, most_rps: the tube's motor's own limits: the prime turns at their middle in ratio, and the ladder
            runs from the least up to it.
        scale: the scale part's records, or None for the defaults.

    Returns:
        A Report whose records are `least_rps` and `stops` (as `judge` gives them), and the failures in words.
    """
    report = Report(f"speeds {tube}")
    start_s, start_gr = bench.now, bench.cup.landed_gr
    response = (scale or {}).get("response") or ((0.0, 0.0, DEFAULT_RESPONSE_S),)
    watch_s = max(row[2] for row in response) + 1.0
    prime_rps = math.sqrt(least_rps * most_rps)
    gr_turn = prime(bench, cup, tube, prime_rps)
    if gr_turn <= 0.0:
        report.failures.append(f"the {tube} tube poured nothing: its least speed keeps its value")
        report.seconds, report.poured_gr = bench.now - start_s, bench.cup.landed_gr - start_gr
        return report
    ratio = (prime_rps / least_rps) ** (1.0 / SPEED_LADDER)
    stops = []
    for step in range(SPEED_LADDER):
        speed = least_rps * ratio ** step
        run_s = min(SPEED_RUN_MOST_S, max(SPEED_RUN_LEAST_S, SPEED_RUN_TURNS / speed))
        runs = [run_and_stop(bench, cup, tube, speed, run_s, watch_s, gr_turn) for _ in range(SPEED_STOPS)]
        if all(run["gain"] <= 0.0 for run in runs):
            report.failures.append(f"the {tube} tube at {speed:.3g} rev/s poured nothing")
            continue
        stops.append((speed, [run["gain"] for run in runs], [run["turns"] for run in runs]))
    report.records, failures = judge(tube, stops, gr_turn)
    report.failures += failures
    report.seconds, report.poured_gr = bench.now - start_s, bench.cup.landed_gr - start_gr
    return report


def farthest(values: list) -> int:
    """The index of the value farthest from their median: a clump or a lull."""
    middle = statistics.median(values)
    return max(range(len(values)), key=lambda i: abs(values[i] - middle))


def pour_rate(gains: list, turns: list) -> float:
    """A speed's grains a turn by its runs' sum, the run farthest from their median (a clump or a lull) left out."""
    out = farthest(gains)
    kept_turns = sum(turns) - turns[out]
    return (sum(gains) - gains[out]) / kept_turns if kept_turns > 0.0 else 0.0


def judge(tube: str, stops: list, stream_gr_turn: float) -> tuple:
    """A tube's least speed from its ladder's runs.

    Args:
        tube: "coarse" or "fine", for the failures' words.
        stops: (speed, each run's registered powder gr, each run's turns) by speed.
        stream_gr_turn: the tube's grains a turn where it streams (the prime's).

    Returns:
        (records: `least_rps` where found, `stream_gr_turn`, and `stops` as (speed, whether it registers, its gr a
        turn); the failures in words).
    """
    failures = []
    rows = [(speed, pour_rate(gains, turns)) for speed, gains, turns in sorted(stops, key=lambda stop: stop[0])]
    pours = [(speed, stream_gr_turn > 0.0 and rate >= REGISTER_SHARE * stream_gr_turn) for speed, rate in rows]
    records = {"stream_gr_turn": stream_gr_turn,
               "stops": [(speed, 1.0 if registers else 0.0, rate) for (speed, rate), (_, registers) in zip(rows, pours)]}
    floor_speeds = [speed for speed, registers in pours if registers]
    if not floor_speeds:
        failures.append(f"no speed of the {tube} tube below its prime's poured at half its rate there: its least keeps "
                        "its value")
        return records, failures
    records["least_rps"] = min(LEAST_TIMES * min(floor_speeds), rows[-1][0])
    return records, failures


def judge_kept(folder, tube: str) -> tuple:
    """`judge` on a kept speeds part's runs (`runs.json`), as the method runs now: the prime's runs first, its last
    one the stream's rate, then the ladder's speeds below the prime's, each with SPEED_STOPS runs or more (a recording
    whose ladder went faster has those speeds left out)."""
    runs = json.loads((Path(folder) / "runs.json").read_text(encoding="utf-8"))
    made = [(round(run["speed_asked_rps"], 6), kept_gain(run), run.get("turns", run["speed_asked_rps"] * run["run_s"]))
            for run in runs]
    prime_rps = made[0][0]
    primed = 0
    while primed < len(made) and made[primed][0] == prime_rps:
        primed += 1
    stream = made[primed - 1][1] / made[primed - 1][2]
    by_speed = {}
    for speed, gain, turns in made[primed:]:
        if speed < prime_rps:
            by_speed.setdefault(speed, []).append((gain, turns))
    stops = [(speed, [g for g, _ in runs_at], [n for _, n in runs_at]) for speed, runs_at in by_speed.items()
             if len(runs_at) >= SPEED_STOPS]
    return judge(tube, stops, stream)


def kept_gain(run: dict) -> float:
    """A kept run's powder: its reading at rest at the end of its watch, less its first."""
    return statistics.median(w for t, w in run["after"] if t >= run["after"][-1][0] - LAST_S) - run["during"][0][1]


def scale_kept(folder) -> dict:
    """`scale_part`'s records made again from a kept scale part's runs (`runs.json`), in the order it ran them: the
    fine tube's prime, the small moves, then the lag's runs by tube, the coarse tube's prime before them."""
    runs = json.loads((Path(folder) / "runs.json").read_text(encoding="utf-8"))

    def measured(run):
        rest, first, settle = settled(run["after"], run["after"][-1][0])
        return {"gain": rest - run["during"][0][1], "first_s": first, "settle_s": settle, "trial": run,
                "before": run["during"][0][1], "rest": rest}

    moves = {}
    lag_runs = {}
    coarse_primed = False
    for run in runs:
        tube, speed = run["tube"], run["speed_asked_rps"]
        if tube == "fine" and abs(speed - RESPONSE_MOVE_RPS) < 1e-6 and run["run_s"] < 1.5:
            size = min(RESPONSE_MOVES_TURNS, key=lambda turns: abs(turns - speed * run["run_s"]))
            moves.setdefault(size, []).append(measured(run))
            continue
        if tube == "coarse" and not coarse_primed:
            coarse_primed = abs(speed - 1.0) > 1e-6     # the prime's runs end where the lag's first speed starts
            if not coarse_primed:
                continue
        planned = dict(LAG_RUNS[tube])
        speed_key = next((s for s in planned if abs(s - speed) < 1e-6), None)
        if speed_key is not None and abs(run["run_s"] - planned[speed_key]) < 0.3:
            lag_runs.setdefault((tube, speed_key), []).append(measured(run))
    response = []
    for size in sorted(moves):
        shown = [run for run in moves[size] if run["gain"] > 0.0 and run["first_s"] is not None]
        if shown:
            response.append((statistics.fmean(run["gain"] for run in shown),
                             statistics.median(run["first_s"] for run in shown),
                             statistics.median(run["settle_s"] for run in shown)))
    lag = []
    for (tube, speed), made in lag_runs.items():
        made = [run for run in made if run["gain"] > 0.0]
        if made:
            flow = statistics.fmean(run["gain"] / run["trial"]["run_s"] for run in made)
            lags = [behind(run["trial"], run["before"], run["rest"], run["settle_s"] or run["trial"]["after"][-1][0])
                    / run["gain"] for run in made]
            lag.append((flow, max(0.0, statistics.fmean(lags))))
    return {"response": sorted(response), "lag": sorted(lag)}


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
