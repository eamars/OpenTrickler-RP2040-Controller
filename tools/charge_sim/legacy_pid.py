"""Today's charge loop, ported line for line from charge_mode.cpp's charge_mode_wait_for_complete().

Two PID loops on the same weight, run once per scale reading. The coarse one aims short of the target by the coarse
stop threshold and is switched off for good on reaching it, then backs off; the fine one aims at the target from
the start. Everything stops when the reading is within the fine stop threshold of the target. The port keeps the
loop's known faults (ADR-009): the I term is not scaled by time, and the back-off ends only when a reading comes.
"""

import dataclasses
from dataclasses import dataclass


@dataclass(frozen=True)
class Profile:
    """A powder profile and the charge settings a throw uses, as the unit stores them."""

    coarse_kp: float
    coarse_ki: float
    coarse_kd: float
    coarse_min_rps: float
    coarse_max_rps: float
    fine_kp: float
    fine_ki: float
    fine_kd: float
    fine_min_rps: float
    fine_max_rps: float
    coarse_stop_threshold: float = 5.0
    fine_stop_threshold: float = 0.03
    backoff_enable: bool = True
    backoff_turns: float = 0.25
    backoff_speed_rps: float = 1.0


# The Pico 2 W's stored AR2209 profile and motor limits (logs/adr004/pico2w/before_review/settings.json): the
# profile's minimum is raised to the motor's own, 0.1 rev/s, as charge_mode.cpp does.
AR2209_PICO2W = Profile(coarse_kp=0.02, coarse_ki=0.0, coarse_kd=0.3, coarse_min_rps=0.1, coarse_max_rps=5.0,
                        fine_kp=2.0, fine_ki=0.0, fine_kd=10.0, fine_min_rps=0.1, fine_max_rps=5.0)
# The two other settings recorded as real throws on 2026-11-20: the fine gain the measurements give, and that
# gain with a slow finish (the fine tube held to 1.5 rev/s, its least speed 0.03).
PICO2W_KP08 = dataclasses.replace(AR2209_PICO2W, fine_kp=0.8)
# The owner's own trickler, the Pico W at 192.168.2.168, its AR2209 profile as he tuned it by hand (read over its web
# API on 2026-11-22, logs/adr009/picow_168_20261122; the same as on 11-19): the coarse tube's top of 8 rev/s held to
# its motor's 5. Its charge settings are the defaults here (stops at 5 and 0.03 gr, the back-off on). Its own tubes
# and gears are not the bench's: on the bench's rig these are its gains, not its rig
AR2209_PICOW_168 = Profile(coarse_kp=0.05, coarse_ki=0.0, coarse_kd=0.3, coarse_min_rps=0.1, coarse_max_rps=5.0,
                           fine_kp=0.8, fine_ki=0.0, fine_kd=15.0, fine_min_rps=0.08, fine_max_rps=2.0)
PICO2W_SLOW_FINISH = dataclasses.replace(AR2209_PICO2W, fine_kp=0.8, fine_max_rps=1.5, fine_min_rps=0.03)


class LegacyPid:
    """The firmware's loop. The harness reads coarse_rps, fine_rps and done after each call."""

    name = "legacy_pid"

    def __init__(self, profile: Profile):
        self.profile = profile

    def start(self, target_gr: float, now_ms: float) -> None:
        """Begin a throw at the moment the firmware enters the loop."""
        self.target = target_gr
        self.coarse_target = max(0.0, target_gr - self.profile.coarse_stop_threshold)
        self.coarse_rps = 0.0
        self.fine_rps = 0.0
        self.done = False
        self.coarse_integral = 0.0
        self.fine_integral = 0.0
        self.coarse_last_error = 0.0
        self.fine_last_error = 0.0
        self.last_sample_ms = now_ms
        self.first_sample = True
        self.coarse_moves = True
        self.backoff_until_ms = None

    def on_tick(self, now_ms: float) -> None:
        """Nothing happens between readings in today's loop."""

    def on_reading(self, now_ms: float, weight: float) -> None:
        """One pass of the loop, on a reading's arrival."""
        profile = self.profile
        if self.backoff_until_ms is not None and now_ms >= self.backoff_until_ms:
            self.coarse_rps = 0.0
            self.backoff_until_ms = None

        coarse_error = self.coarse_target - weight
        fine_error = self.target - weight

        if fine_error < profile.fine_stop_threshold:
            self.fine_rps = 0.0
            self.coarse_rps = 0.0
            self.done = True
            return
        if coarse_error <= 0.0 and self.coarse_moves:
            self.coarse_moves = False
            self.coarse_rps = 0.0
            self._backoff(now_ms)

        elapsed_ms = now_ms - self.last_sample_ms
        self.fine_integral += fine_error
        derivative = self._derivative(fine_error, self.fine_last_error, elapsed_ms)
        speed = profile.fine_kp * fine_error + profile.fine_ki * self.fine_integral + profile.fine_kd * derivative
        self.fine_rps = max(profile.fine_min_rps, min(speed, profile.fine_max_rps))

        if self.coarse_moves and self.backoff_until_ms is None:
            self.coarse_integral += coarse_error
            derivative = self._derivative(coarse_error, self.coarse_last_error, elapsed_ms)
            speed = (profile.coarse_kp * coarse_error + profile.coarse_ki * self.coarse_integral
                     + profile.coarse_kd * derivative)
            self.coarse_rps = max(profile.coarse_min_rps, min(speed, profile.coarse_max_rps))

        self.last_sample_ms = now_ms
        self.fine_last_error = fine_error
        self.coarse_last_error = coarse_error
        self.first_sample = False

    def _derivative(self, error: float, last_error: float, elapsed_ms: float) -> float:
        """Change of the error per millisecond; 0 on the first reading and when no time has passed."""
        if self.first_sample or elapsed_ms <= 0.0:
            return 0.0
        return (error - last_error) / elapsed_ms

    def _backoff(self, now_ms: float) -> None:
        """Run the coarse trickler backwards for the stored turns, as coarse_trickler_backoff() does."""
        profile = self.profile
        if not profile.backoff_enable or profile.backoff_turns <= 0.0:
            return
        speed = profile.backoff_speed_rps if profile.backoff_speed_rps > 0.0 else profile.coarse_min_rps
        if speed <= 0.0:
            return
        time_ms = min(15000.0, profile.backoff_turns / speed * 1000.0)
        if time_ms < 1.0:
            return
        self.coarse_rps = -speed
        self.backoff_until_ms = now_ms + time_ms


class ForecastPid(LegacyPid):
    """Today's loop, its gains untouched, fed the staged controller's forecast in place of the reading (2026-11-22).

    The forecast is the reading and the powder turned out that it does not show yet: each tube's turns (reckoned from
    the speeds it asked for, at the motors' ramp) at the calibration's grains a turn, each parcel due after the lag
    the calibration measured at its flow. A tube turned backwards pours nothing. A question for the simulation: what
    the PID loop gains by knowing what is in the air, with nothing else of the staged controller.
    """

    name = "forecast_pid"

    def __init__(self, profile: Profile, cal, acceleration_rps2: float):
        super().__init__(profile)
        import staged
        self.cal = cal
        self.acceleration = acceleration_rps2
        self.lags = {"coarse": staged.lag_table(cal.coarse_stops, cal.coarse_gr_turn),
                     "fine": staged.lag_table(cal.fine_stops, cal.fine_gr_turn)}
        self.gr_turn = {"coarse": cal.coarse_gr_turn, "fine": cal.fine_gr_turn}
        self._table = staged.table

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.speed = {"coarse": 0.0, "fine": 0.0}
        self.parcels = []           # (due s, grains)
        self.last_tick_s = now_ms / 1000.0

    def on_tick(self, now_ms: float) -> None:
        now = now_ms / 1000.0
        dt = now - self.last_tick_s
        self.last_tick_s = now
        for tube, asked in (("coarse", self.coarse_rps), ("fine", self.fine_rps)):
            change = self.acceleration * dt
            v = self.speed[tube]
            v = min(asked, v + change) if v < asked else max(asked, v - change)
            self.speed[tube] = v
            if v > 0.0 and dt > 0.0:
                self.parcels.append((now + self._table(self.lags[tube], v), v * dt * self.gr_turn[tube]))

    def on_reading(self, now_ms: float, weight: float) -> None:
        now = now_ms / 1000.0
        self.parcels = [p for p in self.parcels if p[0] > now]
        super().on_reading(now_ms, weight + sum(p[1] for p in self.parcels))


class GuardedPid(ForecastPid):
    """A design talked over on 2026-11-22, tried in the simulation; not a plan (research/talk_2026-11-22.md).

    Today's PID loop runs the stream on the reading, as it does now. The forecast runs beside it and stops the
    stream early, when it says the powder already turned out will bring the cup to the stop room under the target.
    Then no wait for the scale to be steady: as soon as the forecast holds nothing still to show (under half a digit)
    the reading is taken; under the target by more than the band, the fine tube tops up at its least speed until the
    forecast reaches the stop room again; within the band, done. The stop room is learnt over the session from each
    throw's first such reading, as the staged controller learns its stop.
    """

    name = "guarded_pid"

    def __init__(self, profile: Profile, cal, acceleration_rps2: float, creep_rps: float = None,
                 on_forecast: bool = False):
        super().__init__(profile, cal, acceleration_rps2)
        import staged
        self._staged = staged
        self.stop_room = cal.stop_room_gr
        self.creep_rps = creep_rps if creep_rps is not None else profile.fine_min_rps
        self.on_forecast = on_forecast      # the PID loop's speeds made from the forecast in place of the reading

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.phase = "stream"
        self.first_error = None
        self.tops = 0
        self.stopped_at = None

    def on_reading(self, now_ms: float, weight: float) -> None:
        now = now_ms / 1000.0
        cal = self.cal
        self.parcels = [p for p in self.parcels if p[0] > now]
        unseen = sum(p[1] for p in self.parcels)
        forecast = weight + unseen
        if self.phase in ("stream", "creep") and forecast >= self.target - self.stop_room:
            self.coarse_rps = self.fine_rps = 0.0
            self.phase, self.stopped_at = "settle", now
            return
        if self.phase == "stream":
            LegacyPid.on_reading(self, now_ms, forecast if self.on_forecast else weight)
            if self.done:               # the PID loop's own stop came first
                self.done = False
                self.phase, self.stopped_at = "settle", now
            return
        if self.phase == "creep":
            self.fine_rps = self.creep_rps
            return
        # settle: the reading is taken once nothing is left to show by the forecast, with no wait for steady
        if unseen >= cal.digit_gr / 2.0 or now < self.stopped_at + 2.0 * cal.interval_s:
            return
        error = weight - self.target
        if self.first_error is None:
            self.first_error = error
            off = max(-2.0, min(2.0, (error - cal.aim_gr) / cal.digit_gr))
            if abs(off) >= 0.25:
                self.stop_room += 0.15 * off * cal.digit_gr
        if error < -(cal.band_gr + cal.digit_gr / 4.0) and self.tops < 8:
            self.tops += 1
            self.phase, self.fine_rps = "creep", self.creep_rps
            return
        self.done = True
