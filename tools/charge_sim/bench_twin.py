"""The bench's tests run against the simulation: each makes trials of the shape `bench/record.py` saves.

A test here turns the simulated tubes as the real test turned the real ones (the same speeds, times and stops) and
reads the simulated scale as the console's `state` was read, so `bench/stats.py` sums both up alike. What the PC
adds on the bench (a command's 20 ms) is left out.
"""

import random
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / "bench"))

import stats                                      # noqa: E402  (bench/stats.py)
from legacy_pid import LegacyPid, Profile         # noqa: E402
from plant import Cup, TubeState                  # noqa: E402
from scale import ScaleState                      # noqa: E402
from throw import DT_S, Rig, Session              # noqa: E402

STATE_READS_PER_S = 47.0        # how often the bench read `state`
HOLD_STILL_S = 5.0              # between trials, as the bench's own pauses and the next trial's commands
REST_WATCH_S = 3.5              # a throw's weight at rest is read this long after its stop, as record.py's watch


class TwinBench:
    """A rig standing on the bench: both tubes, a cup never emptied, the scale, and a clock."""

    def __init__(self, rig: Rig, seed: int):
        self.rig = rig
        self.rng = random.Random(seed)
        self.tubes = {"coarse": TubeState(rig.coarse, rig.powder, self.rng, rig.acceleration_rps2, rig.max_speed_rps),
                      "fine": TubeState(rig.fine, rig.powder, self.rng, rig.acceleration_rps2, rig.max_speed_rps)}
        self.cup = Cup()
        self.scale = ScaleState(rig.scale, self.rng, DT_S)
        self.now = 0.0
        self.reading = 0.0
        self.next_read = 0.0

    def advance(self, seconds: float) -> list:
        """Let time pass; returns the readings `state` would have given, as (time, reading)."""
        samples = []
        end = self.now + seconds
        while self.now < end:
            self.now += DT_S
            for tube in self.tubes.values():
                tube.step(self.now, DT_S, self.cup.landings)
            reading = self.scale.step(self.now, self.cup.step(self.now, DT_S))
            if reading is not None:
                self.reading = reading
            if self.now >= self.next_read:
                self.next_read = self.now + 1.0 / STATE_READS_PER_S
                samples.append((self.now, self.reading))
        return samples

    def creep_stop(self, tube: str, speed_rps: float, run_s: float, still_s: float, stop: str) -> dict:
        """One creep-stop trial, as record.py's: a steady turn, a stop (or the charge's reverse turn first), a watch."""
        state = self.tubes[tube]
        state.set_speed(speed_rps)
        during = self.advance(run_s)
        stopped = self.now
        after = []
        if stop == "backoff":
            state.set_speed(-1.0)
            after = self.advance(0.25)
        state.set_speed(0.0)
        after += self.advance(still_s - (self.now - stopped))
        return {"note": "creepstop", "tube": tube, "speed_asked_rps": speed_rps, "stop": stop,
                "held": stop != "released", "run_s": run_s,
                "during": [(t - stopped, w) for t, w in during], "after": [(t - stopped, w) for t, w in after]}

    def nudge(self, speed_rps: float, turn: float, still_s: float) -> tuple:
        """One nudge of the fine tube: (its gain in kernels, seconds until the reading first moved or None)."""
        before = statistics.fmean(w for _, w in self.advance(0.6))
        start = self.now
        self.tubes["fine"].set_speed(speed_rps)
        samples = self.advance(turn / speed_rps)
        self.tubes["fine"].set_speed(0.0)
        samples += self.advance(still_s)
        gain = (statistics.fmean(w for t, w in samples if t >= self.now - 0.7) - before) / self.rig.powder.kernel_gr
        moved = [t for t, w in samples if w >= before + stats.DIGIT_GR / 2.0]
        return gain, (moved[0] - start if moved else None)


    def ramp(self, tube: str, plan: list, still_s: float) -> dict:
        """One trial of a speed plan with no controller, as record.py's: the speeds in turn, a stop, a watch."""
        state = self.tubes[tube]
        during = []
        for speed_rps, seconds in plan:
            state.set_speed(speed_rps)
            during += [(t, w, speed_rps) for t, w in self.advance(seconds)]
        stopped = self.now
        state.set_speed(0.0)
        after = self.advance(still_s)
        return {"note": "ramp", "tube": tube, "plan": plan, "stop": "held",
                "during": [(t - stopped, w, speed) for t, w, speed in during],
                "after": [(t - stopped, w) for t, w in after]}


def primed(rig: Rig, tube: str, seed: int) -> TwinBench:
    """A bench after the recorder's own priming turn of a tube."""
    bench = TwinBench(rig, seed)
    bench.tubes[tube].set_speed(1.0)
    bench.advance(3.0)
    bench.tubes[tube].set_speed(0.0)
    bench.advance(HOLD_STILL_S)
    return bench


def ramp_trials(rig: Rig, tube: str, plan: list, still_s: float, count: int, seed: int) -> list:
    """A series of trials of one speed plan."""
    bench = primed(rig, tube, seed)
    trials = []
    for _ in range(count):
        trials.append(bench.ramp(tube, plan, still_s))
        bench.advance(0.5)
    return trials


def creep_trials(rig: Rig, tube: str, speed_rps: float, stop: str, run_s: tuple, still_s: float, count: int,
                 seed: int) -> list:
    """A series of creep-stop trials of one kind, after the bench's own priming turn."""
    bench = primed(rig, tube, seed)
    trials = []
    for _ in range(count):
        trials.append(bench.creep_stop(tube, speed_rps, bench.rng.uniform(*run_s), still_s, stop))
        bench.advance(0.5)
    return trials


def nudge_series(rig: Rig, speed_rps: float, turn: float, still_s: float, count: int, seed: int) -> dict:
    """A series of watched nudges, summed up."""
    bench = primed(rig, "fine", seed)
    gains, moves = [], []
    for _ in range(count):
        gain, moved = bench.nudge(speed_rps, turn, still_s)
        gains.append(gain)
        if gain >= 0.7 and moved is not None:
            moves.append((gain, moved))
    return stats.nudge_stats(gains, moves)


def throws(rig: Rig, profile: Profile, target_gr: float, count: int, seed: int) -> list:
    """Throws of today's loop with a profile, each as record.py keeps a real one."""
    session = Session(rig, seed)
    out = []
    for _ in range(count):
        result = session.throw(LegacyPid(profile), target_gr, profile.fine_stop_threshold, record=True,
                               settle_s=REST_WATCH_S)
        readings = [(row[0], row[1]) for row in result.rows if row[1] is not None]
        coarse_on = [row[0] for row in result.rows if row[3] > 0.0]
        out.append({"note": "throw", "target": target_gr, "time_s": result.elapsed_s,
                    "coarse_s": coarse_on[-1] if coarse_on else 0.0,
                    "at_stop": stats.reading_at(readings, result.elapsed_s), "at_rest": readings[-1][1]})
    return out
