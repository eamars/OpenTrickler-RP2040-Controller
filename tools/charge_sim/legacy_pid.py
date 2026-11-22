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


class LearntPid(LegacyPid):
    """Today's PID loop with the glide's continuous learning put in, and no calibration (ADR-009, 2026-11-22 evening).

    The owner's question: can the legacy PID system, with the learning the glide has, do as well or better? Not a
    plan: a design tried in the simulation, then on the rig (research/pid_learning_2026-11-22.md).

    What stays of today's loop: the profile's gains, its least and top speeds, the coarse tube's stop short of the
    target by the coarse stop threshold and its back-off. What is added, each a number of the scale's make or learnt
    from the throws, none measured by a calibration:
    - on_forecast: the fine PID acts on the reading plus the fine tube's powder still in the air, its turns over the
      last lag_s at a figure learnt from the throws (a Smith predictor). False: on the reading, as today.
    - the stop: the fine tube stops when the forecast is within the stop room of the target (the reading, when not
      on_forecast). On the forecast the room is the glide's aim, a digit short (half the band, if more).
    - a rest: one settled reading, rest_s after the stop and no rise for still_s, as the glide's verdict.
    - a top-up: short by more than the band (and a quarter digit), the stream again: today's loop turns the fine
      tube at its least speed or more, stopped as before (on the reading, at the profile's own fine stop threshold,
      since the learnt room is the stream's from full speed); then a rest again; at most most_tops.
    - the learning, at each throw's first rest, a mean of the throws so far with the newest weighing at least
      LEARN_SHARE (a fresh powder learnt in a throw or two, then followed as it drifts):
      on_forecast: the figure, from what landed after the stop over the turns the window held then;
      not on_forecast: the stop room, by the first error against the aim, as the glide learns its stop.
    The figure starts cautious (gr_turn_prior high: the forecast holds too much, the throw stops short, the top-up
    mends it) or from what a session before carried. 2.0 since the rig of 2026-11-22, where 1.0 was not cautious.
    - floor_band_share (the second experiment, 2026-11-22 night): the fine tube's least speed from the learnt figure,
      the glide's rule without its calibration: the speed at which the powder in the air (figure x lag_s x speed) is
      that share of the band; never above the profile's least speed (with the motor's), never below
      floor_least_rps. 0: the profile's, as today.
    """

    name = "learnt_pid"
    LEARN_SHARE = 0.15
    WINDOW_STEP_S = 0.01

    def __init__(self, profile: Profile, digit_gr: float, interval_s: float, band_gr: float = 0.04,
                 on_forecast: bool = True, lag_s: float = 0.9, gr_turn_prior: float = 2.0, rest_s: float = 2.5,
                 acceleration_rps2: float = 50.0, most_tops: int = 8, coarse_quiet_s: float = 1.0,
                 floor_band_share: float = 0.0, floor_least_rps: float = 0.02):
        # Today's loop inside, its own fine stop taken out: the stop is the room's
        super().__init__(dataclasses.replace(profile, fine_stop_threshold=-1e9))
        self.digit = digit_gr
        self.interval_s = interval_s
        self.band = band_gr
        self.on_forecast = on_forecast
        self.lag_s = lag_s
        self.gr_turn = gr_turn_prior
        self.rest_s = rest_s
        self.acceleration = acceleration_rps2
        self.most_tops = most_tops
        self.coarse_quiet_s = coarse_quiet_s
        self.threshold = profile.fine_stop_threshold
        self.floor_band_share = floor_band_share
        self.floor_least_rps = floor_least_rps
        self.profile_least_rps = profile.fine_min_rps
        self.aim = -max(digit_gr, band_gr / 2.0)
        self.stop_room = -self.aim if on_forecast else profile.fine_stop_threshold
        self.learnt = 0             # throws the learning has taken in
        self.still_s = max(0.4, 2.0 * interval_s)

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.phase = "stream"
        self.fine_speed = 0.0       # the fine tube's speed through the motor's ramp
        self.window = []            # fine turns a WINDOW_STEP_S, newest last
        self.window_s = now_ms / 1000.0
        self.window_turns = 0.0
        self.last_tick_s = now_ms / 1000.0
        self.coarse_off_s = None
        self.stop_s = None
        self.stop_reading = None
        self.stop_window_turns = None
        self.first_error = None
        self.tops = 0
        self.highest = None
        self.lowest = 0.0
        self.last_rise_s = 0.0
        self.reading = 0.0

    def window_held(self) -> float:
        """The fine tube's turns over the last lag_s."""
        steps = max(1, round(self.lag_s / self.WINDOW_STEP_S))
        return sum(self.window[-steps:]) + self.window_turns

    def unseen(self) -> float:
        """The fine tube's powder in the air: its turns over the last lag_s at the learnt figure."""
        return self.gr_turn * self.window_held()

    def least_rps(self) -> float:
        """The fine tube's least speed: the profile's, or the one at which the band's share is in the air."""
        if not (self.floor_band_share > 0.0 and self.on_forecast and self.gr_turn > 0.0):
            return self.profile_least_rps
        learnt = self.floor_band_share * self.band / (self.gr_turn * self.lag_s)
        return min(self.profile_least_rps, max(self.floor_least_rps, learnt))

    def room(self) -> float:
        """How far short of the target the fine tube stops."""
        if self.on_forecast or self.tops == 0:
            return self.stop_room
        return min(self.stop_room, self.threshold)

    def forecast(self) -> float:
        return self.reading + (self.unseen() if self.on_forecast else 0.0)

    def on_tick(self, now_ms: float) -> None:
        now = now_ms / 1000.0
        dt = now - self.last_tick_s
        self.last_tick_s = now
        change = self.acceleration * dt
        v = self.fine_speed
        v = min(self.fine_rps, v + change) if v < self.fine_rps else max(self.fine_rps, v - change)
        self.fine_speed = v
        if v > 0.0:
            self.window_turns += v * dt
        while now >= self.window_s + self.WINDOW_STEP_S:
            self.window.append(self.window_turns)
            self.window_turns = 0.0
            self.window_s += self.WINDOW_STEP_S
            if len(self.window) > 400:
                del self.window[:100]
        if self.backoff_until_ms is not None and now_ms >= self.backoff_until_ms:
            self.coarse_rps = 0.0       # the back-off ends on time, not on the next reading
            self.backoff_until_ms = None
        if self.coarse_off_s is None and not self.coarse_moves and self.backoff_until_ms is None:
            self.coarse_off_s = now

    def _stop(self, now: float) -> None:
        self.fine_rps = 0.0
        if self.backoff_until_ms is None:
            self.coarse_rps = 0.0
        self.phase = "rest"
        self.stop_s = now
        self.highest = None

    def on_reading(self, now_ms: float, weight: float) -> None:
        now = now_ms / 1000.0
        self.reading = weight
        if self.highest is None or weight >= self.highest + self.digit / 2.0:
            self.highest = weight
            self.lowest = weight
            self.last_rise_s = now
        self.lowest = min(self.lowest, weight)
        if self.backoff_until_ms is not None and now_ms >= self.backoff_until_ms:
            self.coarse_rps = 0.0
            self.backoff_until_ms = None
        if weight > self.target + 3.0:
            self.coarse_rps = self.fine_rps = 0.0
            self.done = True
            return
        if self.phase == "stream":
            self._stream(now_ms, now)
        elif self.phase == "rest":
            self._rest(now)

    def _stream(self, now_ms: float, now: float) -> None:
        forecast = self.forecast()
        if forecast >= self.target - self.room():
            if self.first_error is None and self.stop_reading is None:
                # What lands after the fine tube stops, the pause's stop or the stop's, is what the window held
                self.stop_reading = self.reading
                self.stop_window_turns = self.window_held()
            if self.coarse_moves:
                self.coarse_moves = False
                self.coarse_rps = 0.0
                self._backoff(now_ms)
                self.coarse_off_s = None
            if self.coarse_off_s is not None and now >= self.coarse_off_s + self.coarse_quiet_s:
                self._stop(now)
            else:
                self.fine_rps = 0.0     # a pause, not the stop: the coarse tube's powder is still landing
            return
        # Today's loop, on the forecast (or the reading); its own fine stop is the room's, taken above
        floor = self.least_rps()
        if floor != self.profile.fine_min_rps:
            self.profile = dataclasses.replace(self.profile, fine_min_rps=floor)
        LegacyPid.on_reading(self, now_ms, forecast)
        self.done = False

    def _rest(self, now: float) -> None:
        if now < self.stop_s + self.rest_s or now < self.last_rise_s + self.still_s:
            return
        at_rest = self.lowest
        error = at_rest - self.target
        if self.first_error is None:
            self.first_error = error
            self._learn(at_rest)
        band_edge = self.band + 0.25 * self.digit
        if -error > band_edge and self.tops < self.most_tops:
            # The top-up is the stream again: today's loop gives at least its least speed, stopped as before
            self.tops += 1
            self.phase = "stream"
            return
        self.done = True

    def _learn(self, at_rest: float) -> None:
        share = max(self.LEARN_SHARE, 1.0 / (self.learnt + 1))
        if self.on_forecast:
            turns = self.stop_window_turns
            if turns is None or turns < 0.01:
                return
            seen = (at_rest - self.stop_reading) / turns
            seen = min(2.0 * self.gr_turn, max(0.5 * self.gr_turn, seen))
            self.gr_turn += share * (seen - self.gr_turn)
        else:
            self.stop_room = max(0.0, self.stop_room + share * (self.first_error - self.aim))
        self.learnt += 1
