"""ADR-009 step 2: the use cases on the twin, the normal path and its failures, step 2 as it ships (2026-11-26).

The owner: "I want you to also try more experiments, for failure modes, or out of ordinary exceptions, like: 1.
Starting with the absurdly wrong PID 2. Someone accidentally put integral term 3. Different powder? I want you to map
all use cases against your simulation, out of the known experiment so far, then conduct the experiment with your
simulation."

Each case is step 2 as it ships (the forecast seeded by step 1, the throw never cut on it, the tuner on kp and kd by
the share modelled) on a rig of the twin, from a start's gains with what the case changes, at a target. Step 1's
records are made on the rig the case calibrated on: the healthy rig for a failure that comes in use (an empty hopper,
a draught, a wrong seed), the case's own for a powder or a scale it was calibrated with. Each prints the last throws'
SD, mean, 5% to 95%, worst, shares under, good and over at the case's tolerance, median and 95% time, timeouts, a
good charge's cost (counted), and the gains the tuner ended on.

    venv\\Scripts\\python tools\\charge_sim\\step2_cases.py [--only <names>] [--sessions 2]
"""

import argparse
import dataclasses
import math
import statistics
from dataclasses import dataclass, field

import rigs
import step2
import step2_tuner
from legacy_pid import AR2209_PICOW_168
from step2_variants import BENCH
from throw import Session
from workers import Workers

F2 = "pico2w_ar2209_f2"
STARTS = {"bench": BENCH, "owner": AR2209_PICOW_168}


@dataclass(frozen=True)
class Case:
    name: str
    what: str
    rig: str = F2
    calibrated_on: str = None       # the rig step 1 ran on; None, the case's own
    start: str = "bench"
    changes: dict = field(default_factory=dict)     # the profile's fields changed from the start's
    seed: object = 1.0              # step 1's weight a turn times this; None, never calibrated
    target: float = 10.0
    tuned: bool = True
    throws: int = 300
    judged: int = 100               # the last throws of each session judged


CASES = (
    # the normal path
    Case("reference, fixed", "the bench's stored gains, no tuner", tuned=False, throws=150, judged=150),
    Case("reference", "the bench's stored gains, the tuner on"),
    Case("reference, owner's", "the owner's hand-tuned gains, the tuner on", start="owner"),
    Case("3 gr", "a charge under the coarse tube's threshold: the fine tube alone", target=3.0),
    Case("30 gr", "a larger charge", target=30.0),
    Case("60 gr", "a large charge", target=60.0),
    Case("big kernels", "kernels three times as heavy (an assumed powder)", rig=F2 + "+big_kernel"),
    Case("big kernels, owner's", "the same from the owner's gains", rig=F2 + "+big_kernel", start="owner"),
    Case("big kernels, fixed", "the same, no tuner", rig=F2 + "+big_kernel", tuned=False, throws=150, judged=150),
    Case("ball powder", "BM2, the twin's ball powder", rig="pico2w_bm2"),
    Case("tolerance 0.02", "the user's tolerance tighter", changes={"fine_stop_threshold": 0.02}),
    Case("tolerance 0.1", "the user's tolerance looser", changes={"fine_stop_threshold": 0.1}),
    Case("grams", "the scale read in grams (a digit of 0.0154 gr)", rig=F2 + "+gram_digit"),
    Case("switch off, owner's", "the owner's gains, the tuner off", start="owner", tuned=False, throws=150,
         judged=150),
    # failure modes and exceptions
    Case("kp / 100", "an absurdly low kp (0.02)", changes={"fine_kp": 0.02}),
    Case("kp x 100", "an absurdly high kp (200)", changes={"fine_kp": 200.0}),
    Case("kp x 100, fixed", "the same, no tuner", changes={"fine_kp": 200.0}, tuned=False, throws=150, judged=150),
    Case("kd 0", "no D term", changes={"fine_kd": 0.0}),
    Case("kd 100000", "an absurdly high kd", changes={"fine_kd": 100000.0}),
    Case("ki 0.01", "an integral term put in by mistake", changes={"fine_ki": 0.01}),
    Case("ki 0.1", "a larger one", changes={"fine_ki": 0.1}),
    Case("ki 1", "an absurd one", changes={"fine_ki": 1.0}),
    Case("ki 0.1, fixed", "the same, no tuner", changes={"fine_ki": 0.1}, tuned=False, throws=150, judged=150),
    Case("seed x 0.5", "another powder's calibration: its weight a turn half this one's", seed=0.5),
    Case("seed x 2", "another powder's calibration: twice this one's", seed=2.0),
    Case("never calibrated", "no step 1 for this powder: no seed", seed=None),
    Case("never calibrated, owner's", "the same from the owner's gains", seed=None, start="owner"),
    Case("never calibrated, first 50", "no seed: its first throws", seed=None, throws=50, judged=50),
    Case("calibrated, first 50", "the reference's first throws", throws=50, judged=50),
    Case("fine hopper empty", "the fine tube pours next to nothing", rig=F2 + "+fine_empty", calibrated_on=F2,
         tuned=False, throws=10, judged=10),
    Case("coarse hopper empty", "the coarse tube pours next to nothing", rig=F2 + "+coarse_empty", calibrated_on=F2,
         tuned=False, throws=10, judged=10),
    Case("draught", "gusts on the pan after the calibration", rig=F2 + "+draught", calibrated_on=F2),
    Case("missing reports", "one report in twenty lost", rig=F2 + "+missing", calibrated_on=F2),
    Case("wild coarse", "the coarse kp ten times the bench's (0.2)", changes={"coarse_kp": 0.2}),
    Case("night rig", "the twin's rig with the most powder in the air at the coarse tube's stop",
         rig="pico2w_ar2209_night"),
)


def run(job: tuple) -> tuple:
    case, seed, records = job
    rig = rigs.get_rig(case.rig)
    profile = dataclasses.replace(STARTS[case.start], **case.changes)
    if case.seed is not None and records.fine_gr_turn:
        records = dataclasses.replace(records, fine_gr_turn=records.fine_gr_turn * case.seed)
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=case.seed is not None,
                             hold="none", cut="reading")
    tuner = step2_tuner.Tuner(profile.fine_stop_threshold, z=0.0, axes="kp+kd", share="modelled") if case.tuned \
        else None
    out, errors, times = [], [], []
    for _ in range(case.throws):
        controller.profile = profile
        result = step2.throw_one(session, controller, case.target, profile.fine_stop_threshold)
        out.append((result.true_error, result.elapsed_s, "starved" if controller.starved else result.verdict))
        errors.append(result.true_error)
        times.append(result.elapsed_s)
        if tuner is not None and len(errors) == step2_tuner.WINDOW:
            profile, _ = tuner(profile, errors, times, k=controller.carried())
            errors, times = [], []
    starved = sum(v == "starved" for _, _, v in out)
    return case.name, out[-case.judged:], (profile.fine_kp, profile.fine_kd, profile.fine_ki), starved


def line(throws: list, tolerance: float) -> str:
    starved = sum(v == "starved" for _, _, v in throws)
    done = [(e, t) for e, t, v in throws if v not in ("timeout", "starved")]
    timeouts = len(throws) - len(done) - starved
    if not done:
        ended = [t for _, t, v in throws if v == "starved"]
        return (f"no throw finished: {starved} starved (stopped in {min(ended):.1f} to {max(ended):.1f} s), "
                f"{timeouts} timed out" if ended else f"every throw timed out ({timeouts})")
    errors = sorted(e for e, _ in done)
    times = sorted(t for _, t in done)
    n = len(errors)
    good = sum(abs(e) < tolerance for e in errors) / n
    under = sum(e <= -tolerance for e in errors) / n
    over = sum(e >= tolerance for e in errors) / n
    cost = times[n // 2] / good if good else math.inf
    return (f"sd {statistics.pstdev(errors):.3f}  mean {statistics.fmean(errors):+.3f}  5-95% "
            f"{errors[int(0.05 * (n - 1))]:+.2f}..{errors[int(0.95 * (n - 1))]:+.2f}  worst {errors[-1]:+.2f}  "
            f"under {under:4.0%} good {good:4.0%} over {over:4.0%}  time {times[n // 2]:4.1f} s "
            f"(95% {times[int(0.95 * (n - 1))]:4.1f})  timeouts {timeouts}  starved {starved}  a good charge "
            f"{'inf' if math.isinf(cost) else f'{cost:.1f}'} s")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--only", nargs="*", help="the cases run, by name (all by default)")
    parser.add_argument("--sessions", type=int, default=2)
    args = parser.parse_args()
    cases = [c for c in CASES if not args.only or c.name in args.only]
    calibrations = sorted({c.calibrated_on or c.rig for c in cases})
    with Workers() as pool:
        made = dict(zip(calibrations, pool.map(step2.rig_records, calibrations)))
        jobs = [(c, 9900 + s, made[c.calibrated_on or c.rig]) for c in cases for s in range(args.sessions)]
        results = pool.map(run, jobs)
    for case in cases:
        mine = [r for r in results if r[0] == case.name]
        throws = [t for _, part, _, _ in mine for t in part]
        starved_all = sum(s for *_, s in mine)
        tolerance = dataclasses.replace(STARTS[case.start], **case.changes).fine_stop_threshold
        ends = "; ".join(f"kp {kp:.3g} kd {kd:.0f}" + (f" ki {ki:g}" if ki else "") for _, _, (kp, kd, ki), _ in mine)
        print(f"{case.name:26s} {line(throws, tolerance)}  (starved in all {starved_all})")
        print(f"{'':26s} {case.what}; {case.target:g} gr, {args.sessions} x {case.throws}, the last {case.judged} each"
              + (f"; the tuner ended on {ends}" if case.tuned else ""))


if __name__ == "__main__":
    main()
