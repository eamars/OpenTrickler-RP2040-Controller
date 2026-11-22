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
# The fine tube's stops are measured at these flows, in digits a second: the scale's lag is taken at each
FINE_RATES_DIGITS_S = ((1.0, 8, 5.0), (2.0, 8, 5.0), (4.0, 4, 3.5), (8.0, 4, 3.0), (16.0, 4, 3.0), (32.0, 4, 3.0))
COARSE_STOPS = ((COARSE_FLOOR_RPS, 3, 1.5), (1.0, 2, 1.5), (COARSE_TOP_RPS, 2, 1.2))
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
LEARN_SHARE = 0.15              # of a throw's first error at rest, into where the fine tube stops
LEARN_MOST_DIGITS = 2.0         # no throw teaches more than this
LEARN_DEAD_DIGITS = 0.25        # nor less
# Two times this close are the same time. The simulation's times and a record's numbers lie on a millisecond's grid, so
# a parcel falls due on a reading's very millisecond often, and which way such a tie falls must not hang on the last
# bit of a float: the firmware's port (src/charge_control.c), in single precision, breaks it the same way (ADR-009)
TIE_S = 1e-4
BAND_EDGE_DIGITS = 0.25        # a rest within the band and this much of a digit is in it


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
    seconds: float = 0.0            # how long the calibration took, and the powder it poured
    poured_gr: float = 0.0
    first_errors: list = field(default_factory=list)


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


def table(points: list, x: float) -> float:
    """A measured table read at x, straight between two points and the end point's outside them."""
    if x <= points[0][0]:
        return points[0][1]
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return points[-1][1]


def measure_on(bench, digit: float, interval_s: float) -> Calibration:
    """The calibration's open-loop part: prime, each tube's powder a turn, its stops by flow, and the small moves.

    Args:
        bench: turns a tube and reads the scale: `creep_stop(tube, speed, run_s, watch_s, "held")` gives a trial
            as `bench/record.py` saves one, `advance(seconds)` lets time pass, `now` is its clock and
            `cup.landed_gr` the powder in the cup.
        digit: the scale's digit, in grains.
        interval_s: the usual time between two readings.
    """
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

    fine_stops = []
    for rate, count, run_s in FINE_RATES_DIGITS_S:
        speed = min(FINE_TOP_RPS, rate * digit / fine_gr)
        if not fine_stops or speed > fine_stops[-1].speed:
            fine_stops.append(stops_at(bench, "fine", speed, count, run_s, watch_s, digit))
    if fine_stops[-1].speed < FINE_TOP_RPS:
        fine_stops.append(stops_at(bench, "fine", FINE_TOP_RPS, 4, 3.0, watch_s, digit))
    # the coarse tube's powder a turn, run by run: its spread is how far a count of its turns can be off
    coarse_yields = [run["gain"] / run["turns"] for run in coarse_runs]
    coarse_stops = [stops_at(bench, "coarse", speed, count, run_s, watch_s, digit, coarse_yields)
                    for speed, count, run_s in COARSE_STOPS]

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
    return Calibration(digit_gr=digit, interval_s=interval_s, coarse_gr_turn=coarse_gr, fine_gr_turn=fine_gr,
                       coarse_stops=coarse_stops, fine_stops=fine_stops, moves=moves, watch_s=watch_s,
                       coarse_share=statistics.stdev(coarse_yields) / statistics.fmean(coarse_yields),
                       seconds=bench.now, poured_gr=bench.cup.landed_gr)


def measure(rig, seed: int) -> Calibration:
    """The open-loop part on the simulation's bench."""
    return measure_on(TwinBench(rig, seed), rig.scale.digit_gr, rig.scale.report_s)


def for_finish(measured: Calibration, finish: str, band_gr: float) -> Calibration:
    """A calibration for one finish and band from what was measured once: its own copy, with the rules set."""
    cal = dataclasses.replace(measured, first_errors=[])
    set_rules(cal, finish, band_gr)
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


def set_rules(cal: Calibration, finish: str, band_gr: float) -> None:
    """The controller's numbers from the measured ones. Nothing here is fitted to a powder, a tube or a scale."""
    cal.finish, cal.band_gr = finish, band_gr
    fine, coarse = cal.fine_stops, cal.coarse_stops
    lags = lag_table(fine, cal.fine_gr_turn)
    spread = [(stop.speed, stop.spread_gr) for stop in fine]
    quiet = [(stop.speed, stop.quiet_s) for stop in fine]
    if finish == "glide":
        # The creep: the flow at which the powder still on its way is about the band, at the lag measured there
        slowest_lag = max(lags[0][1], 0.3)
        cal.floor_rps = min(max(CREEP_BAND_SHARE * band_gr / slowest_lag / cal.fine_gr_turn, 0.5 * fine[0].speed),
                            fine[2].speed)
        # The first reading at rest is meant to lie a digit under the target (half the band, if that is more): a
        # throw that rests short is topped up, one that rests over is lost. Aimed at the target itself the trial
        # gave 5 points fewer good throws and half as many overs again (progress.md, 2026-11-21)
        cal.aim_gr = -max(cal.digit_gr, band_gr / 2.0)
        cal.stop_room_gr = -cal.aim_gr
    else:
        cal.floor_rps = min(FINE_TOP_RPS, DRIBBLE_DIGITS_S * cal.digit_gr / cal.fine_gr_turn)
        cal.stop_room_gr = 2.5 * table(spread, cal.floor_rps) + cal.digit_gr
        cal.aim_gr = -cal.stop_room_gr
    # The floor is reached with twice the spread of a stop from four times its flow still to go
    cal.glide_room_gr = cal.stop_room_gr + (2.0 * table(spread, 4.0 * cal.floor_rps) if finish == "glide" else 0.0)
    if finish == "glide":
        # On a scale so late that such a creep would take too long, the creep is quicker and more is left to the moves
        stretch = cal.glide_room_gr - cal.stop_room_gr
        cal.floor_rps = max(cal.floor_rps, stretch / CREEP_MOST_S / cal.fine_gr_turn)
    cal.glide_s = min(2.0, max(0.4, 0.6 * max(lag for _, lag in lags[2:])))
    coarse_lag = lag_table(coarse, cal.coarse_gr_turn)
    cal.coarse_glide_s = min(1.5, max(0.3, 0.6 * coarse_lag[0][1]))
    cal.coarse_room_gr = 3.0 * coarse[0].spread_gr + cal.glide_room_gr + 0.2
    # The wait before the first judgement: the slowest the reading came to rest after a stop from near the floor
    cal.rest_s = max(stop.quiet_s for stop in fine if stop.speed <= 2.0 * cal.floor_rps or stop is fine[0]) + 0.3


class Staged:
    """The controller. The harness reads coarse_rps, fine_rps and done after each call."""

    def __init__(self, cal: Calibration, acceleration_rps2: float, rehearsal: bool = False):
        self.cal = cal
        self.acceleration = acceleration_rps2
        self.rehearsal = rehearsal      # end at the first reading at rest: the calibration's own throws
        self.coarse_lags = lag_table(cal.coarse_stops, cal.coarse_gr_turn)
        self.fine_lags = lag_table(cal.fine_stops, cal.fine_gr_turn)
        self.stop_room = cal.stop_room_gr
        self.name = cal.finish
        # per throw: (the error at the rest after its last stream, moves made, times it went back to the stream)
        self.log = []

    def start(self, target_gr: float, now_ms: float) -> None:
        cal = self.cal
        self.target = target_gr
        self.done = False
        self.coarse_rps = 0.0
        self.fine_rps = 0.0
        self.phase = "feed"
        # a charge too small for the coarse tube's margin is the fine tube's alone
        self.coarse_on = target_gr > cal.coarse_room_gr + 0.5 * cal.coarse_gr_turn
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
            if turned > 0.0:
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
        if self.phase == "feed":
            self._feed(now)
        elif self.phase == "move" and now >= self.move_until - TIE_S:
            self._end_move(now)

    def _feed(self, now: float) -> None:
        cal = self.cal
        # The coarse tube's powder still unseen is known only by its turns, and a turn's powder varies: three times
        # that spread of it is kept clear by both tubes. A small charge is all unseen when the coarse tube stops,
        # and its margin is the wider for it; in a large one the reading has caught up and little is added
        room = self.target - self._forecast() - COARSE_SPREADS * cal.coarse_share * self.coarse_unseen
        if self.coarse_on:
            if room <= cal.coarse_room_gr:
                self.coarse_on = False
                self.coarse_rps = 0.0
            else:
                asked = (room - cal.coarse_room_gr) / (cal.coarse_gr_turn * cal.coarse_glide_s)
                self.coarse_rps = min(COARSE_TOP_RPS, max(COARSE_FLOOR_RPS, asked))
        if not self.coarse_on and room <= self.stop_room:
            self.fine_rps = 0.0
            if self.coarse_unseen >= cal.digit_gr:
                return      # a pause, not its stop: the fine tube's stop is taken on its own powder alone
            self.phase = "rest"
            self.stopped_at = now
            self.wait = cal.rest_s
            return
        asked = (room - cal.glide_room_gr) / (cal.fine_gr_turn * cal.glide_s)
        self.fine_rps = min(FINE_TOP_RPS, max(cal.floor_rps, asked))

    def on_reading(self, now_ms: float, weight: float) -> None:
        now = now_ms / 1000.0
        cal = self.cal
        if weight >= self.highest + cal.digit_gr / 2.0:
            self.highest = weight
            self.lowest = weight        # the lowest shown since that rise: a reading that flickers shows both
            self.last_rise = now
        self.lowest = min(self.lowest, weight)
        self.reading = weight
        if self.parcels:                # what was due to show by now is in this reading
            self.parcels = [parcel for parcel in self.parcels if parcel[0] > now + TIE_S]
            self.unseen = sum(parcel[1] for parcel in self.parcels)
            self.coarse_unseen = sum(parcel[1] for parcel in self.parcels if parcel[2] == "coarse")
        if self.phase != "rest":
            return
        still = max(0.4, 2.0 * cal.interval_s)
        if now < self.stopped_at + self.wait - TIE_S or now < self.last_rise + still - TIE_S:
            return
        weight = self.highest if REST_ON == "highest" else self.lowest
        short = self.target - weight
        tries_left = self.moves + self.streams < MOST_MOVES
        if not self.rehearsal and tries_left and short - self.stop_room > cal.moves[-1].mean_gr:
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
            self.coarse_on = short > cal.coarse_room_gr + 0.5 * cal.coarse_gr_turn
            self.stopped_at = None
            return
        if self.first_error is None:
            self.first_error = weight - self.target
            self._learn()
        # a rest lies on the scale's digits; a quarter digit past the band is its edge, so that a reading less a large
        # base, a digit's float off it, falls the right way (the firmware's real_t: 10.02 over 60 gr is 0.0200043)
        if self.rehearsal or short <= cal.band_gr + BAND_EDGE_DIGITS * cal.digit_gr or not tries_left:
            self.log.append((self.first_error, self.moves, self.streams))
            self.done = True
            return
        move = self._move_for(short)
        self.moves += 1
        self.phase = "move"
        self.fine_rps = MOVE_RPS
        self.move_turns = move.turns
        self.move_until = now + move.turns / MOVE_RPS
        self.wait = move.wait_s

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
