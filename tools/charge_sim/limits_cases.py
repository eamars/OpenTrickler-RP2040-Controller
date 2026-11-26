"""The user's settings in the frame (ADR-009, gate F.2): the reference with other users' limits and tolerance.

The calibration is measured once on the rig of record (`records/pico2w_ar2209_night.json`) within the simulation's
stand-in user (`staged.sim_limits()`: coarse 0.3 to 2.0, fine 0.01 to 0.5 rev/s, the range its plant models). Each case
then makes the rules for another user's limits or tolerance in the reference (`staged.set_rules`) and throws. A case
passes when its pass, written in `progress.md` before the build (2026-11-23, 23:09, on branch adr009_controller),
holds. On that branch the firmware's C made the same rules and threw in lockstep beside the reference; the restart of
ADR-009 left the C out:

- user_max: a fine most of 0.3 rev/s, below the fastest fine stop measured (0.5): no fine speed above 0.3 in the
  stream in any throw;
- user_least: a fine least of 0.1 rev/s, above the creep: no fine speed under 0.1 while the fine tube streams, and the
  creep said held (the rules' floor above their creep);
- coarse_limits: a coarse range of 0.5 to 1.5 rev/s: no coarse speed outside it while it turns;
- tolerance: a tolerance of 0.05 gr: the band is 0.04 (less half the 0.02 digit), and each verdict is judged on it.

Usage, with the repository's venv, from this folder:
    python limits_cases.py [--throws 20]
"""

import argparse
import dataclasses
import sys

import rigs
import staged
from throw import Session

RECORD = "pico2w_ar2209_night"
SEED = 4242
TARGET_GR = 20.0
TOLERANCE_GR = 0.03
SPEED_TIE = 1e-6


class Watched:
    """The reference given the unit's whole milliseconds, its asked speeds kept while it streams."""

    def __init__(self, reference):
        self.reference = reference
        self.coarse, self.fine = [], []

    def __getattr__(self, name):
        return getattr(self.reference, name)

    def _keep(self):
        ref = self.reference
        if ref.phase == "feed":
            if ref.coarse_rps > 0.0:
                self.coarse.append(ref.coarse_rps)
            if ref.fine_rps > 0.0:
                self.fine.append(ref.fine_rps)

    def start(self, target_gr: float, now_ms: float) -> None:
        self.reference.start(target_gr, float(round(now_ms)))
        self._keep()

    def on_tick(self, now_ms: float) -> None:
        self.reference.on_tick(float(round(now_ms)))
        self._keep()

    def on_reading(self, now_ms: float, weight: float) -> None:
        self.reference.on_reading(float(round(now_ms)), weight)
        self._keep()


def throws(cal, rig, count):
    """The reference's throws. Returns [(result, rest weight, coarse speeds, fine speeds, the reading a second on)]."""
    reference = staged.Staged(cal, rig.acceleration_rps2)
    session = Session(rig, SEED + 5)
    out = []
    for _ in range(count):
        watched = Watched(reference)
        result = session.throw(watched, TARGET_GR, cal.band_gr + cal.digit_gr / 2.0)
        # the verdict is the controller's settled reading, given at once (gate F.5, E9): the weight it judged; the
        # reading a second later is kept beside it, to see what landed after the verdict
        out.append((reference.result, reference.lowest, watched.coarse, watched.fine, result.reading))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--throws", type=int, default=20)
    args = parser.parse_args()
    rig = rigs.get_rig(RECORD)
    digit = rig.scale.digit_gr
    base = staged.sim_limits()
    measured = staged.measure(rig, SEED)
    failures = []
    cases = {
        "user_max": (dataclasses.replace(base, fine_most=0.3), TOLERANCE_GR),
        "user_least": (dataclasses.replace(base, fine_least=0.1), TOLERANCE_GR),
        "coarse_limits": (dataclasses.replace(base, coarse_least=0.5, coarse_most=1.5), TOLERANCE_GR),
        "tolerance": (base, 0.05),
    }
    for name, (limits, tolerance) in cases.items():
        band = tolerance - digit / 2.0
        cal = staged.for_finish(measured, "glide", band, limits)
        staged.rehearse(cal, Session(rig, SEED + 1), rig.acceleration_rps2)
        rows = throws(cal, rig, args.throws)
        coarse = [s for row in rows for s in row[2]]
        fine = [s for row in rows for s in row[3]]
        results = [row[0] for row in rows]
        line = (f"{name}: band {cal.band_gr:.4f}, coarse {cal.coarse_floor_rps:g} to {cal.coarse_top_rps:g}, fine "
                f"{cal.floor_rps:.4f} (creep {cal.creep_rps:.4f}) to {cal.fine_top_rps:g}; streamed coarse "
                f"{min(coarse, default=0):.3f} to {max(coarse, default=0):.3f}, fine {min(fine, default=0):.4f} to "
                f"{max(fine, default=0):.4f}; results {dict((r, results.count(r)) for r in set(results))}")
        print(line)
        if any(r not in ("good", "over", "short") for r in results):
            failures.append(f"{name}: a throw ended otherwise: {results}")
        if name == "user_max" and max(fine) > 0.3 + SPEED_TIE:
            failures.append(f"user_max: a fine speed {max(fine):.4f} above the user's 0.3")
        if name == "user_least":
            if min(fine) < 0.1 - SPEED_TIE:
                failures.append(f"user_least: a fine speed {min(fine):.4f} under the user's 0.1")
            if not cal.floor_rps > cal.creep_rps:
                failures.append("user_least: the creep is not held up")
        if name == "coarse_limits" and (min(coarse) < 0.5 - SPEED_TIE or max(coarse) > 1.5 + SPEED_TIE):
            failures.append(f"coarse_limits: a coarse speed outside 0.5 to 1.5: {min(coarse):.3f} to {max(coarse):.3f}")
        if name == "tolerance":
            if abs(cal.band_gr - 0.04) > 1e-9:
                failures.append(f"tolerance: the band is {cal.band_gr}, not 0.04")
            edge = cal.band_gr + staged.BAND_EDGE_DIGITS * digit
            later = sum(1 for row in rows if row[4] - row[1] >= digit - 1e-9)
            print(f"tolerance: {later} of {len(rows)} throws read a digit or more higher a second after their verdict")
            for result, rest, *_ in rows:
                want = "good" if abs(rest - TARGET_GR) <= edge + 1e-9 else ("over" if rest > TARGET_GR else "short")
                if result != want and abs(abs(rest - TARGET_GR) - edge) > 1e-6:
                    failures.append(f"tolerance: a rest at {rest:.3f} judged {result}, by the band {want}")
    # zero_lag (F.5, D1's floor gone): the slowest fine stop measured a lag of 0 (its powder showed at once). The creep
    # is taken at the slowest stop with a lag above zero; it stays finite and within the ladder
    zero = dataclasses.replace(measured, fine_stops=[dataclasses.replace(measured.fine_stops[0], lag_s=0.0)]
                               + list(measured.fine_stops[1:]))
    expected = staged.for_finish(zero, "glide", TOLERANCE_GR - digit / 2.0)
    print(f"zero_lag: creep {expected.creep_rps:.4f} rev/s, the ladder "
          f"{zero.fine_stops[0].speed:.3f} to {zero.fine_stops[-1].speed:.3f}")
    if not (0.0 < expected.creep_rps <= zero.fine_stops[-1].speed + 1e-9):
        failures.append("zero_lag: the creep not finite and within the ladder")
    for failure in failures:
        print("FAIL", failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
