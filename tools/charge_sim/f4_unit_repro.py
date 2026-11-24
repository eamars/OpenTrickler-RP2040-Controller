"""F.4's unit run reproduced in the simulation (ADR-009, the review's job 3, 2026-11-24): why a powder never calibrated
went over 3 of 3 on the A&D.

The unit's run (progress.md, 2026-11-24, 01:47): the learnt source, lag 2 s and rest 6 s assumed, digit 0.02, tolerance
0.03, the back-off on (0.25 turn at 1 rev/s, no spill measured: the coarse stop threshold, 5 gr), profile 1's gains
(coarse kp 0.02, kd 0.3; fine kp 2, kd 10) and limits (coarse 0.1 to 5, fine 0.08 to 5 rev/s), 3 throws at 10 gr.
Here on the rig of record (`measured`), the coarse tube given the bench's reverse spill (0.83 gr, A11's row). Arms:
- A, as the unit: the controller given the plant's own count of each tube's turns every millisecond, which has no
  direction, as the unit's count of the STEP pin has none;
- B, as the simulation has thrown: the controller counting its own signed ramp;
- C: as A, the back-off off;
- D, a diagnostic added after the first run: as A, with the lag and rest a calibration on this scale measures (the
  fine stops' longest lag and the calibration's rest, as `f4_cases.py`'s `learnt_scale`) in place of 2 s and 6 s.
The prediction was written in progress.md before the first run (2026-11-24, 21:41).

Usage, with the repository's venv, from this folder:
    python f4_unit_repro.py [--sessions 40]
"""

import argparse
import collections
import dataclasses
import statistics
import sys

import f4_cases
import staged
from throw import Session
from workers import Workers

TARGET_GR = 10.0
TOLERANCE_GR = 0.03
THROWS = 3
LAG_S, REST_S = 2.0, 6.0
LIMITS = (0.1, 5.0, 0.08, 5.0)
BACKOFF = (0.25, 1.0)
SPILL_GR = 0.83
GAINS = {"coarse_kp": 0.02, "coarse_kd": 0.3, "fine_kp": 2.0, "fine_kd": 10.0}
ARMS = {"A": (True, True), "B": (False, True), "C": (True, False), "D": (True, True)}  # (unit's count, back-off)


class Counted(staged.Staged):
    """The reference, given each millisecond the plant's own turns since the throw began (no direction), or not; and
    the coarse turns that became powder after the back-off's window, kept for the account."""

    def bind(self, session, counted: bool) -> None:
        self.session = session
        self.unit_count = counted

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.base = {"coarse": self.session.coarse.turns, "fine": self.session.fine.turns}
        self.backoff_seen = False
        self.after_backoff_turns = 0.0
        self.last_coarse = 0.0

    def on_tick(self, now_ms: float) -> None:
        if self.unit_count:
            turns = {tube: getattr(self.session, tube).turns - self.base[tube] for tube in ("coarse", "fine")}
            if self.backoff_seen and self.backoff_until is None:
                self.after_backoff_turns += turns["coarse"] - self.last_coarse
            self.last_coarse = turns["coarse"]
            self.count(now_ms, turns)
        super().on_tick(now_ms)
        if self.backoff_until is not None:
            self.backoff_seen = True


def session_job(job: tuple) -> list:
    arm, seed = job
    counted, backoff = ARMS[arm]
    staged.COARSE_FLOOR_RPS, staged.COARSE_TOP_RPS, staged.FINE_LEAST_RPS, staged.FINE_TOP_RPS = LIMITS
    base = f4_cases.stand_in("measured")
    rig = dataclasses.replace(base, coarse=dataclasses.replace(base.coarse, reverse_spill_gr=SPILL_GR))
    limits = staged.Limits(*LIMITS, backoff_turns=BACKOFF[0] if backoff else 0.0,
                           backoff_rps=BACKOFF[1] if backoff else 0.0, coarse_threshold_gr=5.0)
    digit = rig.scale.digit_gr
    lag, rest = LAG_S, REST_S
    if arm == "D":
        measured = staged.for_finish(staged.measure(rig, seed + 50), "glide", TOLERANCE_GR - digit / 2.0)
        lag = max(lag_ for _, lag_ in staged.lag_table(measured.fine_stops, measured.fine_gr_turn))
        rest = measured.rest_s
    cal = staged.learnt_calibration(digit, rig.scale.report_s, TOLERANCE_GR - digit / 2.0, lag, rest, limits)
    controller = Counted(cal, rig.acceleration_rps2)
    controller.pid = staged.profile_pid(limits, **GAINS)
    session = Session(rig, seed + 300)
    controller.bind(session, counted)
    out = []
    for _ in range(THROWS):
        coarse_before, fine_before = session.coarse.released_gr, session.fine.released_gr
        fine_turns_before = session.fine.turns
        r = session.throw(controller, TARGET_GR, TOLERANCE_GR, timeout_s=f4_cases.TIMEOUT_S)
        fine_turns = session.fine.turns - fine_turns_before
        fine_gr = session.fine.released_gr - fine_before
        out.append({"result": controller.result if controller.done else "timeout",
                    "error_digits": r.true_error / digit, "elapsed": r.elapsed_s, "streams": controller.streams,
                    "first_error_digits": (controller.first_error / digit if controller.first_error is not None
                                           else None),
                    "figure": controller.cal.fine_gr_turn, "fine_poured_turn": fine_gr / fine_turns if fine_turns else 0,
                    "coarse_figure": controller.cal.coarse_gr_turn,
                    "coarse_poured": session.coarse.released_gr - coarse_before,
                    "after_backoff_turns": controller.after_backoff_turns, "lag": lag, "rest": rest})
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--sessions", type=int, default=40)
    args = parser.parse_args()
    jobs = [(arm, seed) for arm in ARMS for seed in range(args.sessions)]
    with Workers() as pool:
        results = pool.map(session_job, jobs)
    by_arm = collections.defaultdict(list)
    for (arm, _), throws in zip(jobs, results):
        by_arm[arm].append(throws)
    print(f"the unit: 3 of 3 over (+8, +8, +2 digits), streams 1 each, fine figure 0.30 / 0.26 / 0.26, "
          f"17 to 24 s", flush=True)
    for arm, sessions in by_arm.items():
        throws = [t for s in sessions for t in s]
        ends = collections.Counter(t["result"] for t in throws)
        overs = [t for t in throws if t["result"] == "over"]
        two_of_three = sum(1 for s in sessions if sum(t["result"] == "over" for t in s) >= 2)
        after_stream = sum(1 for t in overs if t["streams"] >= 1)
        lag = statistics.median(t["lag"] for t in throws)
        rest = statistics.median(t["rest"] for t in throws)
        print(f"arm {arm} (unit count {ARMS[arm][0]}, back-off {ARMS[arm][1]}, lag median {lag:.2f} s, rest "
              f"{rest:.2f} s): "
              f"{dict(ends)} of {len(throws)}; "
              f"sessions with 2 or more over {two_of_three} of {len(sessions)}; overs after a return to the stream "
              f"{after_stream} of {len(overs)}", flush=True)
        for i in range(THROWS):
            nth = [s[i] for s in sessions]
            err = sorted(t["error_digits"] for t in nth)
            print(f"  throw {i + 1}: over {sum(t['result'] == 'over' for t in nth)} of {len(nth)}; error digits "
                  f"median {statistics.median(err):+.1f} (90% {err[int(0.9 * (len(err) - 1))]:+.1f}); streams>=1 "
                  f"{sum(t['streams'] >= 1 for t in nth)}; fine figure after median "
                  f"{statistics.median(t['figure'] for t in nth):.3f}, the tube poured "
                  f"{statistics.median(t['fine_poured_turn'] for t in nth):.3f} a turn; coarse figure "
                  f"{statistics.median(t['coarse_figure'] for t in nth):.2f}; coarse turns after the back-off's "
                  f"window {statistics.median(t['after_backoff_turns'] for t in nth):.3f}; time median "
                  f"{statistics.median(t['elapsed'] for t in nth):.1f} s", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
