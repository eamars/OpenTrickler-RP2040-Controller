"""One throw, and a session of throws on the same rig: a controller against the plant and a scale, a millisecond a step."""

import random
from dataclasses import dataclass, field

from plant import Cup, Powder, Tube, TubeState
from scale import ScaleModel, ScaleState

DT_S = 0.001
VERDICT_AFTER_S = 1.0       # the firmware reads the weight once, a second after the stop
SETTLE_AFTER_S = 3.0        # by then everything has landed: the true weight
TIMEOUT_S = 90.0


@dataclass(frozen=True)
class Rig:
    """A unit as built: its two tubes, the powder in it, its scale and its motors' limits."""

    powder: Powder
    coarse: Tube
    fine: Tube
    scale: ScaleModel
    acceleration_rps2: float = 50.0
    max_speed_rps: float = 5.0


@dataclass
class Result:
    """How one throw went. Weights in grains; error is weight less target, so positive is over."""

    target: float
    elapsed_s: float            # start to the controller's stop, as the unit's screen shows it
    reading: float              # the weight the verdict is judged on
    true_gr: float              # what is really in the cup once everything has landed
    verdict: str                # good, over, under (the firmware's rule on the reading) or timeout
    coarse_gr: float            # what each tube delivered
    fine_gr: float
    rows: list = field(default_factory=list)    # (t_s, reading or None, true landed, coarse rps, fine rps)

    @property
    def error(self) -> float:
        return self.reading - self.target

    @property
    def true_error(self) -> float:
        return self.true_gr - self.target


class Session:
    """Throws one after another on one rig: the tubes keep their state between throws, the cup is emptied."""

    def __init__(self, rig: Rig, seed: int):
        self.rig = rig
        self.rng = random.Random(seed)
        self.coarse = TubeState(rig.coarse, rig.powder, self.rng, rig.acceleration_rps2, rig.max_speed_rps)
        self.fine = TubeState(rig.fine, rig.powder, self.rng, rig.acceleration_rps2, rig.max_speed_rps)

    def throw(self, controller, target_gr: float, tolerance_gr: float, record: bool = False,
              timeout_s: float = TIMEOUT_S) -> Result:
        """Run one throw to its verdict.

        controller: has start(target, now_ms), on_tick(now_ms), on_reading(now_ms, weight), and the attributes
            coarse_rps, fine_rps and done.
        tolerance_gr: the verdict's band, the firmware's fine stop threshold.
        record: keep a row a millisecond for plotting.
        timeout_s: a controller not done by then is stopped, and the throw counts as a timeout.
        """
        coarse, fine = self.coarse, self.fine
        cup = Cup()
        scale = ScaleState(self.rig.scale, self.rng, DT_S)
        coarse_before, fine_before = coarse.released_gr, fine.released_gr
        rows = []
        latest = 0.0
        stopped_at = None
        verdict_reading = None
        now = 0.0
        controller.start(target_gr, 0.0)

        while True:
            now += DT_S
            now_ms = now * 1000.0
            if stopped_at is None:
                controller.on_tick(now_ms)
                coarse.set_speed(controller.coarse_rps)
                fine.set_speed(controller.fine_rps)
            coarse.step(now, DT_S, cup.landings)
            fine.step(now, DT_S, cup.landings)
            reading = scale.step(now, cup.step(now, DT_S))
            if reading is not None:
                latest = reading
                if stopped_at is None:
                    controller.on_reading(now_ms, reading)
                    coarse.set_speed(controller.coarse_rps)
                    fine.set_speed(controller.fine_rps)
                    if controller.done:
                        stopped_at = now
            if record:
                rows.append((now, reading, cup.landed_gr, coarse.speed, fine.speed))

            if stopped_at is None:
                if now >= timeout_s:
                    coarse.set_speed(0.0)
                    fine.set_speed(0.0)
                    stopped_at = now
                    verdict_reading = float("nan")
                continue
            if verdict_reading is None and now >= stopped_at + VERDICT_AFTER_S:
                verdict_reading = latest
            if now >= stopped_at + SETTLE_AFTER_S:
                break

        if verdict_reading != verdict_reading:
            verdict = "timeout"
        elif verdict_reading - target_gr >= tolerance_gr:
            verdict = "over"
        elif target_gr - verdict_reading >= tolerance_gr:
            verdict = "under"
        else:
            verdict = "good"
        return Result(target=target_gr, elapsed_s=stopped_at, reading=verdict_reading, true_gr=cup.landed_gr,
                      verdict=verdict, coarse_gr=coarse.released_gr - coarse_before,
                      fine_gr=fine.released_gr - fine_before, rows=rows)
