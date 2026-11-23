"""ADR-009's two proposals of 2026-11-21, for trial in the simulation: one staged controller, one calibration.

Both proposals share everything but the finish:
- the weight acted on is a forecast: the last reading plus the powder each tube has turned out that the reading
  does not show yet, counted in turns. Each small parcel of powder is taken to show after the lag of the flow it
  was turned out at, and that lag is measured at each flow, since a scale answers a fast stream sooner than a slow
  one and a slow one sooner than a single kernel;
- each tube turns at a speed in proportion to the forecast room above its own margin, between a floor and a top,
  and stops for good when the room is down to its margin;
- the verdict is on a reading that has come to rest, and a throw that rests short is topped up by small moves of
  the fine tube, each sized from what moves of that size were measured to release.

The finish:
- "jog" (proposal 1): the fine tube stops from a steady dribble, short by a margin, and the rest is added by moves;
- "glide" (proposal 2): the fine tube slows to a creep whose powder still on its way is about the band, and stops
  where the forecast is a digit short of the target.

The proposals themselves, with the trial's figures, are in
`docs/developments_plans/ADR-009-controller_and_tuning/research/proposals_2026-11-21.md`.

Nothing is tuned: every number a controller uses is measured by the calibration, which turns the tubes on a
bench, or follows from those by a rule written here. The user gives the band.

The bench is the simulation's (`bench_twin.TwinBench`, with `throw.Session` for the rehearsals: `calibrate()`) or
a real unit's through its console (`bench/trial.py`): `measure_on()` and `rehearse()` take either.
"""

import dataclasses
import math
import statistics
from dataclasses import dataclass, field

from bench_twin import TwinBench
from throw import Session

MOVE_RPS = 0.2                  # a small move is made at this speed, as the bench's watched nudges were
PARCEL_S = 0.01                 # the powder turned out is counted in parcels of this long
# The fine tube's top speed in this trial. The bench turned it to 1 rev/s with both powders (and to 5 with
# AR2209), but above 0.5 rev/s the simulated tube pours in gaps and dumps the bench never showed (no rise for up to
# 1.4 s, where the bench's longest was 0.2 s: progress.md, 2026-11-21), which no controller could be judged on.
FINE_TOP_RPS = 0.5
COARSE_TOP_RPS = 2.0
COARSE_FLOOR_RPS = 0.3          # the coarse tube pours in lumps below it (bench, 2026-11-20)
# Since ADR-009's gate F.2 the three above are no longer the frame's: each tube runs within its user's limits (the
# profile's and the motor's) and never faster than the fastest speed measured. They are the simulation's stand-in
# user, the range its plant models, never a firmware default; FINE_LEAST_RPS is A14's default for the fine motor.
# The studies that set them stand in for a user so (sim_limits() reads them at each call)
FINE_LEAST_RPS = 0.01
# The fine tube's stops are measured at these flows, in digits a second: the scale's lag is taken at each
FINE_RATES_DIGITS_S = ((1.0, 8, 5.0), (2.0, 8, 5.0), (4.0, 4, 3.5), (8.0, 4, 3.0), (16.0, 4, 3.0), (32.0, 4, 3.0))
# The coarse stops (F2): at the coarse least, the geometric mean of its ends and the coarse most, so many runs of so
# long each. Geometric, as a lag changes by flow over a span of tens (the fine ladder doubles its flow each stop)
COARSE_STOP_FORM = ((3, 1.5), (2, 1.5), (2, 1.2))
BACKOFF_STOPS = 3               # F.3: the back-off's spill, from three stops at the coarse floor
# Small moves, by the digits one is meant to release on average, and how many are made
MOVES_DIGITS = ((0.7, 24), (2.5, 12), (6.0, 8))
# How long a stop or a move is watched is the scale's to say: a few small moves are watched this long first, and
# every watch after is half as long again as the slowest of them took to show, and a second
PROBE_MOVES = 4
PROBE_WATCH_S = 15.0
LEAST_WATCH_S = 3.0
DRIBBLE_DIGITS_S = 8.0          # the jog finish's dribble: the slowest flow the bench found even (0.2 rev/s, AR2209)
REHEARSALS = 10
REHEARSAL_GR = 1.0
MOST_MOVES = 8                  # tries a throw is given after its first rest: small moves, and returns to the stream
COARSE_SPREADS = 3.0            # of the coarse tube's spread a turn, kept clear of its powder still unseen
# The creep's powder still on its way, as a share of the band. Half of it was tried: 1.5 to 2 s slower a throw and
# no more good ones (progress.md, 2026-11-21)
CREEP_BAND_SHARE = 1.0
# The reading a throw at rest is judged on: the lowest it showed since its last rise, so that a reading which
# flickers across the band's edge is not taken for one inside it. Judged on the highest, one throw in ten rested
# just under the band and was called good (the jog finish, BM2: 86% good against 94%)
REST_ON = "lowest"
CREEP_MOST_S = 6.0              # the creep is never so slow that its stretch takes longer than this on average
# The rules' own numbers (set_rules and the throw), named for ADR-009's gate N (numbers.md, section D); the firmware's
# port (src/charge_control.c) still writes them as literals
SLOWEST_LAG_LEAST_S = 0.3       # D1: the slowest fine stop's lag taken as at least this
CREEP_LEAST_SHARE = 0.5         # D2: the creep never under this share of the slowest stop's speed
CREEP_TOP_STOP = 2              # D3: nor above the speed of this fine stop (from 0)
GLIDE_SPREADS = 2.0             # D6: the glide room is the stop room and this many spreads of a stop from
GLIDE_FLOW_TIMES = 4.0          # D6: this many times the creep's flow
GLIDE_LAG_SHARE = 0.6           # D7, D8: a glide time is this share of its lag
GLIDE_LEAST_S = 0.4             # D7: the fine glide time's limits
GLIDE_MOST_S = 2.0
COARSE_GLIDE_LEAST_S = 0.3      # D8: the coarse glide time's limits
COARSE_GLIDE_MOST_S = 1.5
COARSE_ROOM_SPREADS = 3.0       # D9: the coarse room is this many spreads of a coarse stop from its floor,
COARSE_ROOM_EXTRA_GR = 0.2      # D9: the glide room and this
REST_FLOOR_TIMES = 2.0          # D10: the rest is the slowest quiet time of the stops within this many times the creep
REST_EXTRA_S = 0.3              # D10: and this
COARSE_LEAST_TURNS = 0.5        # D12: the coarse tube runs only for more than its room and this share of a turn
STILL_LEAST_S = 0.4             # C16, D15: at rest, no new high for this long,
STILL_INTERVALS = 2.0           # D15: or this many reading intervals if longer
LEARN_SHARE = 0.15              # of a throw's first error at rest, into where the fine tube stops
LEARN_MOST_DIGITS = 2.0         # no throw teaches more than this
LEARN_DEAD_DIGITS = 0.25        # nor less
# The fine tube's grains a turn learnt over a session (2026-11-22): each throw's powder from the fine tube's slowing
# (once the coarse tube is quiet, if it ran) to the first rest, over the turns whose powder showed in that stretch; the
# figure used is their mean against the calibration's own, which counts for LEARN_FINE_PRIOR throws (0: no learning),
# never further than LEARN_FINE_MOST from it. A mean, so a throw whose powder is held at the lip and the one after it
# that dumps it cancel; the stretch from the slowing on, so it is the finish's figure, not the stream's at full speed
# Off by default since ADR-009's gate N (numbers.md, C13): on, it read through a lag no calibration had measured.
# Gate 3 decides it. LEARN_FINE_PRIOR_ON is the value it was thrown with, for a study or a check that turns it on
LEARN_FINE_PRIOR = 0.0
LEARN_FINE_PRIOR_ON = 3.0
LEARN_FINE_MOST = 0.25
LEARN_FINE_LEAST_TURNS = 0.2
# The fine tube's figure in the forecast (2026-11-22): "one" figure for every turn, or "split": a turn before the
# stretch opens (the tube at its top speed, or the coarse tube still arriving) at the stream's figure, a turn after it
# at the finish's, each learnt over the session. A whole throw of 5 gr on the rig poured 0.83 gr a fine turn and its
# finish 0.68: one figure cannot hold both
LEARN_FINE_FORM = "split"
LEARN_FINE_LEAST_STREAM_TURNS = 0.5
# Two times this close are the same time. The simulation's times and a record's numbers lie on a millisecond's grid, so
# a parcel falls due on a reading's very millisecond often, and which way such a tie falls must not hang on the last
# bit of a float: the firmware's port (src/charge_control.c), in single precision, breaks it the same way (ADR-009)
TIE_S = 1e-4
BAND_EDGE_DIGITS = 0.25        # a rest within the band and this much of a digit is in it
# The throw's limits (ADR-009, gate F.1: the reference keeps them as the firmware does, src/charge_rules.h): a throw
# past one ends "limit", both tubes stopped
THROW_MOST_S = 120.0           # C18: a throw takes no longer
# C19: the fine tube turns no more in a throw than this many times its target over its grains a turn, and its small
# moves' turns: a tube pouring a third of its figure needs three. With no figure the bound is no turn at all
FINE_MOST_TIMES = 4.0
OVER_TOLERANCES = 100.0        # C20: a reading this many tolerances over the target stops everything
# The scale's noise, one for both tests below (C24): the zero's mean margin (A9, the user's) and this many digits,
# one for a reading's flicker about its mean, one for a drift of a digit in a throw
NOISE_DIGITS = 2.0
ZERO_MARGIN = 0.02             # A9 at its default: the harness gives the user's (`zero_margin`)
# No powder (C25): while a tube turns in the stream, more than this many of the noise of forecast powder due since the
# reading last rose by more than the noise, the first of it waiting longer than the longest lag measured. In the
# simulation an ordinary throw had at most 6.6 of them due before a rise, a tube pouring a third of its figure in
# clumps 19 (progress.md, 2026-11-23, F.1): half as many again
NO_POWDER_RISES = 30.0
# The learnt source (ADR-009, gate F.4): a powder with no calibration. A tube's figure is born in the throw once this
# many of its turns should have shown by the assumed lag (D17's least stretch) and the reading has risen past the
# noise; an unfigured tube that should have shown NO_FIGURE_TURNS with the reading not past the noise pours nothing
# (C26: five times the stretch that makes a figure; a tube pouring a third of its figure shows by 0.6 turn)
FIGURE_LEAST_TURNS = LEARN_FINE_LEAST_TURNS
NO_FIGURE_TURNS = 1.0
# The fine figure learnt at each first rest by `pid_learnt`'s rule (G9): what landed after the fine stop over the
# turns then on their way, held to between these shares of the figure; a stop with fewer turns on their way teaches
# nothing
LEARNT_LEAST_TURNS = 0.01
LEARNT_THROW_LEAST = 0.5
LEARNT_THROW_MOST = 2.0
# The scale's lag and rest a unit never calibrated assumes, one lag for every speed: the owner's (2026-11-24, after
# F.4's study found no pair that holds on every stand-in), the A&D's spec; held at gate 7. Its cost: on `slower`
# about 26 points more often over than a calibrated unit, and first throws of about 40 to 50 s
ASSUMED_LAG_S = 2.0
ASSUMED_REST_S = 6.0


@dataclass(frozen=True)
class Stop:
    """A tube's stops from one speed: what still arrived after them (mean, spread) and when the reading was still."""

    speed: float
    unseen_gr: float
    spread_gr: float
    quiet_s: float
    lag_s: float = None             # how long its powder took from turned out to shown; None in a calibration kept before it


@dataclass(frozen=True)
class Move:
    """Small moves of one size: the turn, what they released (mean, nine in ten at most) and the wait to see it."""

    turns: float
    mean_gr: float
    most_gr: float
    wait_s: float


@dataclass(frozen=True)
class Limits:
    """A user's settings the frame takes (ADR-009, gates F.2 and F.3): each tube's limits (the least the higher of the
    profile's and the motor's least, the most the lower of their most); the back-off at the coarse stop (A11: its
    turns, 0 for off, and its speed); the coarse stop threshold (A6), the coarse room of a back-off whose spill was not
    measured as it is set; and the gate's travel for its move at the coarse stop (A10, A17: 0 for none)."""

    coarse_least: float
    coarse_most: float
    fine_least: float
    fine_most: float
    backoff_turns: float = 0.0
    backoff_rps: float = 0.0
    coarse_threshold_gr: float = 5.0
    gate_coarse_s: float = 0.0


def sim_limits() -> Limits:
    """The simulation's stand-in user, read at each call so a study's settings hold."""
    return Limits(COARSE_FLOOR_RPS, COARSE_TOP_RPS, FINE_LEAST_RPS, FINE_TOP_RPS)


@dataclass
class Calibration:
    """What the calibration measured, and the numbers the controller takes from it by rule."""

    digit_gr: float
    interval_s: float
    coarse_gr_turn: float
    fine_gr_turn: float
    coarse_stops: list
    fine_stops: list
    moves: list
    watch_s: float = 4.0            # how long this scale's stops and moves had to be watched
    coarse_share: float = 0.0       # how far the coarse tube's powder a turn varied from run to run, as a share
    # by rule, from the above and the band
    band_gr: float = 0.02
    finish: str = "glide"
    floor_rps: float = 0.0          # the fine tube's slowest: the creep, or the dribble
    stop_room_gr: float = 0.0       # the fine tube stops when the forecast room is down to this
    aim_gr: float = 0.0             # where the first reading at rest is meant to lie, against the target
    glide_room_gr: float = 0.0      # the fine tube is at its floor from this room down
    glide_s: float = 0.7
    coarse_room_gr: float = 1.0
    coarse_glide_s: float = 0.4
    rest_s: float = 2.0             # the wait after the fine tube's stop before the reading is judged
    # each tube's range, by rule from the user's limits and what was measured (ADR-009, gate F.2)
    fine_top_rps: float = 0.0       # the lower of the fine most and the fastest fine stop measured
    coarse_top_rps: float = 0.0     # the lower of the coarse most and the fastest coarse stop measured
    coarse_floor_rps: float = 0.0   # the coarse least
    creep_rps: float = 0.0          # the creep by the rules, before the fine least held it up
    # the back-off's spill (ADR-009, gate F.3): what arrived after a stop from the coarse floor with the back-off, its
    # spread, and the back-off it was measured at (turns, speed); -1 and 0 until measured
    backoff_unseen_gr: float = -1.0
    backoff_spread_gr: float = -1.0
    backoff_turns: float = 0.0
    backoff_rps: float = 0.0
    # by rule: the back-off at the coarse stop (its speed and time, 0 for none), its own effect on the forecast, and
    # 1 when the coarse room is A6's because the spill was not measured as the back-off is set
    backoff_speed_rps: float = 0.0
    backoff_s: float = 0.0
    backoff_gr: float = 0.0
    backoff_fallback: float = 0.0
    seconds: float = 0.0            # how long the calibration took, and the powder it poured
    poured_gr: float = 0.0
    first_errors: list = field(default_factory=list)
    learnt: bool = False            # the learnt source (F.4): made from no measurement, its figures born in the throw


def percentile(values: list, share: float) -> float:
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(share * len(ordered)))]


def run_stop(bench, tube: str, speed: float, run_s: float, watch_s: float, digit: float) -> dict:
    """One steady turn and a stop, from rest to rest: its powder, what arrived after the stop, when it was still.

    The turns are the unit's own count where the bench gives it, else the speed over the time the tube really turned.
    """
    trial = bench.creep_stop(tube, speed, run_s, watch_s, "held")
    bench.advance(0.3)
    before, at_stop = trial["during"][0][1], trial["during"][-1][1]
    end = statistics.median(w for t, w in trial["after"] if t >= watch_s - 0.6)
    # when the reading first reaches where it ends: powder only adds, and a digit's flicker after it is not waited for
    quiet = next((t for t, w in trial["after"] if w >= end - digit / 2.0), watch_s)
    return {"gain": end - before, "rise": end - at_stop, "quiet": quiet,
            "turns": trial.get("turns", speed * trial["run_s"]), "behind": behind(trial, before, end, quiet)}


def behind(trial: dict, before: float, end: float, quiet: float) -> float:
    """How far the reading ran behind the powder turned out, summed over one turn and its stop, in grain-seconds.

    Over the powder poured it is how long that powder took, on average, from turned out to shown: the scale's
    delay at that flow, with whatever lateness is the powder's own. It is taken over the whole run and not at the
    moment of the stop. What is unseen at a stop also holds whatever the one who turns the tubes adds (a PC's
    reading is a command old and its stop a command late: 0.07 s on the bench, 2026-11-21), and a forecast that
    took that for the scale's delay counted a fast stream twice while its first powder showed. Taken over the run it
    holds less of that, but not none: a PC's lags through the console ran 0.055 s behind those the firmware's own
    throws show (2026-11-22, `bench/forecast_replay.py --fit`), so a calibration is measured where its controller runs.
    """
    run_s = trial["run_s"]
    poured = end - before
    samples = [(t + run_s, min(w, end) - before) for t, w in trial["during"] + trial["after"] if t <= quiet]
    area = 0.0
    for (t0, shown0), (t1, shown1) in zip(samples, samples[1:]):
        out0 = poured * min(1.0, max(0.0, t0 / run_s))      # turned out evenly from the start to the stop
        out1 = poured * min(1.0, max(0.0, t1 / run_s))
        area += ((out0 - shown0) + (out1 - shown1)) / 2.0 * (t1 - t0)
    return area


def stops_at(bench, tube: str, speed: float, count: int, run_s: float, watch_s: float, digit: float,
             yields: list = None) -> Stop:
    """A tube's stops from one speed. `yields` takes each run's grains a turn, for whoever wants their spread."""
    runs = [run_stop(bench, tube, speed, run_s, watch_s, digit) for _ in range(count)]
    if yields is not None:
        yields.extend(run["gain"] / run["turns"] for run in runs)
    rises = [run["rise"] for run in runs]
    poured = sum(run["gain"] for run in runs)
    lag = max(0.0, sum(run["behind"] for run in runs) / poured) if poured >= digit else None
    return Stop(speed, statistics.fmean(rises), statistics.stdev(rises), percentile([run["quiet"] for run in runs], 0.9),
                lag)


def measure_backoff(bench, cal: Calibration, limits: Limits, digit: float) -> None:
    """The back-off's spill (ADR-009, gate F.3; gate 3's step built early): BACKOFF_STOPS stops from the coarse floor
    with the user's back-off, what arrived after them and its spread, kept with the back-off they were made at."""
    runs = []
    for _ in range(BACKOFF_STOPS):
        trial = bench.creep_stop("coarse", limits.coarse_least, COARSE_STOP_FORM[0][1], cal.watch_s, "backoff",
                                 backoff=(limits.backoff_turns, limits.backoff_rps))
        bench.advance(0.3)
        end = statistics.median(w for tt, w in trial["after"] if tt >= cal.watch_s - 0.6)
        runs.append(end - trial["during"][-1][1])
    cal.backoff_unseen_gr = statistics.fmean(runs)
    cal.backoff_spread_gr = statistics.stdev(runs)
    cal.backoff_turns, cal.backoff_rps = limits.backoff_turns, limits.backoff_rps


def table(points: list, x: float) -> float:
    """A measured table read at x, straight between two points and the end point's outside them."""
    if x <= points[0][0]:
        return points[0][1]
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return points[-1][1]


def measure_on(bench, digit: float, interval_s: float, limits: Limits = None) -> Calibration:
    """The calibration's open-loop part: prime, each tube's powder a turn, its stops by flow, and the small moves.

    Args:
        bench: turns a tube and reads the scale: `creep_stop(tube, speed, run_s, watch_s, "held")` gives a trial
            as `bench/record.py` saves one, `advance(seconds)` lets time pass, `now` is its clock and
            `cup.landed_gr` the powder in the cup.
        digit: the scale's digit, in grains.
        interval_s: the usual time between two readings.
        limits: the user's limits for each tube (the simulation's stand-in user when none): the ladder is cut by them.
    """
    limits = limits or sim_limits()
    for tube, turns in (("coarse", 1.0), ("fine", 3.0)):    # prime until two runs pour the same, not by a count
        last = None
        for _ in range(5):
            gain = run_stop(bench, tube, 1.0, turns, 3.0, digit)["gain"]
            if last is not None and abs(gain - last) <= 0.1 * max(gain, last):
                break
            last = gain
    # how long this scale takes to show a small move: every watch below follows from it
    probes = [run_stop(bench, "fine", MOVE_RPS, 0.1 / MOVE_RPS, PROBE_WATCH_S, digit) for _ in range(PROBE_MOVES)]
    watch_s = max(LEAST_WATCH_S, 1.5 * max(run["quiet"] for run in probes) + 1.0)
    coarse_runs = [run_stop(bench, "coarse", 1.0, 0.75, watch_s, digit) for _ in range(3)]
    fine_runs = [run_stop(bench, "fine", 1.0, 1.5, watch_s, digit) for _ in range(3)]
    coarse_gr = sum(run["gain"] for run in coarse_runs) / sum(run["turns"] for run in coarse_runs)
    fine_gr = sum(run["gain"] for run in fine_runs) / sum(run["turns"] for run in fine_runs)

    # each stop's flow in digits a second, cut into the user's fine range; one at the fine most if it ends below
    fine_stops = []
    for rate, count, run_s in FINE_RATES_DIGITS_S:
        speed = min(limits.fine_most, max(limits.fine_least, rate * digit / fine_gr))
        if not fine_stops or speed > fine_stops[-1].speed:
            fine_stops.append(stops_at(bench, "fine", speed, count, run_s, watch_s, digit))
    if fine_stops[-1].speed < limits.fine_most:
        fine_stops.append(stops_at(bench, "fine", limits.fine_most, 4, 3.0, watch_s, digit))
    # the coarse tube's powder a turn, run by run: its spread is how far a count of its turns can be off
    coarse_yields = [run["gain"] / run["turns"] for run in coarse_runs]
    least, most = limits.coarse_least, limits.coarse_most
    coarse_speeds = (least, math.sqrt(least * most), most)
    coarse_stops = [stops_at(bench, "coarse", speed, count, run_s, watch_s, digit, coarse_yields)
                    for speed, (count, run_s) in zip(coarse_speeds, COARSE_STOP_FORM)]

    moves = []
    for digits, count in MOVES_DIGITS:
        turns = digits * digit / fine_gr
        gains, shown, turned = [], [], []
        for _ in range(count):
            run = run_stop(bench, "fine", MOVE_RPS, turns / MOVE_RPS, watch_s, digit)
            gains.append(run["gain"])
            turned.append(run["turns"])
            if run["gain"] >= digit / 2.0:
                shown.append(run["quiet"])
        wait = percentile(shown, 0.9) + 0.3 if shown else watch_s
        # the move is the turn the tube really made, by the unit's own count where the bench gives it: a throw's
        # move is made to that
        moves.append(Move(statistics.fmean(turned), statistics.fmean(gains), percentile(gains, 0.9), wait))
    cal = Calibration(digit_gr=digit, interval_s=interval_s, coarse_gr_turn=coarse_gr, fine_gr_turn=fine_gr,
                      coarse_stops=coarse_stops, fine_stops=fine_stops, moves=moves, watch_s=watch_s,
                      coarse_share=statistics.stdev(coarse_yields) / statistics.fmean(coarse_yields))
    if limits.backoff_turns > 0.0 and limits.backoff_rps > 0.0:
        measure_backoff(bench, cal, limits, digit)      # the user's back-off on: its spill measured as it is set
    cal.seconds, cal.poured_gr = bench.now, bench.cup.landed_gr
    return cal


def measure(rig, seed: int) -> Calibration:
    """The open-loop part on the simulation's bench."""
    return measure_on(TwinBench(rig, seed), rig.scale.digit_gr, rig.scale.report_s)


def for_finish(measured: Calibration, finish: str, band_gr: float, limits: Limits = None) -> Calibration:
    """A calibration for one finish and band from what was measured once: its own copy, with the rules set within
    the user's limits (the simulation's stand-in user when none)."""
    cal = dataclasses.replace(measured, first_errors=[])
    set_rules(cal, finish, band_gr, limits)
    return cal


def to_json(cal: Calibration) -> dict:
    return dataclasses.asdict(cal)


def from_json(kept: dict) -> Calibration:
    kept = dict(kept)
    for name, kind in (("coarse_stops", Stop), ("fine_stops", Stop), ("moves", Move)):
        kept[name] = [kind(**row) for row in kept[name]]
    return Calibration(**kept)


def lag_table(stops: list, gr_turn: float) -> list:
    """(speed, seconds): how far behind the pan the reading is at each speed's flow."""
    return [(stop.speed, stop.lag_s if stop.lag_s is not None else max(0.0, stop.unseen_gr) / (gr_turn * stop.speed))
            for stop in stops]


def set_rules(cal: Calibration, finish: str, band_gr: float, limits: Limits = None) -> None:
    """The controller's numbers from the measured ones, within the user's limits (the simulation's stand-in user when
    none). Nothing here is fitted to a powder, a tube or a scale."""
    limits = limits or sim_limits()
    cal.finish, cal.band_gr = finish, band_gr
    fine, coarse = cal.fine_stops, cal.coarse_stops
    # each tube's range: the user's limits, and never faster than the fastest speed the calibration measured
    cal.fine_top_rps = min(limits.fine_most, fine[-1].speed)
    cal.coarse_top_rps = min(limits.coarse_most, coarse[-1].speed)
    cal.coarse_floor_rps = limits.coarse_least
    lags = lag_table(fine, cal.fine_gr_turn)
    spread = [(stop.speed, stop.spread_gr) for stop in fine]
    quiet = [(stop.speed, stop.quiet_s) for stop in fine]
    if finish == "glide":
        # The creep: the flow at which the powder still on its way is about the band, at the lag measured there
        slowest_lag = max(lags[0][1], SLOWEST_LAG_LEAST_S)
        cal.floor_rps = min(max(CREEP_BAND_SHARE * band_gr / slowest_lag / cal.fine_gr_turn,
                                CREEP_LEAST_SHARE * fine[0].speed), fine[min(CREEP_TOP_STOP, len(fine) - 1)].speed)
        # never under the user's fine least: a least above the creep holds it up, and the charge says so
        cal.creep_rps = cal.floor_rps
        cal.floor_rps = max(cal.floor_rps, limits.fine_least)
        # The first reading at rest is meant to lie a digit under the target (half the band, if that is more): a
        # throw that rests short is topped up, one that rests over is lost. Aimed at the target itself the trial
        # gave 5 points fewer good throws and half as many overs again (progress.md, 2026-11-21)
        cal.aim_gr = -max(cal.digit_gr, band_gr / 2.0)
        cal.stop_room_gr = -cal.aim_gr
    else:
        cal.floor_rps = min(cal.fine_top_rps, DRIBBLE_DIGITS_S * cal.digit_gr / cal.fine_gr_turn)
        cal.creep_rps = cal.floor_rps
        cal.floor_rps = max(cal.floor_rps, limits.fine_least)
        cal.stop_room_gr = 2.5 * table(spread, cal.floor_rps) + cal.digit_gr
        cal.aim_gr = -cal.stop_room_gr
    # The floor is reached with twice the spread of a stop from four times its flow still to go
    cal.glide_room_gr = cal.stop_room_gr + (GLIDE_SPREADS * table(spread, GLIDE_FLOW_TIMES * cal.floor_rps)
                                            if finish == "glide" else 0.0)
    if finish == "glide":
        # On a scale so late that such a creep would take too long, the creep is quicker and more is left to the moves
        stretch = cal.glide_room_gr - cal.stop_room_gr
        cal.floor_rps = max(cal.floor_rps, stretch / CREEP_MOST_S / cal.fine_gr_turn)
    cal.glide_s = min(GLIDE_MOST_S, max(GLIDE_LEAST_S, GLIDE_LAG_SHARE * max(lag for _, lag in lags[2:])))
    coarse_lag = lag_table(coarse, cal.coarse_gr_turn)
    cal.coarse_glide_s = min(COARSE_GLIDE_MOST_S, max(COARSE_GLIDE_LEAST_S, GLIDE_LAG_SHARE * coarse_lag[0][1]))
    cal.coarse_room_gr = COARSE_ROOM_SPREADS * coarse[0].spread_gr + cal.glide_room_gr + COARSE_ROOM_EXTRA_GR
    # The back-off (A11): made at the coarse stop when it is on; its spill forecast only when measured as it is set,
    # else the coarse room is the coarse stop threshold (A6), and the charge says so
    cal.backoff_speed_rps = cal.backoff_s = cal.backoff_gr = cal.backoff_fallback = 0.0
    if limits.backoff_turns > 0.0 and limits.backoff_rps > 0.0:
        cal.backoff_speed_rps = limits.backoff_rps
        cal.backoff_s = limits.backoff_turns / limits.backoff_rps
        measured = (cal.backoff_unseen_gr >= 0.0 and abs(cal.backoff_turns - limits.backoff_turns) < 1e-6
                    and abs(cal.backoff_rps - limits.backoff_rps) < 1e-6)
        if measured:
            cal.backoff_gr = cal.backoff_unseen_gr - coarse[0].unseen_gr
            cal.coarse_room_gr = (COARSE_ROOM_SPREADS * max(coarse[0].spread_gr, cal.backoff_spread_gr)
                                  + cal.glide_room_gr + COARSE_ROOM_EXTRA_GR)
        else:
            cal.backoff_fallback = 1.0
            cal.coarse_room_gr = limits.coarse_threshold_gr
    # The wait before the first judgement: the slowest the reading came to rest after a stop from near the floor
    cal.rest_s = max(stop.quiet_s for stop in fine
                     if stop.speed <= REST_FLOOR_TIMES * cal.floor_rps or stop is fine[0]) + REST_EXTRA_S


def learnt_calibration(digit_gr: float, interval_s: float, band_gr: float, lag_s: float = None, rest_s: float = None,
                       limits: Limits = None) -> Calibration:
    """The learnt source (ADR-009, gate F.4): a calibration made from no measurement, for a powder never calibrated.

    No figure (each is born in the throw), one stop for each tube carrying the scale's lag for every speed (the unit
    record's when measured, else the assumed), no spread, no small moves. Its rules: the coarse room is the coarse stop
    threshold (A6), the stop room a digit (the glide's aim), the creep the user's fine least, the rest the scale's.
    """
    limits = limits or sim_limits()
    lag_s = ASSUMED_LAG_S if lag_s is None else lag_s
    rest_s = ASSUMED_REST_S if rest_s is None else rest_s
    cal = Calibration(digit_gr=digit_gr, interval_s=interval_s, coarse_gr_turn=0.0, fine_gr_turn=0.0,
                      coarse_stops=[Stop(limits.coarse_least, 0.0, 0.0, rest_s, lag_s)],
                      fine_stops=[Stop(limits.fine_least, 0.0, 0.0, rest_s, lag_s)], moves=[], learnt=True)
    set_learnt_rules(cal, band_gr, limits)
    return cal


def set_learnt_rules(cal: Calibration, band_gr: float, limits: Limits = None) -> None:
    """The learnt source's rules (F.4, point 3), within the user's limits: nothing measured but the scale's lag and
    rest, so no measured speed bounds a tube, no spread widens a margin, and no move is made."""
    limits = limits or sim_limits()
    cal.finish, cal.band_gr = "glide", band_gr
    cal.fine_top_rps = limits.fine_most
    cal.coarse_top_rps = limits.coarse_most
    cal.coarse_floor_rps = limits.coarse_least
    cal.floor_rps = cal.creep_rps = limits.fine_least
    cal.aim_gr = -max(cal.digit_gr, band_gr / 2.0)
    cal.stop_room_gr = -cal.aim_gr
    cal.glide_room_gr = cal.stop_room_gr
    lag = cal.fine_stops[0].lag_s
    cal.glide_s = min(GLIDE_MOST_S, max(GLIDE_LEAST_S, GLIDE_LAG_SHARE * lag))
    cal.coarse_glide_s = min(COARSE_GLIDE_MOST_S, max(COARSE_GLIDE_LEAST_S, GLIDE_LAG_SHARE * cal.coarse_stops[0].lag_s))
    cal.coarse_room_gr = limits.coarse_threshold_gr
    cal.backoff_speed_rps = cal.backoff_s = cal.backoff_gr = cal.backoff_fallback = 0.0
    if limits.backoff_turns > 0.0 and limits.backoff_rps > 0.0:
        cal.backoff_speed_rps = limits.backoff_rps
        cal.backoff_s = limits.backoff_turns / limits.backoff_rps
        cal.backoff_fallback = 1.0      # no spill measured: the coarse room is A6's, as it is anyway
    cal.rest_s = cal.fine_stops[0].quiet_s


@dataclass(frozen=True)
class PidLaw:
    """The PID speed law in the frame (ADR-009, the owner's option 2, 2026-11-22 night; an experiment).

    Each tube's speed is today's PID loop on the frame's forecast, the profile's gains: the coarse tube's error is the
    room above the coarse room (the frame switches it off there), the fine tube's the room itself. The P term every
    tick; the I and D terms as today's loop takes them, on each reading (the integral a sum of errors, the derivative
    a change per millisecond). The fine tube's least speed is the frame's floor, not the profile's: the stop's speed is
    the frame's (the night's second run). Everything else is the frame's: the stop, the rest, the small moves, the
    learning.
    """

    coarse_kp: float
    coarse_ki: float
    coarse_kd: float
    coarse_min_rps: float
    coarse_max_rps: float
    fine_kp: float
    fine_ki: float
    fine_kd: float
    fine_max_rps: float


def pid_law_from(cal: Calibration, fine_kp: float = 0.8, fine_ki: float = 0.0, fine_kd: float = 15.0,
                 fine_max_rps: float = 2.0) -> PidLaw:
    """The PID law of option 2: the coarse tube's numbers made from the calibration (kp from its grains a turn and
    glide time, between the frame's coarse floor and top, as option 1 had them), the fine tube's the profile's (by
    default the owner's Pico W AR2209 gains)."""
    return PidLaw(coarse_kp=1.0 / (cal.coarse_gr_turn * cal.coarse_glide_s), coarse_ki=0.0, coarse_kd=0.0,
                  coarse_min_rps=COARSE_FLOOR_RPS, coarse_max_rps=COARSE_TOP_RPS, fine_kp=fine_kp, fine_ki=fine_ki,
                  fine_kd=fine_kd, fine_max_rps=fine_max_rps)


def profile_pid(limits: Limits = None, coarse_kp: float = 0.05, coarse_kd: float = 0.3, fine_kp: float = 0.8,
                fine_kd: float = 15.0) -> PidLaw:
    """The PID law on a powder profile's own gains, within the user's limits: the learnt source's speed law (F.4).
    By default the firmware's default AR2209 profile (`src/profile.c`, profile 1)."""
    limits = limits or sim_limits()
    return PidLaw(coarse_kp=coarse_kp, coarse_ki=0.0, coarse_kd=coarse_kd, coarse_min_rps=limits.coarse_least,
                  coarse_max_rps=limits.coarse_most, fine_kp=fine_kp, fine_ki=0.0, fine_kd=fine_kd,
                  fine_max_rps=limits.fine_most)


def pid_offer(cal: Calibration) -> PidLaw:
    """The auto-tuner's offer (ADR-009, 2026-11-22 night): the PID law's gains made from a calibration with its rules
    set. The coarse tube: kp 1 / (its grains a turn x its glide time), between the frame's coarse floor and top, as
    options 1 and 2 threw it. The fine tube: kp 0.6 / (its grains a turn x the glide time), a P loop whose time constant
    is the fine tube's longest lag (the glide time is 0.6 of it), the frame's top; its least speed is the frame's floor
    whatever the profile says. No I or D: they added nothing on the rig. Chosen in `offer_study.py`
    (fit/study_20261122/offer.txt) over the glide's own gain (7 points worse with AR2209 at 5 gr) and half of it."""
    return PidLaw(coarse_kp=1.0 / (cal.coarse_gr_turn * cal.coarse_glide_s), coarse_ki=0.0, coarse_kd=0.0,
                  coarse_min_rps=cal.coarse_floor_rps, coarse_max_rps=cal.coarse_top_rps,
                  fine_kp=0.6 / (cal.fine_gr_turn * cal.glide_s), fine_ki=0.0, fine_kd=0.0,
                  fine_max_rps=cal.fine_top_rps)


class Staged:
    """The controller. The harness reads coarse_rps, fine_rps and done after each call.

    pid: None for the glide's speed law; a PidLaw for the PID law in the same frame.
    """

    def __init__(self, cal: Calibration, acceleration_rps2: float, rehearsal: bool = False):
        self.cal = dataclasses.replace(cal)     # its own copy: the learnt fine figure goes into it
        self.learn_fine_prior = LEARN_FINE_PRIOR
        self.fine_gr_measured = cal.fine_gr_turn
        self.fine_learn_n = 0
        self.fine_learn_sum = 0.0
        self.fine_form = LEARN_FINE_FORM
        self.fine_gr_stream = cal.fine_gr_turn       # the split form's two figures, each learnt like the one
        self.stream_learn_n = 0
        self.stream_learn_sum = 0.0
        self.acceleration = acceleration_rps2
        self.rehearsal = rehearsal      # end at the first reading at rest: the calibration's own throws
        self.coarse_lags = lag_table(cal.coarse_stops, cal.coarse_gr_turn)
        self.fine_lags = lag_table(cal.fine_stops, cal.fine_gr_turn)
        self.stop_room = cal.stop_room_gr
        self.name = cal.finish
        self.pid = None
        # The PID law's guard against gains bad to begin with (2026-11-22 night): each tube never faster than the
        # frame's top (the fastest speed whose lag the calibration knows) nor than the frame can stop it from (the
        # fine tube: its floor plus the room above the stop over its grains a turn and glide time; the coarse tube:
        # the glide's coarse law), and the I term counting only while its tube is not held at a limit
        self.pid_envelope = True
        self.zero_margin = ZERO_MARGIN
        self.gate_coarse_s = 0.0        # the gate's travel for its move at the coarse stop (A10, A17): the caller's
        # per throw: (the error at the rest after its last stream, moves made, times it went back to the stream)
        self.log = []
        self.learnt_n = 0               # the learnt source: the throws its fine figure counts, its birth the first

    def start(self, target_gr: float, now_ms: float) -> None:
        cal = self.cal
        self.target = target_gr
        self.done = False
        self.result = "none"            # good, over, short, limit, cup, no_powder once done
        self.started_at = now_ms / 1000.0
        self.moved_turns = 0.0          # the small moves' turns asked for, which C19 adds to its bound
        self.backoff_until = None       # while the coarse tube turns back at its stop (F.3)
        self.hold_until = None          # the rest not judged before the gate's move at the coarse stop has ended
        # No powder: the reading at the last rise counted, the forecast powder due since, and when the first of it was due
        self.powder_mark = 0.0
        self.due_since_mark = 0.0
        self.first_due = None
        self.longest_lag = max(lag for _, lag in lag_table(cal.coarse_stops, cal.coarse_gr_turn)
                               + lag_table(cal.fine_stops, cal.fine_gr_turn))
        self.coarse_rps = 0.0
        self.fine_rps = 0.0
        self.glide_rps = {"coarse": 0.0, "fine": 0.0}  # the glide's stream law's asked speeds, in shadow
        self.phase = "feed"
        # a charge too small for the coarse tube's margin is the fine tube's alone
        self.coarse_on = target_gr > cal.coarse_room_gr + COARSE_LEAST_TURNS * cal.coarse_gr_turn
        self.turned = {"coarse": 0.0, "fine": 0.0}      # as the unit counts its steps
        self.speed = {"coarse": 0.0, "fine": 0.0}       # the motors' own ramped speeds
        self.counted = False            # the harness gives the unit's own count of the turns (`count`)
        self.parcels = []               # [when it will show, grains, tube]: turned out, not yet in the reading
        self.unseen = 0.0               # their sum
        self.coarse_unseen = 0.0        # the coarse tube's part of it
        self.marked = {"coarse": 0.0, "fine": 0.0}      # each tube's turns at the last parcel
        self.marked_at = now_ms / 1000.0
        self.next_mark = now_ms / 1000.0 + PARCEL_S
        self.last_tick = now_ms / 1000.0
        self.reading = 0.0
        self.highest = 0.0              # powder only adds: the reading is at rest when it has made no new high
        self.lowest = 0.0
        self.last_rise = now_ms / 1000.0
        self.stopped_at = None
        self.wait = 0.0
        self.move_until = None
        self.move_turns = None          # while a small move is asked for: the turn it is to be
        self.moves = 0
        self.streams = 0                # times a rest far short sent the throw back to the stream
        self.first_error = None
        self.coarse_ran = self.coarse_on
        self.coarse_off_at = None
        self.window_reading = None      # the fine tube's stretch for its learning: the reading, and the turns shown
        self.window_shown_turns = 0.0
        self.window_turned = 0.0        # the fine tube's turns when it opened, and its powder still unseen then
        self.window_unseen_gr = 0.0
        self.pid_integral = {"coarse": 0.0, "fine": 0.0}
        self.pid_last_error = {"coarse": 0.0, "fine": 0.0}
        self.pid_term = {"coarse": 0.0, "fine": 0.0}      # the I and D terms, as of the last reading
        self.pid_held = {"coarse": False, "fine": False}  # its last speed was held at a limit (the envelope's too)
        self.pid_last_s = now_ms / 1000.0
        self.pid_first = True
        # The learnt source (F.4): an unfigured tube's turns due since its count began, and the reading it began at
        # (the coarse tube's at the start; the fine tube's once the coarse tube's powder has all shown); the fine
        # stop that its learning reads (the reading, the turns then on their way, the coarse tube's powder unseen)
        self.shown_turns = {"coarse": 0.0, "fine": 0.0}
        self.count_mark = {"coarse": 0.0, "fine": None}
        self.coarse_birth = None        # (s since the start, the reading, the forecast) when the coarse figure was born
        self.have_stop = False
        self.stop_reading = self.stop_window_turns = self.stop_coarse_unseen = 0.0
        self.learnt_done = False

    def _figured(self, tube: str) -> bool:
        return (self.cal.coarse_gr_turn if tube == "coarse" else self.cal.fine_gr_turn) > 0.0

    def _room(self) -> float:
        """The room the stream is poured against: the target less the forecast, and less three spreads of the coarse
        tube's powder still unseen (kept clear by both tubes)."""
        return self.target - self._forecast() - COARSE_SPREADS * self.cal.coarse_share * self.coarse_unseen

    def _pid_reading(self, now: float) -> None:
        """The PID law's I and D terms on a reading, as today's loop takes them."""
        pid = self.pid
        room = self._room()
        errors = {"coarse": room - self.cal.coarse_room_gr, "fine": room}
        elapsed_ms = (now - self.pid_last_s) * 1000.0
        for tube, ki, kd in (("coarse", pid.coarse_ki, pid.coarse_kd), ("fine", pid.fine_ki, pid.fine_kd)):
            if not (self.pid_envelope and self.pid_held[tube]):
                self.pid_integral[tube] += errors[tube]
            derivative = 0.0 if self.pid_first or elapsed_ms <= 0.0 else \
                (errors[tube] - self.pid_last_error[tube]) / elapsed_ms
            self.pid_term[tube] = ki * self.pid_integral[tube] + kd * derivative
            self.pid_last_error[tube] = errors[tube]
        self.pid_last_s = now
        self.pid_first = False

    def _parcel(self, now: float) -> None:
        """Count what each tube turned out since the last parcel: it will show after the lag of its present flow.

        The parcel was turned out over the stretch since the last one, and is counted from that stretch's middle.
        """
        cal = self.cal
        turned_out_at = (self.marked_at + now) / 2.0
        self.marked_at = now
        for tube, gr_turn, lags in (("coarse", cal.coarse_gr_turn, self.coarse_lags), ("fine", cal.fine_gr_turn, self.fine_lags)):
            turned = self.turned[tube] - self.marked[tube]
            self.marked[tube] = self.turned[tube]
            if tube == "coarse" and self.backoff_until is not None:
                continue        # turned back at its stop, a tube pours nothing (F.3)
            if turned > 0.0:
                if gr_turn <= 0.0:
                    # no figure yet (the learnt source): its turns, forecast once its figure is born
                    self.parcels.append([turned_out_at + table(lags, self.speed[tube]), turned, tube])
                    continue
                if tube == "fine" and self.fine_form == "split" and not cal.learnt:
                    gr_turn = cal.fine_gr_turn if self.window_reading is not None else self.fine_gr_stream
                grains = gr_turn * turned
                self.parcels.append([turned_out_at + table(lags, self.speed[tube]), grains, tube])
                self.unseen += grains
                if tube == "coarse":
                    self.coarse_unseen += grains

    def _forecast(self) -> float:
        """The weight in the cup once all that was turned out has shown: the reading and what is on its way."""
        return self.reading + self.unseen

    def count(self, now_ms: float, turns: dict) -> None:
        """Each tube's turns since the throw began, by the unit's own count of its steps.

        A harness that has the count gives it before each tick, and the controller then reckons nothing itself: a
        unit's firmware has it for nothing. A PC that reckoned the turns from the speeds it had asked for was a
        command ahead of the motors (bench, 2026-11-21).
        """
        now = now_ms / 1000.0
        elapsed = now - self.marked_at
        for tube in ("coarse", "fine"):
            if elapsed > 0.0:
                self.speed[tube] = (turns[tube] - self.turned[tube]) / elapsed
            self.turned[tube] = turns[tube]
        self.counted = True
        self._parcel(now)

    def moved(self, now_ms: float) -> None:
        """The harness made the small move asked for (`move_turns`) itself, to its step count: the move is over."""
        self._end_move(now_ms / 1000.0)

    def _end_move(self, now: float) -> None:
        self.fine_rps = 0.0
        self.move_turns = None
        self.phase = "rest"
        self.stopped_at = now

    def on_tick(self, now_ms: float) -> None:
        now = now_ms / 1000.0
        elapsed = now - self.last_tick
        self.last_tick = now
        if not self.counted:
            for tube, asked in (("coarse", self.coarse_rps), ("fine", self.fine_rps)):
                change = self.acceleration * elapsed
                speed = self.speed[tube]
                speed = min(asked, speed + change) if speed < asked else max(asked, speed - change)
                self.speed[tube] = speed
                self.turned[tube] += speed * elapsed
            if now >= self.next_mark - TIE_S:
                self._parcel(now)
                self.next_mark += PARCEL_S
        if self.backoff_until is not None and now >= self.backoff_until - TIE_S:
            self.backoff_until = None
            self.coarse_rps = 0.0
        # with no figure the bound is no turn (F.1); the learnt source's, before its figure is born, none: no powder
        # covers a tube that shows nothing (C26, F.4)
        fine_most = (FINE_MOST_TIMES * self.target / self.cal.fine_gr_turn + self.moved_turns
                     if self.cal.fine_gr_turn > 0.0 else math.inf if self.cal.learnt else 0.0)
        if now - self.started_at > THROW_MOST_S or self.turned["fine"] > fine_most:
            self._stop_all("limit", self.reading)
            return
        if self.phase == "feed":
            self._feed(now)
        elif self.phase == "move" and now >= self.move_until - TIE_S:
            self._end_move(now)

    def _feed(self, now: float) -> None:
        cal = self.cal
        # The coarse tube's powder still unseen is known only by its turns, and a turn's powder varies: three times
        # that spread of it is kept clear by both tubes. A small charge is all unseen when the coarse tube stops,
        # and its margin is the wider for it; in a large one the reading has caught up and little is added
        room = self._room()
        pid = self.pid
        self.glide_rps = {"coarse": 0.0, "fine": 0.0}
        if self.coarse_on:
            if room <= cal.coarse_room_gr:
                self.coarse_on = False
                self.coarse_rps = 0.0
                if self.coarse_off_at is None:
                    self.coarse_off_at = now
                if cal.backoff_s > 0.0:
                    # the back-off: turned back at its speed for its time; its own effect forecast when measured
                    self.backoff_until = now + cal.backoff_s
                    self.coarse_rps = -cal.backoff_speed_rps
                    if cal.backoff_gr != 0.0:
                        lag = table(self.coarse_lags, cal.coarse_floor_rps)
                        # shaken off while it turns back: due once the turn back is over, and the lag
                        self.parcels.append([now + cal.backoff_s + lag, cal.backoff_gr, "coarse"])
                        self.unseen += cal.backoff_gr
                        self.coarse_unseen += cal.backoff_gr
                if self.gate_coarse_s > 0.0:
                    self.hold_until = now + self.gate_coarse_s
            elif not self._figured("coarse"):
                # no figure yet (the learnt source): its least speed until one is born (F.4)
                self.coarse_rps = self.glide_rps["coarse"] = cal.coarse_floor_rps
            else:
                glide = (room - cal.coarse_room_gr) / (cal.coarse_gr_turn * cal.coarse_glide_s)
                self.glide_rps["coarse"] = min(cal.coarse_top_rps, max(cal.coarse_floor_rps, glide))
                if pid is not None:
                    asked = pid.coarse_kp * (room - cal.coarse_room_gr) + self.pid_term["coarse"]
                    top = pid.coarse_max_rps
                    if self.pid_envelope:
                        top = min(top, cal.coarse_top_rps, cal.coarse_floor_rps + (room - cal.coarse_room_gr) / (cal.coarse_gr_turn * cal.coarse_glide_s))
                    self.coarse_rps = min(top, max(pid.coarse_min_rps, asked))
                    self.pid_held["coarse"] = not (pid.coarse_min_rps < asked < top)
                else:
                    self.coarse_rps = self.glide_rps["coarse"]
        if not self.coarse_on and room <= self.stop_room:
            self.fine_rps = 0.0
            if self.coarse_unseen >= cal.digit_gr:
                return      # a pause, not its stop: the fine tube's stop is taken on its own powder alone
            if cal.learnt and not self.have_stop and self._figured("fine"):
                # the learnt source's fine stop, for its learning at the rest: the turns on their way now
                self.have_stop = True
                self.stop_reading = self.reading
                self.stop_window_turns = self._fine_unseen_turns()
                self.stop_coarse_unseen = self.coarse_unseen
            self.phase = "rest"
            self.stopped_at = now
            self.wait = cal.rest_s
            return
        if not self._figured("fine"):
            # no figure yet (the learnt source): its least speed, after the coarse tube's figure where it runs (F.4)
            waits = self.coarse_on and not self._figured("coarse")
            self.fine_rps = self.glide_rps["fine"] = 0.0 if waits else cal.floor_rps
            return
        glide = (room - cal.glide_room_gr) / (cal.fine_gr_turn * cal.glide_s)
        self.glide_rps["fine"] = min(cal.fine_top_rps, max(cal.floor_rps, glide))
        if pid is not None:
            asked = pid.fine_kp * room + self.pid_term["fine"]
            top = pid.fine_max_rps
            if self.pid_envelope:
                top = min(top, cal.fine_top_rps, cal.floor_rps + max(0.0, room - self.stop_room) / (cal.fine_gr_turn * cal.glide_s))
            self.fine_rps = min(top, max(cal.floor_rps, asked))
            self.pid_held["fine"] = not (cal.floor_rps < asked < top)
            return
        self.fine_rps = self.glide_rps["fine"]

    def glide_speed(self, tube: str) -> float:
        """What the glide's stream law asks of a tube now, for a recording: in the stream its speed whichever law
        pours; at a rest or a move the speed asked, the same under both laws (charge_control_glide_rps)."""
        if self.phase == "feed":
            return self.glide_rps[tube]
        return self.coarse_rps if tube == "coarse" else self.fine_rps

    def on_reading(self, now_ms: float, weight: float) -> None:
        now = now_ms / 1000.0
        cal = self.cal
        if weight >= self.highest + cal.digit_gr / 2.0:
            self.highest = weight
            self.lowest = weight        # the lowest shown since that rise: a reading that flickers shows both
            self.last_rise = now
        self.lowest = min(self.lowest, weight)
        self.reading = weight
        # what came due with this reading from a tube still turning, and the earliest of it: a stopped tube's powder
        # still due is its figure's error, not a tube that pours nothing (the coarse tube's varies by its share)
        due_gr, first_due = 0.0, None
        turning = {"coarse": self.coarse_rps > 0.0, "fine": self.fine_rps > 0.0}
        figured = {"coarse": self._figured("coarse"), "fine": self._figured("fine")}
        if self.parcels:                # what was due to show by now is in this reading
            for parcel in self.parcels:
                if parcel[0] <= now + TIE_S:
                    if not figured[parcel[2]]:
                        # an unfigured tube's turns (the learnt source): counted as shown once its count has begun
                        if self.count_mark[parcel[2]] is not None:
                            self.shown_turns[parcel[2]] += parcel[1]
                    elif turning[parcel[2]]:
                        due_gr += parcel[1]
                        first_due = parcel[0] if first_due is None else min(first_due, parcel[0])
            self.parcels = [parcel for parcel in self.parcels if parcel[0] > now + TIE_S]
            self._sum_unseen()
        noise = self.zero_margin + NOISE_DIGITS * cal.digit_gr
        # the cup lifted: a reading below the throw's base by more than the scale's noise. Both tubes stop at once
        if weight < -noise:
            self._stop_all("cup", weight)
            return
        # far over, in the user's terms: the tolerance is the band and half a digit (A7's conversion)
        if weight > self.target + OVER_TOLERANCES * (cal.band_gr + cal.digit_gr / 2.0):
            self._stop_all("limit", weight)
            return
        if cal.learnt and self._births(now, weight, noise):
            return
        # no powder: the forecast's powder due and not shown, while a tube turns in the stream
        if self.phase != "feed" or weight > self.powder_mark + noise:
            self.powder_mark = weight
            self.due_since_mark = 0.0
            self.first_due = None
        elif due_gr > 0.0:
            self.due_since_mark += due_gr
            if self.first_due is None:
                self.first_due = first_due
        if (self.phase == "feed" and (self.coarse_rps > 0.0 or self.fine_rps > 0.0)
                and self.due_since_mark > NO_POWDER_RISES * noise and self.first_due is not None
                and now - self.first_due > self.longest_lag + TIE_S):
            self._stop_all("no_powder", weight)
            return
        self._open_window(now, weight)
        if self.pid is not None and self.phase == "feed":
            self._pid_reading(now)
        if self.phase != "rest":
            return
        still = max(STILL_LEAST_S, STILL_INTERVALS * cal.interval_s)
        if now < self.stopped_at + self.wait - TIE_S or now < self.last_rise + still - TIE_S:
            return
        if self.hold_until is not None and now < self.hold_until - TIE_S:
            return      # the gate's move at the coarse stop is not over
        weight = self.highest if REST_ON == "highest" else self.lowest
        short = self.target - weight
        tries_left = self.moves + self.streams < MOST_MOVES
        if cal.learnt and not self.learnt_done:
            self.learnt_done = True     # the learnt source learns at the first rest, whatever follows it (G9)
            self._learn_learnt(weight)
        edge = cal.band_gr + BAND_EDGE_DIGITS * cal.digit_gr
        # with no small moves (the learnt source) every rest short of the band goes back to the stream
        back = short - self.stop_room > cal.moves[-1].mean_gr if cal.moves else short > edge
        if not self.rehearsal and tries_left and back:
            # Short by more than the stream's own stop margin and the largest small move together: the stream
            # would pour more from this rest than that move gives, so back to the stream, by the same law.
            # Whatever made the forecast stop the tubes early (it counted a fast coarse stream twice on the
            # bench, 2026-11-21; a hopper that bridged would do the same) costs the throw time and not its
            # weight. Nothing is learnt from such a rest: the stop was not where the margin put it.
            # A rest inside the stop's margin is the moves' to mend, however far it is: a stream begun there
            # stops at once. Measured against the largest move alone, one throw on the bench went back eight
            # times, poured nothing and ended 11 digits short.
            self.streams += 1
            self.phase = "feed"
            self.coarse_on = short > cal.coarse_room_gr + COARSE_LEAST_TURNS * cal.coarse_gr_turn
            self.stopped_at = None
            return
        if self.first_error is None:
            self.first_error = weight - self.target
            self._learn()
            self._learn_fine(weight)
        # a rest lies on the scale's digits; a quarter digit past the band is its edge, so that a reading less a large
        # base, a digit's float off it, falls the right way (the firmware's real_t: 10.02 over 60 gr is 0.0200043)
        if self.rehearsal or short <= cal.band_gr + BAND_EDGE_DIGITS * cal.digit_gr or not tries_left:
            self.log.append((self.first_error, self.moves, self.streams))
            self.done = True
            edge = cal.band_gr + BAND_EDGE_DIGITS * cal.digit_gr
            self.result = "over" if weight - self.target > edge else "short" if short > edge else "good"
            return
        move = self._move_for(short)
        self.moves += 1
        self.phase = "move"
        self.fine_rps = MOVE_RPS
        self.move_turns = move.turns
        self.moved_turns += move.turns
        self.move_until = now + move.turns / MOVE_RPS
        self.wait = move.wait_s

    def _sum_unseen(self) -> None:
        """The forecast's powder on its way: the parcels of each tube with a figure (an unfigured tube's are turns)."""
        figured = {"coarse": self._figured("coarse"), "fine": self._figured("fine")}
        self.unseen = sum(parcel[1] for parcel in self.parcels if figured[parcel[2]])
        self.coarse_unseen = sum(parcel[1] for parcel in self.parcels if parcel[2] == "coarse" and figured["coarse"])

    def _births(self, now: float, weight: float, noise: float) -> bool:
        """The learnt source's figures (F.4): each tube's born from the rise over its turns shown, once enough have
        shown; one that showed nothing for NO_FIGURE_TURNS pours nothing. The fine tube's count begins once the coarse
        tube's powder has all shown. True when the throw ended."""
        cal = self.cal
        if (self.count_mark["fine"] is None and not self._figured("fine") and not self.coarse_on
                and self.backoff_until is None and not any(parcel[2] == "coarse" for parcel in self.parcels)):
            self.count_mark["fine"] = weight
        for tube in ("coarse", "fine"):
            if self._figured(tube) or self.count_mark[tube] is None:
                continue
            rise = weight - self.count_mark[tube]
            if self.shown_turns[tube] >= FIGURE_LEAST_TURNS - 1e-6 and rise > noise:
                figure = rise / self.shown_turns[tube]
                for parcel in self.parcels:
                    if parcel[2] == tube:
                        parcel[1] *= figure     # its turns on their way, forecast at the figure
                if tube == "coarse":
                    cal.coarse_gr_turn = figure
                    self._sum_unseen()
                    self.coarse_birth = (now - self.started_at, weight, weight + self.unseen)
                else:
                    cal.fine_gr_turn = self.fine_gr_measured = self.fine_gr_stream = figure
                    self.learnt_n = 1
                    self._sum_unseen()
            elif self.shown_turns[tube] >= NO_FIGURE_TURNS - 1e-6:
                self._stop_all("no_powder", weight)
                return True
        return False

    def _learn_learnt(self, at_rest: float) -> None:
        """The learnt source's fine figure at a throw's first rest, by `pid_learnt`'s rule (G9): what landed after the
        fine stop over the turns then on their way, held to half and twice the figure, into the throws' running mean."""
        cal = self.cal
        if not self.have_stop or self.stop_window_turns < LEARNT_LEAST_TURNS or cal.fine_gr_turn <= 0.0:
            return
        figure = cal.fine_gr_turn
        seen = (at_rest - self.stop_reading - self.stop_coarse_unseen) / self.stop_window_turns
        seen = min(LEARNT_THROW_MOST * figure, max(LEARNT_THROW_LEAST * figure, seen))
        share = max(LEARN_SHARE, 1.0 / (self.learnt_n + 1))
        cal.fine_gr_turn = figure + share * (seen - figure)
        self.fine_gr_measured = self.fine_gr_stream = cal.fine_gr_turn
        self.learnt_n += 1

    def precharge(self, grains: float, speed_rps: float, now_ms: float) -> None:
        """The precharge's powder held by the closed gate (A12, F.3), given after start(): it falls when the gate
        opens, and is forecast as one coarse parcel due at the coarse lag of the precharge's speed."""
        if grains > 0.0:
            self.parcels.append([now_ms / 1000.0 + table(self.coarse_lags, speed_rps), grains, "coarse"])
            self.unseen += grains
            self.coarse_unseen += grains

    def _stop_all(self, result: str, weight: float) -> None:
        """Both tubes stopped at once and the throw ended, by a limit or a fault: nothing is learnt from it."""
        self.backoff_until = None
        self.coarse_rps = 0.0
        self.fine_rps = 0.0
        self.move_turns = None
        self.coarse_on = False
        self.done = True
        self.result = result
        self.rest_weight = weight

    def _fine_unseen_turns(self) -> float:
        return sum(parcel[1] for parcel in self.parcels if parcel[2] == "fine") / self.cal.fine_gr_turn

    def _open_window(self, now: float, weight: float) -> None:
        """The fine tube's stretch begins at the first reading at which it is slowing, the coarse tube quiet."""
        cal = self.cal
        if self.window_reading is not None or self.rehearsal or self.learn_fine_prior <= 0.0 or self.phase != "feed":
            return
        if not (0.0 < self.fine_rps < self.cal.fine_top_rps):
            return
        if self.coarse_ran and (self.coarse_off_at is None
                                or now < self.coarse_off_at + cal.coarse_stops[0].quiet_s + 0.3 - TIE_S):
            return
        self.window_reading = weight
        self.window_shown_turns = self.turned["fine"] - self._fine_unseen_turns()
        self.window_turned = self.turned["fine"]
        self.window_unseen_gr = sum(parcel[1] for parcel in self.parcels if parcel[2] == "fine")

    def _learn_fine(self, at_rest: float) -> None:
        """The stretch's powder over its turns, into the session's mean."""
        if self.window_reading is None or self.learn_fine_prior <= 0.0:
            return
        if self.fine_form == "split":
            self._learn_fine_split(at_rest)
            return
        turns = self.turned["fine"] - self._fine_unseen_turns() - self.window_shown_turns
        gained = at_rest - self.window_reading
        if turns < LEARN_FINE_LEAST_TURNS or gained <= 0.0:
            return
        self.fine_learn_n += 1
        self.fine_learn_sum += gained / turns
        mean = ((self.learn_fine_prior * self.fine_gr_measured + self.fine_learn_sum)
                / (self.learn_fine_prior + self.fine_learn_n))
        most = LEARN_FINE_MOST * self.fine_gr_measured
        self.cal.fine_gr_turn = min(self.fine_gr_measured + most, max(self.fine_gr_measured - most, mean))

    def _learn_fine_split(self, at_rest: float) -> None:
        """The split form: the stream's figure from what showed before the stretch (the fine tube alone), the
        finish's from the stretch, the stream's powder still unseen at its start taken off."""
        cal = self.cal
        prior, most = self.learn_fine_prior, LEARN_FINE_MOST * self.fine_gr_measured
        clamp = lambda g: min(self.fine_gr_measured + most, max(self.fine_gr_measured - most, g))
        if not self.coarse_ran and self.window_shown_turns >= LEARN_FINE_LEAST_STREAM_TURNS and self.window_reading > 0.0:
            self.stream_learn_n += 1
            self.stream_learn_sum += self.window_reading / self.window_shown_turns
            self.fine_gr_stream = clamp((prior * self.fine_gr_measured + self.stream_learn_sum)
                                        / (prior + self.stream_learn_n))
        unseen_turns_now = sum(parcel[1] for parcel in self.parcels if parcel[2] == "fine") / cal.fine_gr_turn
        turns = self.turned["fine"] - unseen_turns_now - self.window_turned
        gained = at_rest - self.window_reading - self.window_unseen_gr
        if turns < LEARN_FINE_LEAST_TURNS or gained <= 0.0:
            return
        self.fine_learn_n += 1
        self.fine_learn_sum += gained / turns
        cal.fine_gr_turn = clamp((prior * self.fine_gr_measured + self.fine_learn_sum) / (prior + self.fine_learn_n))

    def _move_for(self, short_gr: float) -> Move:
        """The largest move that on average takes the weight no more than half way to the band's top."""
        cal = self.cal
        chosen = cal.moves[0]
        for move in cal.moves[1:]:
            if move.mean_gr <= (short_gr + cal.band_gr) / 2.0:
                chosen = move
        return chosen

    def _learn(self) -> None:
        """A share of the first error at rest moves where the fine tube stops: capped, and nothing from a small one."""
        cal = self.cal
        if self.rehearsal:
            return
        off = (self.first_error - cal.aim_gr) / cal.digit_gr
        if abs(off) < LEARN_DEAD_DIGITS:
            return
        off = max(-LEARN_MOST_DIGITS, min(LEARN_MOST_DIGITS, off))
        self.stop_room += LEARN_SHARE * off * cal.digit_gr


def borrow_stops(cal: Calibration, other: Calibration) -> None:
    """Take the stops, and so the scale's lag at each flow, from a calibration made with another powder.

    The same scale weighed both. A stop is kept at its flow in grains a second: its speed here is that flow over
    this powder's grains a turn.
    """
    for name, mine, theirs in (("coarse_stops", cal.coarse_gr_turn, other.coarse_gr_turn),
                               ("fine_stops", cal.fine_gr_turn, other.fine_gr_turn)):
        setattr(cal, name, [Stop(stop.speed * theirs / mine, stop.unseen_gr, stop.spread_gr, stop.quiet_s, stop.lag_s)
                            for stop in getattr(other, name)])


def shift_lags(cal: Calibration, shift_s: float) -> None:
    """Every stop's lag moved by `shift_s`: a calibration measured where the controller does not run holds that
    place's latency. One made by a PC through the console ran 0.055 s behind the firmware's own (2026-11-22)."""
    for name in ("coarse_stops", "fine_stops"):
        setattr(cal, name, [dataclasses.replace(stop, lag_s=max(0.0, stop.lag_s + shift_s)) for stop in getattr(cal, name)])


def calibrate(rig, seed: int, finish: str, band_gr: float, lag_rig=None, lag_shift_s: float = 0.0,
              gr_turn_factors: tuple = (1.0, 1.0)) -> Calibration:
    """The whole calibration: the open-loop measurements, the rules, then rehearsals that place the stop.

    With `lag_rig`, the scale's part (the stops by flow) is taken from a calibration on that rig, another powder
    on the same scale, as a unit would keep the scale's part from one powder to the next. `lag_shift_s` moves every
    lag after it is measured (`shift_lags`), to see what a calibration made elsewhere does.
    """
    cal = measure(rig, seed)
    if lag_rig is not None:
        borrow_stops(cal, measure(lag_rig, seed + 7))
    if lag_shift_s:
        shift_lags(cal, lag_shift_s)
    # each tube's grains a turn off by these factors, to see what a calibration's error costs (2026-11-22)
    cal.coarse_gr_turn *= gr_turn_factors[0]
    cal.fine_gr_turn *= gr_turn_factors[1]
    set_rules(cal, finish, band_gr)
    rehearse(cal, Session(rig, seed + 1), rig.acceleration_rps2)
    return cal


def rehearse(cal: Calibration, session, acceleration_rps2: float) -> None:
    """The rehearsals: throws of a grain with the fine tube alone, each ended at its first rest, that place the stop.

    Args:
        cal: the calibration with its rules set (`for_finish`); its stop is moved here.
        session: throws on the bench: `throw(controller, target, tolerance)` gives a result with `elapsed_s` and
            `true_gr` (`throw.Session`, or a unit's in `bench/trial.py`).
        acceleration_rps2: the motors' ramp, for the controller's own count of the turns.
    """
    finish = cal.finish
    controller = Staged(cal, acceleration_rps2, rehearsal=True)
    for _ in range(REHEARSALS):
        result = session.throw(controller, REHEARSAL_GR, cal.band_gr + cal.digit_gr / 2.0)
        cal.seconds += result.elapsed_s + 1.0
        cal.poured_gr += result.true_gr
    cal.first_errors = [entry[0] for entry in controller.log]
    if finish == "jog":
        # short by twice the rehearsals' own spread and a digit: few first readings at rest are over
        cal.aim_gr = -(2.0 * statistics.stdev(cal.first_errors) + cal.digit_gr)
    shift = statistics.median(cal.first_errors) - cal.aim_gr
    cal.stop_room_gr += shift       # the margins above the stop move with it
    cal.glide_room_gr += shift
    cal.coarse_room_gr += shift


def make(rig, seed: int, finish: str, band_gr: float, lag_rig=None, lag_shift_s: float = 0.0,
         gr_turn_factors: tuple = (1.0, 1.0)) -> Staged:
    return Staged(calibrate(rig, seed, finish, band_gr, lag_rig, lag_shift_s, gr_turn_factors), rig.acceleration_rps2)


class StagedQuickRest(Staged):
    """A variant talked over on 2026-11-22, tried in the simulation; not a plan (research/talk_2026-11-22.md).

    The staged controller, its rest judged as soon as the forecast holds nothing still to show (under half a digit)
    and the reading has stopped rising, in place of the wait the calibration sets from its slowest stops (`rest_s`,
    and each move's watched wait).
    """

    def on_reading(self, now_ms: float, weight: float) -> None:
        now = now_ms / 1000.0
        if self.phase == "rest" and self.stopped_at is not None:
            unseen = sum(parcel[1] for parcel in self.parcels if parcel[0] > now + TIE_S)
            if unseen < self.cal.digit_gr / 2.0:
                self.wait = min(self.wait, max(0.0, now - self.stopped_at))
        super().on_reading(now_ms, weight)


def make_quick_rest(rig, seed: int, finish: str, band_gr: float) -> Staged:
    return StagedQuickRest(calibrate(rig, seed, finish, band_gr), rig.acceleration_rps2)


# Two modes, talked over on 2026-11-22, tried in the simulation; not a plan (research/talk_2026-11-22.md):
# (band, over budget, the time a throw aims at, judged as soon as the forecast holds nothing)
MODES = {"fast": (0.04, 1.0 / 15.0, 10.0, True), "accurate": (0.02, 1.0 / 30.0, 20.0, False)}
MODE_REHEARSALS = 40
MODE_CAP_TIMES = 2.0            # a throw past twice its mode's time ends short


def tail(values: list, share: float) -> float:
    """The value only `share` of the values lie above, straight between two of them; the largest past the last."""
    ordered = sorted(values)
    position = (1.0 - share) * (len(ordered) - 1)
    low = int(position)
    if low >= len(ordered) - 1:
        return ordered[-1]
    return ordered[low] + (ordered[low + 1] - ordered[low]) * (position - low)


def calibrate_mode(rig, seed: int, mode: str) -> Calibration:
    """The calibration for a mode: the glide's, its rehearsals 40, its stop aimed from their tail."""
    band, budget, _, _ = MODES[mode]
    cal = measure(rig, seed)
    set_rules(cal, "glide", band)
    controller = Staged(cal, rig.acceleration_rps2, rehearsal=True)
    session = Session(rig, seed + 1)
    for _ in range(MODE_REHEARSALS):
        result = session.throw(controller, REHEARSAL_GR, band + cal.digit_gr / 2.0)
        cal.seconds += result.elapsed_s + 1.0
        cal.poured_gr += result.true_gr
    cal.first_errors = [entry[0] for entry in controller.log]
    median = statistics.median(cal.first_errors)
    # A rest is over from the band and half a digit; the stream's stop may give half the budget's overs
    over_edge = band + cal.digit_gr / 2.0
    above = tail([error - median for error in cal.first_errors], budget / 2.0)
    aim = min(-cal.digit_gr / 2.0, over_edge - above)
    shift = median - aim            # the stop moves so that the median first rest lies at the aim
    cal.aim_gr = aim
    cal.stop_room_gr += shift
    cal.glide_room_gr += shift
    cal.coarse_room_gr += shift
    return cal


class ModeStaged(Staged):
    """The staged controller in a mode: its waits, its top-ups and its time cap as the mode says."""

    def __init__(self, cal: Calibration, acceleration_rps2: float, mode: str):
        super().__init__(cal, acceleration_rps2)
        self.mode = mode
        self.band, self.budget, self.aim_s, self.quick = MODES[mode]
        self.name = mode

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.t0 = now_ms / 1000.0

    def on_reading(self, now_ms: float, weight: float) -> None:
        now = now_ms / 1000.0
        if self.quick and self.phase == "rest" and self.stopped_at is not None:
            unseen = sum(parcel[1] for parcel in self.parcels if parcel[0] > now + TIE_S)
            if unseen < self.cal.digit_gr / 2.0:
                self.wait = min(self.wait, max(0.0, now - self.stopped_at))
        if now - self.t0 > MODE_CAP_TIMES * self.aim_s:
            self.moves = max(self.moves, MOST_MOVES)        # no tries left: the next rest ends the throw
        super().on_reading(now_ms, weight)

    def _move_for(self, short_gr: float) -> Move:
        if self.quick:
            return super()._move_for(short_gr)
        # accurate: the largest move whose 90% release fits under what is left and the band; else the smallest
        cal = self.cal
        chosen = cal.moves[0]
        for move in cal.moves[1:]:
            if move.most_gr <= short_gr + cal.band_gr:
                chosen = move
        return chosen


def make_mode(rig, seed: int, mode: str) -> ModeStaged:
    return ModeStaged(calibrate_mode(rig, seed, mode), rig.acceleration_rps2, mode)


class StagedSession(Staged):
    """Two ideas talked over on 2026-11-22, tried in the simulation; not a plan (research/talk_2026-11-22.md).

    - learn_fine: the base controller's learning of the fine tube's grains a turn (LEARN_FINE_PRIOR_ON), on or off. Its
      first form in the talk read the stretch from the coarse tube's quiet; the base controller's reads it from the
      fine tube's slowing, so that a throw of the fine tube alone teaches the finish's figure.
    - owed: before a top-up, the powder its earlier top-ups turned out and the scale has not shown is counted as on
      its way (a run of nudges that showed nothing is followed by what they owed, bench 2026-11-20 and 21). If it
      covers what is short, the throw waits for it instead of turning again.
    """

    def __init__(self, cal: Calibration, acceleration_rps2: float, learn_fine: bool = False, owed: bool = False):
        super().__init__(cal, acceleration_rps2)
        self.owe = owed
        self.learn_fine_prior = LEARN_FINE_PRIOR_ON if learn_fine else 0.0  # the base controller's learning

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.first_rest_reading = None
        self.topup_turns = 0.0

    def on_reading(self, now_ms: float, weight: float) -> None:
        had_first = self.first_error is not None
        super().on_reading(now_ms, weight)
        if not had_first and self.first_error is not None:
            self.first_rest_reading = self.target + self.first_error

    def _move_for(self, short_gr: float) -> Move:
        cal = self.cal
        if self.owe and self.first_rest_reading is not None:
            shown = self.lowest - self.first_rest_reading
            owed = max(0.0, self.topup_turns * cal.fine_gr_turn - shown)
            if short_gr - owed <= cal.band_gr:
                # what it owes covers what is short: wait for it, as long as the slowest move is watched
                return Move(0.0, 0.0, 0.0, max(move.wait_s for move in cal.moves))
            short_gr -= owed
        move = super()._move_for(short_gr)
        self.topup_turns += move.turns
        return move


def make_session(rig, seed: int, band_gr: float, learn_fine: bool, owed: bool, fine_factor: float = 1.0) -> Staged:
    return StagedSession(calibrate(rig, seed, "glide", band_gr, gr_turn_factors=(1.0, fine_factor)),
                         rig.acceleration_rps2, learn_fine, owed)
