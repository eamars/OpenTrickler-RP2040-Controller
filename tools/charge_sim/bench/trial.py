"""Both proposals of 2026-11-21 run on a unit from the PC: their calibration, then their throws. No firmware change.

The PC is the controller. It reads the weight by the console's `state` and turns the tubes by `motor`, as
`record.py` does for the recordings, and runs on that the very code the simulation ran (`../staged.py`): the
calibration's turns, stops and small moves, the rehearsals, then throws of each finish at each band. The scale is
never zeroed: each throw is counted from the weight the cup rests at, so the cup fills as in a recording.

What a unit's own firmware would do better: it acts every millisecond, where the PC acts once a round of console
commands (the weight, the unit's count of its steps, a speed or two: 40 to 80 ms). The turns are the unit's own
count either way (`motor`), and a small move is made to its turn. A proposal that holds from the PC holds there.

The bench rules hold (AGENTS.md): no run before the owner says the bench is ready, powder in the hopper, an empty
cup on a zeroed scale. The tool is one console session; it stops both motors however it ends. If it is killed from
outside, or loses its console, a motor may be left turning: send `motor coarse 0` and `motor fine 0` at once.

Usage, with the repository's venv:
    python trial.py --repo C:\\workspace\\OpenTrickler-RP2040-Controller --name pico2w_bm2
    python trial.py --repo ... --resume <the trial's folder>      go on after the cup was emptied
    python trial.py --repo ... --name pico2w_bm2 --remeasure <an earlier trial's folder>
                                                                   its kept stops and moves measured again, no turn
    python trial.py --made-up pico2w_bm2 --seed 3                  the same against the simulation, no unit
    python trial.py --forecast pico2w_bm2                          what the simulation expects of it: written first
"""

import argparse
import datetime
import json
import math
import random
import statistics
import sys
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import rigs                         # noqa: E402  (../rigs.py)
import staged                       # noqa: E402  (../staged.py)
from bench_twin import TwinBench    # noqa: E402
from throw import Result            # noqa: E402

SETS = (("jog", 0.02), ("glide", 0.02), ("jog", 0.04), ("glide", 0.04))
# The console's stand-in for a tube stopped and held: `motor <tube> 0` switches the driver off, which a charge
# never does, so a held tube is asked for a speed too slow to pour (in a minute: a thousandth of a coarse turn,
# six thousandths of a fine one)
HOLD_RPS = {"coarse": 0.00002, "fine": 0.0001}
# The firmware's step pulses take a new speed only once the steps already handed to the PIO are made: two segments
# at the old rate (`step_pulses.c`), and at a hold's rate a segment is one step of a third of a second or more. So
# a tube started from a hold is switched off first, which stops the pulses at once, and then started: as every
# move of the calibration and of the bench's recordings began. Found on the unit, 2026-11-21: without it eight
# moves of a jog finish in a row released nothing, each over before its first step
RELEASE_FIRST = True
STEPS_A_TURN = {"coarse": 200 * 256 / 1.25, "fine": 200 * 256 / (40.0 / 22.0)}    # microsteps a turn of each tube
SEGMENT_S = 0.002                   # the firmware hands the PIO its steps this far ahead, or one step if that is longer
STOPPED_STEPS_S = 0.05              # a rate under this is a stop (`STEP_PULSES_MIN_RATE`)
COMMAND_S = (0.018, 0.026)          # what one console command takes (47 `state` a second on the bench)
COMMAND_MID_S = 0.022
# A turn shorter than this is made as a small move is: the speed, the wait, the hold, with nothing read in between.
# Read through, a turn ends a command or two late, which is a third of the smallest move
SHORT_RUN_S = 1.0
SPEED_STEP = 0.02                   # a new speed is sent when it differs from the last sent by this share
REST_STILL_S = 1.0                  # the cup is at rest when its reading has not changed for this long
REST_MOST_S = 20.0
WATCH_AFTER_S = 4.0                 # a throw's weight at rest is read this long after its verdict
THROW_MOST_S = 120.0                # a throw not done by then is stopped
OVER_MOST_GR = 3.0                  # a reading this far over the target stops everything: something is wrong
FINE_MOST_TURNS = 15.0              # no throw turns the fine tube further than this
CUP_LIMIT_GR = 600.0                # the cup holds 700 gr and more (the owner, 2026-11-20)
MEASURE_ROOM_GR = 220.0             # the cup's room the calibration's open-loop part is given
FORECAST_SEEDS = 16


class UnitTrouble(Exception):
    """The unit cannot go on: no reply, no reading, a limit passed. Both motors are stopped by whoever catches it."""


class MadeUpUnit:
    """A unit that is the simulation: the console's commands answered by a simulated bench, each taking its time."""

    def __init__(self, rig, seed: int):
        self.bench = TwinBench(rig, seed)
        self.rng = random.Random(seed + 17)
        self.digit, self.interval_s, self.acceleration = rig.scale.digit_gr, rig.scale.report_s, rig.acceleration_rps2
        self.cup_limit_gr = math.inf    # a made-up cup is never full: a forecast is not stopped to empty it
        self.runs = []
        self.asked = {"coarse": 0.0, "fine": 0.0}       # the speed each tube's pulses are made at now
        self.pending = {}                               # tube: (when its new speed is taken, the speed)

    def _advance(self, seconds: float) -> None:
        """Let time pass, each tube taking a speed that waits for it when its moment comes."""
        end = self.bench.now + seconds
        while self.pending:
            tube, (when, rps) = min(self.pending.items(), key=lambda item: item[1][0])
            if when > end:
                break
            self.bench.advance(max(0.0, when - self.bench.now))
            self.bench.tubes[tube].set_speed(rps)
            self.asked[tube] = rps
            del self.pending[tube]
        self.bench.advance(max(0.0, end - self.bench.now))

    def _command(self) -> None:
        self._advance(self.rng.uniform(*COMMAND_S))

    def clock(self) -> float:
        return self.bench.now

    def weight(self) -> float:
        self._command()
        return self.bench.reading

    def set_speed(self, tube: str, rps: float) -> None:
        """As the firmware takes a speed: a stop at once, any other once the steps already handed over are made."""
        self._command()
        rate = abs(self.asked[tube]) * STEPS_A_TURN[tube]
        if rps == 0.0 or rate < STOPPED_STEPS_S:
            wait = 0.0
        else:
            segment = max(SEGMENT_S, 1.0 / rate)
            wait = segment * self.rng.uniform(1.0, 2.0)
        self.pending[tube] = (self.bench.now + wait, rps)
        self._advance(0.0)

    def turned(self, tube: str) -> float:
        return self.turns()[tube]

    def turns(self) -> dict:
        """Each tube's turns by the unit's own count of its steps: one command."""
        self._command()
        return {tube: self.bench.tubes[tube].turns for tube in ("coarse", "fine")}

    def move(self, tube: str, rps: float, turns: float) -> None:
        """A short turn from a tube switched off, then held: nothing is read in between, so it is as long as asked."""
        self.set_speed(tube, rps)
        self._advance(max(0.0, turns / rps - COMMAND_MID_S))
        self.set_speed(tube, HOLD_RPS[tube])

    def sleep(self, seconds: float) -> None:
        self._advance(seconds)

    def ready(self) -> None:
        pass

    def stop(self) -> None:
        self.pending.clear()
        for tube in ("coarse", "fine"):
            self.bench.tubes[tube].set_speed(0.0)
            self.asked[tube] = 0.0

    def close(self) -> int:
        return 0


class RealUnit:
    """A unit on the debug probe, through `record.py`'s session."""

    def __init__(self, repo: Path, name: str, board: str, digit: float, interval_s: float, acceleration: float):
        import record   # beside this file; it finds the checkout's own rc_run.py once the repository is known
        self.record = record
        self.bench = record.Bench(repo, f"trial_{name}", board, tap=False)
        self.digit, self.interval_s, self.acceleration = digit, interval_s, acceleration
        self.cup_limit_gr = CUP_LIMIT_GR
        self.runs = self.bench.runs
        self.folder = self.bench.folder

    def _send(self, line: str) -> str:
        reply = self.bench.session.send(line)
        if reply is None or reply.startswith("@err"):
            raise UnitTrouble(f"\"{line}\": {reply}")
        return reply

    def clock(self) -> float:
        return time.monotonic()

    def weight(self) -> float:
        reply = self._send("state")
        text = reply.split("weight=")[1].split()[0]
        weight = float(text)
        if not math.isfinite(weight):
            raise UnitTrouble("the scale gives no weight")
        return weight

    def set_speed(self, tube: str, rps: float) -> None:
        self._send(f"motor {tube} {rps:.5f}")

    def turned(self, tube: str) -> float:
        return self.turns()[tube]

    def turns(self) -> dict:
        """Each tube's turns by the unit's own count of the steps on its pin: one command."""
        fields = dict(part.split("=") for part in self._send("motor").split() if "=" in part)
        return {tube: int(fields[f"{tube}_made"]) / self.record.MICROSTEPS_PER_MOTOR_TURN * self.record.TUBE_PER_MOTOR[tube]
                for tube in ("coarse", "fine")}

    def move(self, tube: str, rps: float, turns: float) -> None:
        """A short turn from a tube switched off, then held: nothing is read in between, so it is as long as asked."""
        self.set_speed(tube, rps)
        time.sleep(max(0.0, turns / rps - COMMAND_MID_S))
        self.set_speed(tube, HOLD_RPS[tube])

    def sleep(self, seconds: float) -> None:
        time.sleep(seconds)

    def ready(self) -> None:
        """Refuse a unit that is in a charge or away from its menu: the console's `motor` is refused there."""
        fields = dict(part.split("=", 1) for part in self._send("state").split() if "=" in part)
        if int(fields["charge"]) != 0 or int(fields.get("form", "0")) < 0:
            raise UnitTrouble(f"the unit is not at its menu (charge={fields['charge']}, form={fields.get('form')})")

    def stop(self) -> None:
        for tube in ("coarse", "fine"):
            self.bench.session.send(f"motor {tube} 0")

    def close(self) -> int:
        return self.bench.close()


class UnitBench:
    """The calibration's bench on a unit: a turn and a stop as `record.py` makes one, kept as it keeps one."""

    def __init__(self, unit):
        self.unit = unit
        self.started = unit.clock()
        self.base = self.last = unit.weight()
        self.cup = self

    @property
    def now(self) -> float:
        return self.unit.clock() - self.started

    @property
    def landed_gr(self) -> float:
        return self.last - self.base

    def advance(self, seconds: float) -> None:
        self.unit.sleep(seconds)

    def _weight(self) -> tuple:
        """A reading with the time it came at: (time, weight)."""
        self.last = self.unit.weight()
        if self.last > self.unit.cup_limit_gr:
            raise UnitTrouble(f"the cup holds {self.last:.0f} gr: past its limit")
        return self.unit.clock(), self.last

    def creep_stop(self, tube: str, speed_rps: float, run_s: float, watch_s: float, stop: str) -> dict:
        unit = self.unit
        turned_before = unit.turned(tube)
        during = [self._weight()]       # the cup at rest, before the tube turns
        started = unit.clock()
        if run_s < SHORT_RUN_S:
            unit.move(tube, speed_rps, speed_rps * run_s)
        else:
            unit.set_speed(tube, speed_rps)
            started = unit.clock()
            while unit.clock() - started < run_s:
                during.append(self._weight())
            unit.set_speed(tube, HOLD_RPS[tube])
        stopped = unit.clock()
        turned = unit.turned(tube) - turned_before
        after = []
        while unit.clock() - stopped < watch_s:
            after.append(self._weight())
        unit.set_speed(tube, 0.0)
        trial = {"note": "creepstop", "tube": tube, "speed_asked_rps": speed_rps, "stop": stop, "held": True,
                 "run_s": stopped - started, "turns": turned, "stamped": "reply",
                 "during": [(round(t - stopped, 3), w) for t, w in during],
                 "after": [(round(t - stopped, 3), w) for t, w in after]}
        unit.runs.append(trial)
        return trial


class KeptBench:
    """A calibration's kept turns given again in their order: its measurements made anew, and no tube turned."""

    def __init__(self, runs: list, seconds: float, poured_gr: float):
        self.runs = list(runs)
        self.now = seconds
        self.cup = self
        self.landed_gr = poured_gr

    def advance(self, seconds: float) -> None:
        pass

    def creep_stop(self, tube: str, speed_rps: float, run_s: float, watch_s: float, stop: str) -> dict:
        if not self.runs:
            raise UnitTrouble("the kept turns ran out: they are not this calibration's")
        trial = dict(self.runs.pop(0))
        if trial["tube"] != tube or abs(trial["speed_asked_rps"] - speed_rps) > 1e-6 * speed_rps:
            raise UnitTrouble(f"the kept turns are not this calibration's: {trial['tube']} at "
                              f"{trial['speed_asked_rps']} rev/s where {tube} at {speed_rps} was due")
        if trial.get("stamped") != "reply":
            # kept before 2026-11-21's mend: a reading was stamped when it was asked for, a command before it came
            times = [t for t, _ in trial["during"]]
            late = statistics.median(b - a for a, b in zip(times, times[1:])) if len(times) > 2 else COMMAND_MID_S
            trial["during"] = [(t + late, w) for t, w in trial["during"]]
            trial["after"] = [(t + late, w) for t, w in trial["after"]]
        return trial


def measured_again(folder: Path) -> staged.Calibration:
    """A kept trial's calibration made again from its kept turns (`runs.json`), by the rules as they now stand."""
    old = json.loads((folder / "calibration.json").read_text(encoding="utf-8"))
    runs = [run for run in json.loads((folder / "runs.json").read_text(encoding="utf-8")) if run.get("note") == "creepstop"]
    bench = KeptBench(runs, old["seconds"], old["poured_gr"])
    return staged.measure_on(bench, old["digit_gr"], old["interval_s"])


class UnitSession:
    """Throws on a unit: the controller's ticks and readings come from the console, its speeds go to it."""

    def __init__(self, unit):
        self.unit = unit

    def rest_weight(self) -> float:
        """The cup's weight once its reading has stood still: what the next throw is counted from.

        Still is a second of readings within one digit of each other: a reading on the edge of a digit flickers
        between two and is at rest all the same. The weight is their middle one, so it lies on the scale's digits.
        """
        unit = self.unit
        asked = unit.clock()
        window = []
        while True:
            window.append((unit.clock(), unit.weight()))
            while max(w for _, w in window) - min(w for _, w in window) > 1.01 * unit.digit:
                window.pop(0)
            if window[-1][0] - window[0][0] >= REST_STILL_S:
                return statistics.median_low(w for _, w in window)
            if window[-1][0] - asked > REST_MOST_S:
                raise UnitTrouble("the cup's weight does not come to rest")

    def throw(self, controller, target_gr: float, tolerance_gr: float) -> Result:
        """One throw to its verdict and its weight at rest; stopped at once by any limit."""
        unit = self.unit
        base = self.rest_weight()
        if base + target_gr + OVER_MOST_GR > unit.cup_limit_gr:
            raise UnitTrouble("no room in the cup for a throw")
        coarse_most = target_gr / controller.cal.coarse_gr_turn + 1.5
        sent = {"coarse": 0.0, "fine": 0.0}
        held = {"coarse": False, "fine": False}     # the last throw's end switched both off
        rows = []
        turns_before = unit.turns()
        started = unit.clock()
        controller.start(target_gr, 0.0)
        try:
            while True:
                weight = unit.weight() - base
                controller.on_reading((unit.clock() - started) * 1000.0, weight)
                turns = unit.turns()
                now = unit.clock() - started
                controller.count(now * 1000.0, {tube: turns[tube] - turns_before[tube] for tube in turns})
                controller.on_tick(now * 1000.0)
                rows.append((round(now, 3), round(weight, 3), controller.coarse_rps, round(controller.fine_rps, 4),
                             round(weight + controller.unseen, 3), round(controller.turned["coarse"], 4),
                             round(controller.turned["fine"], 4)))
                if controller.done:
                    break
                if now > THROW_MOST_S:
                    raise UnitTrouble(f"a throw not done in {THROW_MOST_S:.0f} s")
                if weight > target_gr + OVER_MOST_GR:
                    raise UnitTrouble(f"the reading is {weight - target_gr:.2f} gr over the target")
                if controller.turned["coarse"] > coarse_most or controller.turned["fine"] > FINE_MOST_TURNS:
                    raise UnitTrouble("a tube turned further than a throw can need")
                if controller.move_turns is not None:       # a small move: made to its turn, nothing read meanwhile
                    if RELEASE_FIRST and held["fine"]:
                        unit.set_speed("fine", 0.0)
                    unit.move("fine", staged.MOVE_RPS, controller.move_turns)
                    controller.moved((unit.clock() - started) * 1000.0)
                    sent["fine"], held["fine"] = 0.0, True
                    continue
                for tube, wanted in (("coarse", controller.coarse_rps), ("fine", controller.fine_rps)):
                    if wanted != sent[tube] and (wanted == 0.0 or sent[tube] == 0.0
                                                 or abs(wanted - sent[tube]) > SPEED_STEP * sent[tube]):
                        if RELEASE_FIRST and wanted > 0.0 and held[tube]:
                            unit.set_speed(tube, 0.0)       # from a hold: off first, so the new speed is taken at once
                        unit.set_speed(tube, wanted if wanted > 0.0 else HOLD_RPS[tube])
                        sent[tube], held[tube] = wanted, wanted == 0.0
            done_at = unit.clock() - started
            for tube in ("coarse", "fine"):     # held through the watch, as through the throw
                if sent[tube] != 0.0:
                    unit.set_speed(tube, HOLD_RPS[tube])
            watched = []
            while unit.clock() - started < done_at + WATCH_AFTER_S:
                watched.append((unit.clock() - started, unit.weight() - base))
        finally:
            unit.stop()
        rest = statistics.median(weight for t, weight in watched if t >= done_at + WATCH_AFTER_S - 0.6)
        verdict = "over" if rest - target_gr >= tolerance_gr else "under" if target_gr - rest >= tolerance_gr else "good"
        rows.extend((round(t, 3), round(weight, 3), 0.0, 0.0, round(weight, 3), rows[-1][5], rows[-1][6])
                    for t, weight in watched)
        return Result(target=target_gr, elapsed_s=done_at, reading=rows[len(rows) - len(watched) - 1][1], true_gr=rest,
                      verdict=verdict, coarse_gr=0.0, fine_gr=0.0, rows=rows)


def percentile(values: list, share: float) -> float:
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(share * len(ordered)))]


def summed(throws: list, band_gr: float, digit: float, aim_gr: float) -> dict:
    """A set of throws as the figures the forecast is written in. Good is within the band on the weight at rest.

    `first_near` is the share of throws whose first rest lay within a digit of where the stop was aimed.
    """
    edge = band_gr + digit / 2.0
    errors = [throw["rest_error_gr"] for throw in throws]
    times = [throw["elapsed_s"] for throw in throws]
    count = len(throws)
    return {"throws": count,
            "good": sum(1 for error in errors if abs(error) < edge) / count,
            "over": sum(1 for error in errors if error >= edge) / count,
            "under": sum(1 for error in errors if error <= -edge) / count,
            "time_median_s": statistics.median(times), "time_most_s": percentile(times, 0.95),
            "first_error_digits": statistics.fmean(throw["first_error_gr"] for throw in throws) / digit,
            "first_spread_digits": statistics.pstdev(throw["first_error_gr"] for throw in throws) / digit,
            "first_near": sum(1 for throw in throws if abs(throw["first_error_gr"] - aim_gr) <= 1.01 * digit) / count,
            "moves": statistics.fmean(throw["moves"] for throw in throws),
            "streams": statistics.fmean(throw.get("streams", 0) for throw in throws)}


def cup_room(unit, needed_gr: float, what: str) -> None:
    """Stop before a step the cup has no room for; the trial goes on from there once the cup is emptied."""
    holds = unit.weight()
    if holds + needed_gr > unit.cup_limit_gr:
        raise UnitTrouble(f"the cup holds {holds:.0f} gr and {what} needs {needed_gr:.0f}: empty it, put it back, "
                          f"and go on with --resume")


def run_trial(unit, folder: Path, target_gr: float, throws_a_set: int, say=print) -> dict:
    """
    The whole trial on a unit, kept in a folder as it goes, so that it can go on after a stop.

    Args:
        unit: a RealUnit or a MadeUpUnit.
        folder: where the calibration, each set's throws and the summary are kept, and read from when going on.
        target_gr: each throw's target.
        throws_a_set: throws of each finish at each band.
        say: prints a line of progress.

    Returns:
        the summary: each set's figures.
    """
    folder.mkdir(parents=True, exist_ok=True)
    unit.ready()
    unit.stop()     # whatever an earlier session left: nothing turns before the first weight is taken
    measured_file = folder / "calibration.json"
    if measured_file.exists():
        measured = staged.from_json(json.loads(measured_file.read_text(encoding="utf-8")))
        say(f"the calibration's measurements are those kept in {measured_file.name}")
    else:
        cup_room(unit, MEASURE_ROOM_GR, "the calibration")
        say("the calibration's measurements: prime, the probe, grains a turn, the stops by flow, the small moves")
        measured = staged.measure_on(UnitBench(unit), unit.digit, unit.interval_s)
        measured_file.write_text(json.dumps(staged.to_json(measured), indent=1), encoding="utf-8")
        say(f"   {measured.seconds / 60.0:.1f} min, {measured.poured_gr:.0f} gr; coarse {measured.coarse_gr_turn:.2f} and fine "
            f"{measured.fine_gr_turn:.3f} gr a turn; every watch {measured.watch_s:.1f} s")
    session = UnitSession(unit)
    summary = {}
    for finish, band in SETS:
        name = f"{finish}_{band:g}"
        set_file = folder / f"throws_{name}.json"
        kept = json.loads(set_file.read_text(encoding="utf-8")) if set_file.exists() else None
        if kept is None:
            cup_room(unit, staged.REHEARSALS * staged.REHEARSAL_GR + 5.0, f"the rehearsals of {name}")
            cal = staged.for_finish(measured, finish, band)
            staged.rehearse(cal, session, unit.acceleration)
            kept = {"finish": finish, "band_gr": band, "target_gr": target_gr, "calibration": staged.to_json(cal),
                    "stop_room_gr": cal.stop_room_gr, "throws": []}
            set_file.write_text(json.dumps(kept), encoding="utf-8")
            say(f"{name}: rehearsed; the first rests (digits) "
                f"{[round(error / cal.digit_gr) for error in cal.first_errors]}; the fine tube's floor "
                f"{cal.floor_rps:.3f} rev/s, its stop {cal.stop_room_gr / cal.digit_gr:+.1f} digits of room")
        cal = staged.from_json(kept["calibration"])
        controller = staged.Staged(cal, unit.acceleration)
        controller.stop_room = kept["stop_room_gr"]     # what the throws so far have taught
        while len(kept["throws"]) < throws_a_set:
            cup_room(unit, target_gr + OVER_MOST_GR, f"a throw of {name}")
            result = session.throw(controller, target_gr, band + cal.digit_gr / 2.0)
            first, moves, streams = controller.log[-1]
            kept["throws"].append({"elapsed_s": result.elapsed_s, "rest_error_gr": result.true_gr - target_gr,
                                   "shown_error_gr": result.reading - target_gr, "first_error_gr": first,
                                   "moves": moves, "streams": streams, "verdict": result.verdict, "rows": result.rows})
            kept["stop_room_gr"] = controller.stop_room
            set_file.write_text(json.dumps(kept), encoding="utf-8")
            say(f"{name} throw {len(kept['throws']):2d}: {result.elapsed_s:5.1f} s, first rest "
                f"{first / cal.digit_gr:+.0f} digits, {moves} moves{f', back to the stream {streams}x' if streams else ''}, "
                f"at rest {(result.true_gr - target_gr) / cal.digit_gr:+.1f} digits: {result.verdict}")
        summary[name] = summed(kept["throws"], band, cal.digit_gr, cal.aim_gr)
        figures = summary[name]
        say(f"{name}: good {figures['good']:.0%}, over {figures['over']:.0%}, under {figures['under']:.0%}; "
            f"{figures['time_median_s']:.1f} s, one in twenty over {figures['time_most_s']:.1f} s; "
            f"{figures['moves']:.1f} moves a throw, back to the stream {figures['streams']:.2f} times; the first "
            f"rest within a digit of its aim in {figures['first_near']:.0%}")
    (folder / "trial.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    return summary


def made_up_trial(job: tuple) -> dict:
    """One whole trial against the simulation, for the forecast: nothing kept but its summary."""
    record, seed, target_gr, throws_a_set, folder = job
    unit = MadeUpUnit(rigs.get_rig(record), seed)
    return run_trial(unit, Path(folder) / f"seed_{seed}", target_gr, throws_a_set, say=lambda line: None)


def forecast(record: str, target_gr: float, throws_a_set: int, out: Path, scratch: Path) -> dict:
    """What the simulation expects of the trial: the same trial against it many times, each figure's range."""
    jobs = [(record, 100 + index, target_gr, throws_a_set, str(scratch)) for index in range(FORECAST_SEEDS)]
    with ProcessPoolExecutor(max_workers=FORECAST_SEEDS // 2) as pool:
        trials = list(pool.map(made_up_trial, jobs))
    ranges = {}
    for name in trials[0]:
        ranges[name] = {}
        for figure in trials[0][name]:
            values = sorted(trial[name][figure] for trial in trials)
            ranges[name][figure] = {"low": values[1], "middle": statistics.median(values), "high": values[-2]}
    kept = {"record": record, "target_gr": target_gr, "throws_a_set": throws_a_set, "trials": FORECAST_SEEDS,
            "what": "each figure's middle over the trials, and the range all but the lowest and the highest fell in",
            "sets": ranges}
    out.write_text(json.dumps(kept, indent=1), encoding="utf-8")
    return kept


def compare(summary: dict, expected: dict, say=print) -> None:
    """Each of the trial's figures beside the forecast's range."""
    for name, figures in summary.items():
        for figure in ("good", "over", "first_near", "time_median_s", "first_spread_digits", "moves"):
            row = expected["sets"][name][figure]
            inside = row["low"] <= figures[figure] <= row["high"]
            say(f"  {name:10s} {figure:20s} {figures[figure]:7.2f}   forecast {row['low']:.2f} to {row['high']:.2f} "
                f"(middle {row['middle']:.2f})   {'inside' if inside else 'OUTSIDE'}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", type=Path, help="the checkout whose rc_run.py and build match the unit's firmware")
    parser.add_argument("--name", help="the trial's name: the unit and the powder, as a record's (pico2w_bm2)")
    parser.add_argument("--board", default="pico2_w")
    parser.add_argument("--resume", type=Path, help="a trial's folder, to go on from where it stopped")
    parser.add_argument("--calibration", type=Path, help="an earlier trial's calibration.json: its measurements "
                                                         "are taken, and the sets begin afresh")
    parser.add_argument("--remeasure", type=Path, help="an earlier trial's folder: its kept turns measured again by "
                                                       "the rules as they now stand, and the sets begin afresh")
    parser.add_argument("--made-up", help="a record: the trial against the simulation, no unit")
    parser.add_argument("--forecast", help="a record: write what the simulation expects of the trial")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--target", type=float, default=10.0)
    parser.add_argument("--throws", type=int, default=25, help="of each finish at each band")
    parser.add_argument("--digit", type=float, default=0.02, help="the scale's digit, in grains")
    parser.add_argument("--interval", type=float, default=0.048, help="the scale's usual time between readings, s")
    parser.add_argument("--acceleration", type=float, default=50.0, help="the motors' stored acceleration, rev/s2")
    parser.add_argument("--out", type=Path, default=HERE.parent / "fit", help="where forecasts and made-up trials go")
    args = parser.parse_args()

    if args.forecast:
        out = args.out / f"trial_forecast_{args.forecast}.json"
        kept = forecast(args.forecast, args.target, args.throws, out, args.out / "trial_made_up" / args.forecast)
        print(f"wrote {out}: {kept['trials']} trials of {kept['throws_a_set']} throws a set at {kept['target_gr']:g} gr")
        for name, figures in kept["sets"].items():
            print(f"  {name:10s} " + "; ".join(f"{figure} {row['low']:.2f} to {row['high']:.2f}"
                                               for figure, row in figures.items() if figure != "throws"))
        return 0

    if args.made_up:
        unit = MadeUpUnit(rigs.get_rig(args.made_up), args.seed)
        folder = args.resume or args.out / "trial_made_up" / f"{args.made_up}_seed{args.seed}"
        name = args.made_up
    else:
        if args.repo is None or (args.name is None and args.resume is None):
            parser.error("a unit's trial needs --repo, and --name or --resume")
        name = args.name or args.resume.name.split("_trial_", 1)[1]
        unit = RealUnit(args.repo, name, args.board, args.digit, args.interval, args.acceleration)
        folder = args.resume or unit.folder
    if args.calibration:
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "calibration.json").write_text(args.calibration.read_text(encoding="utf-8"), encoding="utf-8")
    if args.remeasure:
        folder.mkdir(parents=True, exist_ok=True)
        again = staged.to_json(measured_again(args.remeasure))
        (folder / "calibration.json").write_text(json.dumps(again, indent=1), encoding="utf-8")
    stamp = datetime.datetime.now().strftime("%H:%M:%S")
    print(f"{stamp} the trial of {name}: {args.throws} throws a set at {args.target:g} gr, kept in {folder}", flush=True)
    summary = None
    try:
        summary = run_trial(unit, folder, args.target, args.throws, say=lambda line: print(line, flush=True))
    except UnitTrouble as trouble:
        print(f"STOPPED: {trouble}", flush=True)
    except KeyboardInterrupt:
        print("STOPPED by the keyboard", flush=True)
    finally:
        try:
            unit.stop()
        except Exception as error:      # the session's own end must not hide that a motor may still be turning
            print(f"BOTH MOTORS MAY STILL BE TURNING ({error}): send `motor coarse 0` and `motor fine 0` now", flush=True)
        code = unit.close()
    expected_file = args.out / f"trial_forecast_{name}.json"
    if summary is not None and expected_file.exists():
        print(f"the trial beside the simulation's forecast ({expected_file.name}):")
        compare(summary, json.loads(expected_file.read_text(encoding="utf-8")))
    return code if summary is not None else 1


if __name__ == "__main__":
    sys.exit(main())
