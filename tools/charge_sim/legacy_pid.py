"""Today's charge loop, ported line for line from charge_mode.cpp's charge_mode_wait_for_complete().

Two PID loops on the same weight, run once per scale reading. The coarse one aims short of the target by the coarse
stop threshold and is switched off for good on reaching it, then backs off; the fine one aims at the target from
the start. Everything stops when the reading is within the fine stop threshold of the target. The port keeps the
loop's known faults (ADR-009): the I term is not scaled by time, and the back-off ends only when a reading comes.
"""

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
