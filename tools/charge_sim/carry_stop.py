"""The stop's learning fed into the calibration, session after session (ADR-009, the talk of 2026-11-22; not a plan).

The calibration places its stop at the median of its rehearsals' first errors. Here each session's throws add their
own first errors to that list, each translated to the calibration's own stop room (the throw's error, plus the room it
was thrown with, less the calibration's), the newest CARRY_KEPT kept, and the next session's calibration places its
stop from them, never more than CARRY_MOST_DIGITS from where the rehearsals put it. Within a session the stop is learnt
as the firmware does (15% of each first error); the fine figures are not learnt.

A stop placed wrong is played by shifting the rehearsals' first errors (a rehearsal that is not a throw's last
stretch reads them off: the PC's rehearsals on the rig placed the firmware's stop late).

    python carry_stop.py [--rigs pico2w_ar2209,pico2w_bm2] [--targets 5,10] [--runs 12] [--out fit/...]
"""

import argparse
import dataclasses
import statistics
from pathlib import Path

import rigs
import staged
from throw import Session
from workers import Workers

BAND_GR = 0.04
TOLERANCE_GR = 0.05         # over at 0.05 gr and more: within 0.04 on a 0.02 gr digit
SESSIONS = 4
THROWS = 25
CARRY_KEPT = 16
CARRY_MOST_DIGITS = 3.0
OFFSETS_DIGITS = (-2.0, 0.0, 2.0)   # the rehearsals' first errors read this far off: -2, the stop placed late


def placed(rules: staged.Calibration, errors: list, rehearsed_room: float = None) -> staged.Calibration:
    """A calibration with its stop placed at the median of the first errors, as `staged.rehearse` places it."""
    cal = dataclasses.replace(rules, first_errors=list(errors))
    shift = statistics.median(errors) - cal.aim_gr
    if rehearsed_room is not None:
        most = CARRY_MOST_DIGITS * cal.digit_gr
        shift = min(rehearsed_room - rules.stop_room_gr + most, max(rehearsed_room - rules.stop_room_gr - most, shift))
    cal.stop_room_gr += shift
    cal.glide_room_gr += shift
    cal.coarse_room_gr += shift
    return cal


def run(job: tuple) -> list:
    """One run: a calibration, then SESSIONS sessions. Each throw as (session, index, over, good, seconds)."""
    rig_name, target, offset_digits, carry, seed = job
    rig = rigs.get_rig(rig_name)
    measured = staged.measure(rig, seed)
    rules = staged.for_finish(measured, "glide", BAND_GR)
    rehearsed = dataclasses.replace(rules)
    staged.rehearse(rehearsed, Session(rig, seed + 1), rig.acceleration_rps2)
    errors = [error + offset_digits * rules.digit_gr for error in rehearsed.first_errors]
    first_room = placed(rules, errors).stop_room_gr
    session = Session(rig, seed + 2)
    out = []
    for k in range(SESSIONS):
        cal = placed(rules, errors, first_room if k else None)
        controller = staged.Staged(cal, rig.acceleration_rps2)
        controller.learn_fine_prior = 0.0
        for i in range(THROWS):
            room_used = controller.stop_room
            result = session.throw(controller, target, TOLERANCE_GR)
            error = result.true_gr - target
            out.append((k, i, error >= TOLERANCE_GR - 1e-9, abs(error) < TOLERANCE_GR - 1e-9, result.elapsed_s))
            first = controller.first_error
            if carry and first is not None and controller.streams == 0 and result.verdict != "timeout":
                # translated to the rules' own room, where the rehearsals were thrown (2026-11-22 night: it was
                # the placed room, which moved a session's stop by the whole placement of the one before)
                errors = (errors + [first + room_used - rules.stop_room_gr])[-CARRY_KEPT:]
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rigs", default="pico2w_ar2209,pico2w_bm2")
    parser.add_argument("--targets", default="5,10")
    parser.add_argument("--runs", type=int, default=12)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    cases = [(r, float(t), o, c) for r in args.rigs.split(",") for t in args.targets.split(",")
             for o in OFFSETS_DIGITS for c in (False, True)]
    jobs = [(r, t, o, c, args.seed * 1000 + n) for (r, t, o, c) in cases for n in range(args.runs)]
    with Workers() as pool:
        results = pool.map(run, jobs)
    lines = [f"{args.runs} runs a case of {SESSIONS} sessions of {THROWS} throws, glide at {BAND_GR} gr; overs at the "
             f"true weight. Session 1's last ten against sessions 2 to {SESSIONS}' first ten",
             f"{'rig':16s}{'gr':>4s}{'placed':>8s}{'carried':>9s}   {'s1 first10 over':>15s}{'s1 last10 over':>16s}"
             f"{'s2-4 first10 over':>19s}{'s2-4 all over':>15s}{'s2-4 good':>11s}{'s2-4 median':>13s}"]
    for case in cases:
        throws = [t for job, res in zip(jobs, results) if job[:4] == case for t in res]
        share = lambda rows, column: 100.0 * sum(r[column] for r in rows) / max(1, len(rows))
        s1_first = [t for t in throws if t[0] == 0 and t[1] < 10]
        s1_last = [t for t in throws if t[0] == 0 and t[1] >= THROWS - 10]
        later_first = [t for t in throws if t[0] > 0 and t[1] < 10]
        later = [t for t in throws if t[0] > 0]
        r, tgt, o, c = case
        lines.append(f"{r:16s}{tgt:4.0f}{o:+8.0f}{'yes' if c else 'no':>9s}   {share(s1_first, 2):14.1f}%{share(s1_last, 2):15.1f}%"
                     f"{share(later_first, 2):18.1f}%{share(later, 2):14.1f}%{share(later, 3):10.1f}%"
                     f"{statistics.median(t[4] for t in later):12.1f} s")
        print(lines[-1], flush=True)
    if args.out:
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
