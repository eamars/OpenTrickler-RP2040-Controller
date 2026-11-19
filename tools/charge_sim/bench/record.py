"""Open-loop recordings on a real unit through the debug probe, for ADR-009's bench session (bench_guide.md).

Each run turns one tube at a speed for a time with the scale's tap on, and keeps the A&D's frames and the motor's
step counts, both on the unit's own clock. Uses the console only (`motor`, `scale tap`): no firmware change.

Usage, with the repository's venv, the unit on the probe and the bench rules kept:
    python record.py --repo C:\\workspace\\OpenTrickler-RP2040-Controller chain
    python record.py --repo ... nudges --count 100 --pause-ms 40 --still-s 1.5
    python record.py --repo ... runs --tube fine --speeds 0.1,0.5,2 --repeats 3 --limit-gr 5 --limit-s 60
    python record.py --repo ... steps --tube fine --plan 0.1:60,2:20
    python record.py --repo ... creepstop --speeds 0.05,0.1 --count 30
    python record.py --repo ... throws --host <address> --target 42.5 --profile 1 --count 10
Output: <repo>/logs/adr009/<time>_<name>/frames.csv (t_ms, weight, header) and runs.json (one entry a run).
"""

import argparse
import datetime
import json
import random
import re
import statistics
import sys
import time
from pathlib import Path

MICROSTEPS_PER_MOTOR_TURN = 200 * 256
# Tube turns per motor turn, from the teeth: 40 on the motor, 32 on the large tube, 22 on the low-flow small one.
# The unit's stored ratio is not used: the Pico 2 W's fine one was a generation-1 figure on 2026-11-19.
TUBE_PER_MOTOR = {"coarse": 40.0 / 32.0, "fine": 40.0 / 22.0}
CUP_LIMIT_GR = 260.0
# The flick test's kinds, as the fine tube's speeds one after another. Each ends one forward move further on
# than it began, as far as the PC's timing holds: a plain nudge; the same with two small swings back and forth;
# the same with two wide ones; and a step back before two forward.
FLICKS = {"nudge": [0.2],
          "shake": [0.2, -0.2, 0.2, -0.2, 0.2],
          "shake_wide": [0.2, -0.6, 0.6, -0.6, 0.6],
          "retract": [-0.2, 0.2, 0.2]}
PRECONDITION_RPS = 1.0
# A speed that keeps the driver switched on with the tube as good as still (0.2 degrees a second): the console's
# `motor fine 0` switches the driver off, which a charge never does between its start and its end.
HOLD_RPS = 0.0005
DIGIT_GR = 0.02
# The charge's states as the console's `state` gives them (charge_mode.h), and its default stop threshold
CHARGE_EXIT, CHARGE_WAIT_FOR_ZERO, CHARGE_WAIT_FOR_COMPLETE = 0, 1, 2
CHARGE_WAIT_FOR_CUP_REMOVAL, CHARGE_WAIT_FOR_CUP_RETURN = 3, 4
FINE_STOP_THRESHOLD_GR = 0.03


class Bench:
    """A session on the unit: commands, the tap's frames, and the runs made."""

    def __init__(self, repo: Path, name: str, board: str, tap: bool = True):
        sys.path.insert(0, str(repo / "tools" / "remote_control"))
        import rc_run   # the checkout's own runner, of the firmware's time: found only once the repo is known
        self.session = rc_run.Session(argparse.Namespace(board=board, elf=None), f"adr009_{name}")
        stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.folder = repo / "logs" / "adr009" / f"{stamp}_{name}"
        self.folder.mkdir(parents=True)
        self.runs = []
        self.charging = False
        self.stored_ratio = {}
        for tube in ("coarse", "fine"):
            self.session.send(f"setting settings/motors/{tube}")
            for line in self.session.console.data_lines:
                match = re.match(r"@setting gear_ratio=([\d.]+)", line)
                if match:
                    self.stored_ratio[tube] = float(match.group(1))
        if tap:
            self.session.send("scale tap on")

    def motor_stats(self) -> dict:
        """The unit's clock and the steps each motor has made."""
        reply = self.session.send("motor")
        fields = dict(re.findall(r"(\w+)=([\d.]+)", reply))
        return {"t_ms": int(fields["t_us"]) / 1000.0, "coarse": int(fields["coarse_made"]),
                "fine": int(fields["fine_made"])}

    def weight(self) -> float:
        """The scale's reading now, by the console's state."""
        reply = self.session.send("state")
        return float(re.search(r"weight=(-?[\d.]+|nan|inf)", reply).group(1))

    def turn(self, tube: str, speed_rps: float, run_s: float, still_s: float, note: str = "",
             limit_gr: float = None) -> dict:
        """
        One run: the tube at a speed for a time (or until a weight has fallen), then still.

        Args:
            tube: coarse or fine.
            speed_rps: the speed asked of the unit, in its own rev/s (its stored gear ratio applies).
            run_s: how long to turn.
            still_s: how long to stay still after, for the scale and the late kernels.
            note: kept with the run.
            limit_gr: stop early once the reading has risen by this much.

        Returns:
            The run's facts: clock and steps before, at the stop and after, and the true turns of the tube.
        """
        before = self.motor_stats()
        start_weight = self.weight() if limit_gr is not None else None
        self.session.send(f"motor {tube} {speed_rps:g}")
        started = time.monotonic()
        if limit_gr is None:
            time.sleep(run_s)
        else:
            while time.monotonic() - started < run_s:
                time.sleep(0.2)
                if self.weight() - start_weight >= limit_gr:
                    break
        at_stop = self.motor_stats()
        self.session.send(f"motor {tube} 0")
        stopped = self.motor_stats()
        time.sleep(still_s)
        after = self.motor_stats()
        steps = after[tube] - before[tube]
        run = {"tube": tube, "speed_asked_rps": speed_rps, "note": note,
               "t_before_ms": before["t_ms"], "t_stop_asked_ms": at_stop["t_ms"], "t_stopped_ms": stopped["t_ms"],
               "t_after_ms": after["t_ms"], "steps": steps, "steps_at_stop_ask": at_stop[tube] - before[tube],
               "turns": steps / MICROSTEPS_PER_MOTOR_TURN * TUBE_PER_MOTOR[tube],
               "stored_gear_ratio": self.stored_ratio.get(tube)}
        self.runs.append(run)
        return run

    def settled_weight(self, reads: int = 6, gap_s: float = 0.1) -> float:
        """The mean of a few readings a tenth of a second apart: finer than one reading's digit, and no tap."""
        values = []
        for _ in range(reads):
            values.append(self.weight())
            time.sleep(gap_s)
        return statistics.fmean(values)

    def flick(self, variant: str, moves: list, pause_s: float, still_s: float) -> dict:
        """
        One trial of the flick test: the fine tube through a list of speeds one after another, then still.

        Args:
            variant: the trial's kind, kept with it.
            moves: speeds in rev/s, each held for the pause; negative is backwards.
            pause_s: how long each speed is held, before the command's own time.
            still_s: how long to stay still before the settled weight is read.

        Returns:
            The trial's facts: the weight gained and the steps made (forward and back both count).
        """
        before_weight = self.settled_weight()
        before = self.motor_stats()
        for speed in moves:
            self.session.send(f"motor fine {speed:g}")
            time.sleep(pause_s)
        self.session.send("motor fine 0")
        after = self.motor_stats()
        time.sleep(still_s)
        gained = self.settled_weight() - before_weight
        trial = {"variant": variant, "gained_gr": gained, "steps": after["fine"] - before["fine"],
                 "moves_ms": after["t_ms"] - before["t_ms"], "note": "flick"}
        self.runs.append(trial)
        return trial

    def creep_stop(self, speed_rps: float, run_s: float, still_s: float, held: bool) -> dict:
        """
        One trial of the creep-stop test: the fine tube at a steady slow speed, stopped, then still.

        It measures what a finish by one stop has to allow for: how much the reading still rises after the stop,
        and for how long. Weights are read by `state` as fast as the console answers, on the PC's clock.

        Args:
            speed_rps: the creep's speed.
            run_s: how long to turn before the stop.
            still_s: how long to watch after it.
            held: stop with the driver left on (as in a charge), not switched off.

        Returns:
            The trial's facts: the readings during the creep and after the stop, each with its time from the stop.
        """
        before = self.motor_stats()
        self.session.send(f"motor fine {speed_rps:g}")
        started = time.monotonic()
        during = []
        while time.monotonic() - started < run_s:
            during.append((time.monotonic(), self.weight()))
        self.session.send(f"motor fine {HOLD_RPS:g}" if held else "motor fine 0")
        stopped = time.monotonic()
        made = self.motor_stats()
        after = []
        while time.monotonic() - stopped < still_s:
            after.append((time.monotonic(), self.weight()))
        if held:
            self.session.send("motor fine 0")
        trial = {"note": "creepstop", "speed_asked_rps": speed_rps, "held": held, "run_s": stopped - started,
                 "turns": (made["fine"] - before["fine"]) / MICROSTEPS_PER_MOTOR_TURN * TUBE_PER_MOTOR["fine"],
                 "during": [(round(t - stopped, 3), w) for t, w in during],
                 "after": [(round(t - stopped, 3), w) for t, w in after]}
        self.runs.append(trial)
        return trial

    def state(self) -> dict:
        """The console's state now: the charge's state, the weight and both speeds, with the PC's clock."""
        reply = self.session.send("state")
        fields = dict(re.findall(r"(\w+)=(-?[\d.]+|nan|inf)", reply))
        return {"t": time.monotonic(), "charge": int(fields["charge"]), "weight": float(fields["weight"]),
                "coarse": float(fields["coarse"]), "fine": float(fields["fine"])}

    def end_charge(self) -> None:
        """Stop the charge and wait until the menu is back at its forms."""
        self.session.send("charge stop")
        self.charging = False
        while self.state()["charge"] != CHARGE_EXIT:
            time.sleep(0.1)
        time.sleep(1.0)

    def zero_scale(self, unit) -> None:
        """Ask the scale to zero over the API (taken with no job) until it reads zero and stays there."""
        for _ in range(3):
            unit.act("scale/zero")
            asked = time.monotonic()
            still = 0
            while time.monotonic() - asked < 6.0:
                still = still + 1 if abs(self.weight()) < DIGIT_GR / 2.0 else 0
                if still >= 20:
                    return
        raise SystemExit("the scale did not come to zero")

    def start_charge(self, unit, target: float, profile: int) -> None:
        """A start over the API, asked again while the menu is still leaving the last charge."""
        import unit_api
        for attempt in range(10):
            try:
                unit.act("charge/start", target=target, powder_profile=profile)
                self.charging = True
                return
            except unit_api.ApiError:
                if attempt == 9:
                    raise
                time.sleep(0.5)

    def throws(self, host: str, target: float, profile: int, count: int, watch_s: float, rezero: bool) -> None:
        """
        Real throws of the firmware's own loop, recorded: the charge started over the API with a powder profile,
        the console's state read as fast as it answers.

        Each throw is kept from its zeroing to the end of its watch: the time the tubes ran, the reading when
        they stopped, a second later (the unit's verdict) and at rest. Between throws either a person changes the
        cup (it must stay on for the watch after the tubes stop), or, with rezero, the charge is stopped, the
        scale zeroed with the powder left in the cup and a new charge started: no hand at the cup, and no more
        throws than the cup holds.

        Args:
            host: the unit's address, for the start and the zero.
            target: the charge's target.
            profile: the powder profile's id.
            count: throws to record.
            watch_s: how long after the stop the weight at rest is read.
            rezero: zero the scale between throws in place of a changed cup.
        """
        import unit_api   # beside rc_run.py, of the firmware's time
        unit = unit_api.Unit(host)
        if rezero:
            if abs(self.weight()) > 2.0:
                raise SystemExit("the scale is far from zero: is the empty cup on it?")
            self.zero_scale(unit)
        self.start_charge(unit, target, profile)
        samples, done, poured, last_state, pending, stopped_at = [], 0, 0.0, None, False, None
        limit = time.monotonic() + count * 180.0
        while done < count and time.monotonic() < limit:
            sample = self.state()
            if sample["charge"] == CHARGE_EXIT and last_state not in (None, CHARGE_EXIT):
                print("the charge ended at the unit")
                self.charging = False
                break
            if sample["charge"] == CHARGE_WAIT_FOR_COMPLETE and last_state != CHARGE_WAIT_FOR_COMPLETE:
                samples, pending, stopped_at = [], True, None
            samples.append(sample)
            if sample["charge"] == CHARGE_WAIT_FOR_CUP_REMOVAL and last_state == CHARGE_WAIT_FOR_COMPLETE:
                stopped_at = sample["t"]
            # A throw is whole once it has been watched after its stop, or the cup has gone before that
            watched = stopped_at is not None and sample["t"] - stopped_at >= watch_s + 0.1
            last_state = sample["charge"]
            if not (pending and stopped_at is not None and (watched or sample["charge"] != CHARGE_WAIT_FOR_CUP_REMOVAL)):
                continue
            done += 1
            pending = False
            self.runs.append(throw_facts(samples, target, watch_s))
            poured += self.runs[-1]["at_verdict"]
            print(throw_line(done, self.runs[-1]) + f"; {poured:.1f} gr in the cup", flush=True)
            if rezero and done < count:
                if poured + target > CUP_LIMIT_GR:
                    print("stopping: the cup would pass its limit")
                    break
                self.end_charge()
                self.zero_scale(unit)
                self.start_charge(unit, target, profile)
                last_state = None
        if self.charging:
            self.end_charge()

    def precondition(self, tube: str, seconds: float) -> None:
        """The same state before every run: a time at 1 rev/s, then 5 s still. 0 for none."""
        if seconds > 0.0:
            self.turn(tube, PRECONDITION_RPS, seconds, 5.0, note="precondition")

    def close(self) -> int:
        """Stop a charge left running and both motors, save the frames and the runs, end with the verdict."""
        if self.charging:
            self.end_charge()
        for tube in ("coarse", "fine"):
            self.session.send(f"motor {tube} 0")
        self.session.send("scale tap off")
        time.sleep(0.3)
        with open(self.folder / "frames.csv", "w", encoding="utf-8") as out:
            out.write("t_ms,weight,header\n")
            for _, line in self.session.console.tap_lines:
                parts = line.split()
                header = bytes.fromhex(parts[3][:4]).decode(errors="replace") if len(parts) > 3 else ""
                out.write(f"{parts[1]},{parts[2]},{header}\n")
        (self.folder / "runs.json").write_text(json.dumps(self.runs, indent=1), encoding="utf-8")
        print(f"saved {len(self.session.console.tap_lines)} frames and {len(self.runs)} runs to {self.folder}")
        return self.session.finish()


def frames_between(folder: Path, start_ms: float, end_ms: float) -> list:
    """Weights of the saved frames in a span of the unit's clock."""
    out = []
    with open(folder / "frames.csv", encoding="utf-8") as lines:
        next(lines)
        for line in lines:
            t_ms, weight, _ = line.rstrip("\n").split(",")
            if start_ms <= float(t_ms) <= end_ms and weight not in ("nan", "inf"):
                out.append(float(weight))
    return out


def summarise(folder: Path) -> None:
    """Per run that is not a precondition: turns, the weight gained by the settled end, what showed after the stop."""
    runs = json.loads((folder / "runs.json").read_text(encoding="utf-8"))
    print("tube speed_asked turns run_s gained_gr gr_per_turn gr_per_s late_gr note")
    for run in runs:
        if run["note"] == "precondition":
            continue
        before = frames_between(folder, run["t_before_ms"] - 600.0, run["t_before_ms"])
        at_stop = frames_between(folder, run["t_stop_asked_ms"] - 120.0, run["t_stop_asked_ms"])
        after = frames_between(folder, run["t_after_ms"] - 700.0, run["t_after_ms"])
        if not before or not after or not at_stop:
            print(run["tube"], run["speed_asked_rps"], "no frames")
            continue
        gained = statistics.fmean(after) - statistics.fmean(before)
        run_s = (run["t_stop_asked_ms"] - run["t_before_ms"]) / 1000.0
        late = statistics.fmean(after) - at_stop[-1]
        per_turn = gained / run["turns"] if run["turns"] else float("nan")
        print(f"{run['tube']} {run['speed_asked_rps']:g} {run['turns']:.4f} {run_s:.2f} {gained:+.3f} {per_turn:.3f} "
              f"{gained / run_s:.3f} {late:+.3f} {run['note']}")


def throw_facts(samples: list, target: float, watch_s: float) -> dict:
    """One throw from its state samples (zeroing to the cup's removal), or None when it never charged."""
    charging = [s for s in samples if s["charge"] == CHARGE_WAIT_FOR_COMPLETE]
    after = [s for s in samples if s["charge"] == CHARGE_WAIT_FOR_CUP_REMOVAL]
    if not charging or not after:
        return None
    stop = after[0]["t"]
    coarse_on = [s["t"] for s in charging if s["coarse"] > 0.0]
    # At rest: the readings late in the watch, while the cup is still on (a lifted cup reads far under the target)
    rest = [s["weight"] for s in after if watch_s - 1.0 <= s["t"] - stop <= watch_s and s["weight"] > target - 1.0]
    verdict = [s["weight"] for s in after if s["t"] - stop <= 1.0]
    return {"note": "throw", "target": target, "time_s": stop - charging[0]["t"],
            "coarse_s": (coarse_on[-1] - charging[0]["t"]) if coarse_on else 0.0,
            "at_stop": charging[-1]["weight"], "at_verdict": verdict[-1],
            "at_rest": statistics.median(rest) if rest else None,
            "samples": [(round(s["t"] - stop, 3), s["charge"], s["weight"], s["coarse"], s["fine"])
                        for s in samples if s["charge"] != CHARGE_WAIT_FOR_ZERO]}


def throw_result(error: float) -> str:
    """Over, under or good, by the unit's own rule at its default stop threshold."""
    return "OVER" if error >= FINE_STOP_THRESHOLD_GR - 1e-6 else "under" if error <= -FINE_STOP_THRESHOLD_GR + 1e-6 \
        else "good"


def throw_line(index: int, throw: dict) -> str:
    """A throw as one line."""
    target = throw["target"]
    rest = throw["at_rest"]
    end = f"at rest {rest - target:+.3f} {throw_result(rest - target)}" if rest is not None else "cup lifted early"
    return (f"throw {index}: {throw['time_s']:.1f} s (coarse {throw['coarse_s']:.1f} s), at the stop "
            f"{throw['at_stop'] - target:+.3f}, a second later {throw['at_verdict'] - target:+.3f} "
            f"{throw_result(throw['at_verdict'] - target)}, {end}")


def summarise_throws(folder: Path) -> None:
    """Every throw's line, then the counts by the weight at rest."""
    throws = [run for run in json.loads((folder / "runs.json").read_text(encoding="utf-8")) if run["note"] == "throw"]
    for index, throw in enumerate(throws):
        print(throw_line(index + 1, throw))
    rested = [throw for throw in throws if throw["at_rest"] is not None]
    results = [throw_result(throw["at_rest"] - throw["target"]) for throw in rested]
    shown = [throw_result(throw["at_verdict"] - throw["target"]) for throw in rested]
    if rested:
        print(f"{len(rested)} throws at rest: {results.count('OVER')} over, {results.count('under')} under, "
              f"{results.count('good')} good (the unit's verdict a second after the stop: {shown.count('OVER')} over, "
              f"{shown.count('under')} under); time median {statistics.median(t['time_s'] for t in rested):.1f} s, "
              f"longest {max(t['time_s'] for t in rested):.1f} s; rise after the stop mean "
              f"{statistics.fmean((t['at_rest'] - t['at_stop']) / DIGIT_GR for t in rested):+.1f} digits")


def creep_gain(trial: dict) -> tuple:
    """A creep-stop trial's rise after the stop in digits, and the time its reading last differed from its end."""
    at_stop = trial["during"][-1][1]
    end = statistics.median(w for t, w in trial["after"] if t >= trial["after"][-1][0] - 0.6)
    moving = [t for t, w in trial["after"] if abs(w - end) > DIGIT_GR / 2.0]
    return (end - at_stop) / DIGIT_GR, (moving[-1] if moving else 0.0)


def summarise_creep(folder: Path) -> None:
    """Per speed and kind of stop: the rise after the stop (mean, spread, counts by digit) and the widest share
    of stops a band of three digits holds, which is the most a finish by one stop from that speed can keep."""
    trials = [run for run in json.loads((folder / "runs.json").read_text(encoding="utf-8"))
              if run["note"] == "creepstop"]
    kinds = sorted({(trial["speed_asked_rps"], trial["held"]) for trial in trials})
    for speed, held in kinds:
        group = [trial for trial in trials if (trial["speed_asked_rps"], trial["held"]) == (speed, held)]
        gains, settles, rates = [], [], []
        for trial in group:
            gain, settle = creep_gain(trial)
            gains.append(round(gain))
            settles.append(settle)
            late = [(t, w) for t, w in trial["during"] if t >= -trial["run_s"] + 1.5]
            if len(late) > 1:
                rates.append((late[-1][1] - late[0][1]) / (late[-1][0] - late[0][0]))
        counts = {digit: gains.count(digit) for digit in sorted(set(gains))}
        best = max(sum(1 for gain in gains if low <= gain <= low + 2) for low in range(min(gains), max(gains) + 1))
        spread = statistics.pstdev(gains) if len(gains) > 1 else 0.0
        print(f"fine {speed:g} rev/s, {'driver on' if held else 'driver off'}: {len(group)} stops, "
              f"{statistics.fmean(rates) / DIGIT_GR:.2f} digits/s; after the stop +{statistics.fmean(gains):.2f} digits "
              f"(spread {spread:.2f}), by digit {counts}; three digits hold {best} of {len(gains)}; "
              f"still by median {statistics.median(settles):.2f} s, longest {max(settles):.2f} s")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", required=True, type=Path, help="the checkout whose rc_run.py and build match the unit")
    parser.add_argument("--board", default="pico2_w")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("chain", help="five short runs of the fine tube: the recording chain's proof")
    nudges = sub.add_parser("nudges", help="small turns of the fine tube, each followed by a pause")
    nudges.add_argument("--count", type=int, default=100)
    nudges.add_argument("--pause-ms", type=float, default=40.0)
    nudges.add_argument("--speed", type=float, default=0.5)
    nudges.add_argument("--still-s", type=float, default=1.5)
    runs = sub.add_parser("runs", help="a tube at each of several speeds, with a stop and a settle after each")
    runs.add_argument("--tube", required=True, choices=("coarse", "fine"))
    runs.add_argument("--speeds", required=True, help="comma-separated rev/s, as asked of the unit")
    runs.add_argument("--repeats", type=int, default=3)
    runs.add_argument("--limit-gr", type=float, default=5.0)
    runs.add_argument("--limit-s", type=float, default=60.0)
    runs.add_argument("--still-s", type=float, default=5.0)
    runs.add_argument("--precondition-s", type=float, default=5.0)
    steps = sub.add_parser("steps", help="speeds one after another with no stop between: speed:seconds,...")
    steps.add_argument("--tube", required=True, choices=("coarse", "fine"))
    steps.add_argument("--plan", required=True)
    flick = sub.add_parser("flick", help="plain nudges against shaken ones, turn about, weights by `state` (no tap)")
    flick.add_argument("--count", type=int, default=40, help="trials of each kind")
    flick.add_argument("--still-s", type=float, default=2.6)
    flick.add_argument("--pause-ms", type=float, default=10.0)
    flick.add_argument("--variants", default="nudge,shake,shake_wide")
    creep = sub.add_parser("creepstop", help="the fine tube at a slow steady speed, stopped, watched: what a finish "
                                             "by one stop must allow for (weights by `state`, no tap)")
    creep.add_argument("--speeds", default="0.05,0.1", help="comma-separated rev/s")
    creep.add_argument("--count", type=int, default=30, help="stops of each speed and each kind of stop")
    creep.add_argument("--run-s", default="3:6", help="least:most seconds of turning before a stop, drawn at random")
    creep.add_argument("--still-s", type=float, default=3.0)
    creep.add_argument("--stops", default="held,released", help="held (driver on, as in a charge), released, or both")
    throws = sub.add_parser("throws", help="real throws of the firmware's own loop, recorded by `state`; a person "
                                           "changes the cup after each")
    throws.add_argument("--host", required=True, help="the unit's address, for the start over the API")
    throws.add_argument("--target", type=float, required=True)
    throws.add_argument("--profile", type=int, required=True, help="the powder profile's id")
    throws.add_argument("--count", type=int, default=10)
    throws.add_argument("--watch-s", type=float, default=3.5, help="the cup stays on this long after the tubes stop")
    throws.add_argument("--rezero", action="store_true",
                        help="zero the scale between throws, the powder left in the cup, in place of a changed cup")
    show = sub.add_parser("show", help="sum up a saved folder")
    show.add_argument("folder", type=Path)
    args = parser.parse_args()

    if args.command == "show":
        runs = json.loads((args.folder / "runs.json").read_text(encoding="utf-8"))
        if any(run["note"] == "creepstop" for run in runs):
            summarise_creep(args.folder)
        elif any(run["note"] == "throw" for run in runs):
            summarise_throws(args.folder)
        else:
            summarise(args.folder)
        return

    bench = Bench(args.repo, args.command, args.board, tap=args.command not in ("flick", "creepstop", "throws"))
    try:
        start = bench.weight()
        print(f"weight at the start {start:.2f}")
        if start > CUP_LIMIT_GR and args.command != "throws":
            raise SystemExit("the cup is near full: ask the owner to empty it")
        if args.command == "chain":
            for _ in range(5):
                bench.turn("fine", 0.5, 2.0, 3.0, note="chain")
        elif args.command == "nudges":
            for index in range(args.count):
                bench.turn("fine", args.speed, args.pause_ms / 1000.0, args.still_s, note=f"nudge {index}")
        elif args.command == "runs":
            plan = [float(text) for text in args.speeds.split(",")] * args.repeats
            random.Random(len(plan)).shuffle(plan)
            for speed in plan:
                if bench.weight() > CUP_LIMIT_GR:
                    print("stopping: the cup is near full")
                    break
                bench.precondition(args.tube, args.precondition_s)
                bench.turn(args.tube, speed, args.limit_s, args.still_s, note="map", limit_gr=args.limit_gr)
        elif args.command == "flick":
            names = args.variants.split(",")
            for index in range(args.count * len(names)):
                name = names[index % len(names)]
                trial = bench.flick(name, FLICKS[name], args.pause_ms / 1000.0, args.still_s)
                print(f"{name} {trial['gained_gr']:+.3f} {trial['steps']} {trial['moves_ms']:.0f}", flush=True)
        elif args.command == "throws":
            bench.throws(args.host, args.target, args.profile, args.count, args.watch_s, args.rezero)
        elif args.command == "creepstop":
            least, most = (float(text) for text in args.run_s.split(":"))
            plan = [(float(speed), stop == "held") for speed in args.speeds.split(",")
                    for stop in args.stops.split(",")] * args.count
            draw = random.Random(len(plan))
            draw.shuffle(plan)
            bench.turn("fine", PRECONDITION_RPS, 3.0, 5.0, note="precondition")
            for index, (speed, held) in enumerate(plan):
                if bench.weight() > CUP_LIMIT_GR:
                    print("stopping: the cup is near full")
                    break
                trial = bench.creep_stop(speed, draw.uniform(least, most), args.still_s, held)
                gain, settle = creep_gain(trial)
                print(f"{index + 1}/{len(plan)} {speed:g} {'held' if held else 'released'} "
                      f"{trial['run_s']:.1f} s {gain:+.1f} digits, still by {settle:.2f} s", flush=True)
        elif args.command == "steps":
            bench.precondition(args.tube, 5.0)
            first = bench.motor_stats()
            for part in args.plan.split(","):
                speed, seconds = (float(text) for text in part.split(":"))
                bench.session.send(f"motor {args.tube} {speed:g}")
                mark = bench.motor_stats()
                bench.runs.append({"tube": args.tube, "speed_asked_rps": speed, "note": "step", "t_ms": mark["t_ms"],
                                   "steps_so_far": mark[args.tube] - first[args.tube]})
                time.sleep(seconds)
            bench.session.send(f"motor {args.tube} 0")
            time.sleep(5.0)
            last = bench.motor_stats()
            bench.runs.append({"tube": args.tube, "speed_asked_rps": 0.0, "note": "step end", "t_ms": last["t_ms"],
                               "steps_so_far": last[args.tube] - first[args.tube]})
        print(f"weight at the end {bench.weight():.2f}")
    finally:
        code = bench.close()
    if args.command in ("chain", "nudges", "runs"):
        summarise(bench.folder)
    if args.command == "creepstop":
        summarise_creep(bench.folder)
    if args.command == "throws":
        summarise_throws(bench.folder)
    sys.exit(code)


if __name__ == "__main__":
    main()
