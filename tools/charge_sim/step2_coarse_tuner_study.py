"""ADR-009 step 2: the coarse tube's PID tuned under the existing tuner, on the twin (2026-11-26).

The owner: "You should not implement your own algorithm and discard the current coarse PID, so use and tune PID ...
I'd play safe, that don't change the core algorithm, but tune under the existing framework." And on the margin the
coarse tube's powder must leave under the target: "This should be a tuning question, so it should always fit to the
existing framework."

The existing framework is step2_tuner.Tuner: a compass search, a window of throws a trial, judged by the time per
good charge (the share modelled). Here it gains a third axis, the coarse kp, beside the fine tube's steady gain and
braking; the coarse kp is tried up first (the coarse tube is for speed, the owner 2026-11-26), and the cost decides.
No margin of its own: a coarse tube that lands its powder past the target makes bad charges, and the cost counts them.
`bound` adds one more limit, a coarse kp only up to BOUND_GAIN / (step 1's coarse weight a turn x its middle lag), a
number with no unit; the grid of 13:33 found the coarse tube sound at 10 and 30 gr to a kp of 0.05 (0.37 by it) and
not at 0.1 (0.75).

Each session tunes at 10 gr for TUNE_THROWS, then the user changes the charge to 30 gr with the same powder and throws
CHANGED_THROWS more, the tuner on: a coarse kp tuned on a small charge may overshoot a larger one. Printed per arm:
each phase's last throws (mean, SD, worst, time), the throws over by more than OVER_GR, and the coarse kp ended on.

    venv\\Scripts\\python tools\\charge_sim\\step2_coarse_tuner_study.py [--sessions 4]
"""

import argparse
import dataclasses
import math
import statistics

import step2
import step2_tuner
from step2_tuner import Tuner
from step2_variants import BENCH
from throw import Session
from workers import Workers
import rigs

RIG = "pico2w_ar2209_f2"
TUNE_TARGET = 10.0
CHANGED_TARGET = 30.0
TUNE_THROWS = 300
CHANGED_THROWS = 150
JUDGED = 75                     # each phase's last throws judged
OVER_GR = 0.5                   # a throw over by more than this: the coarse tube's powder past the target
BOUND_GAIN = 0.4
ARMS = ("fine only (today's tuner)", "fine and coarse", "fine and coarse, bounded",
        "fine only, the coarse kp put right", "fine only, the coarse kp put right to the bound")


class CoarseTuner(Tuner):
    """The existing tuner with a third axis, the coarse kp: tried up first, judged by the same cost."""

    def __init__(self, *args, coarse_k: float = None, bound: bool = False, **kwargs):
        super().__init__(*args, axes="kp+kd", **kwargs)
        self.axes = ("gain", "braking", "coarse")
        self.coarse_k = coarse_k
        self.bound = bound

    def missing_way(self, errors: list) -> int:
        if self.axes[self.axis] == "coarse":
            return 1                # for speed: quicker first
        return super().missing_way(errors)

    def coarse_most(self):
        if not self.bound or not self.coarse_k or not self.lag:
            return math.inf
        return BOUND_GAIN / (self.coarse_k * self.lag)

    def moves(self, trial) -> bool:
        base = self.base
        return (trial.fine_kp, trial.fine_kd, trial.coarse_kp) != (base.fine_kp, base.fine_kd, base.coarse_kp)

    def trial(self):
        if self.axes[self.axis] != "coarse":
            return super().trial()
        kp = min(self.base.coarse_kp * self.factor ** self.direction, self.coarse_most())
        return dataclasses.replace(self.base, coarse_kp=max(kp, 0.0))

    def next_trial(self, said: str):
        trial = self.trial()
        if self.moves(trial):
            return trial, said
        return self.put_back(f"{said}; no move")

    def put_back(self, said: str):
        while True:
            self.tried += 1
            if self.tried % 2 == 1:
                self.direction = -self.direction
                said_now = f"{said}: the other way"
            elif self.tried < 2 * len(self.axes):
                self.axis = (self.axis + 1) % len(self.axes)
                self.direction = self.missing_way(self.pool[0])
                said_now = f"{said}: the {self.axes[self.axis]}"
            else:
                self.tried = 0
                self.factor = max(step2_tuner.LEAST_FACTOR, math.sqrt(self.factor))
                self.resting = step2_tuner.REST_WINDOWS
                self.axis = 0
                self.direction = self.missing_way(self.pool[0])
                return self.base, f"{said}: put back every way, resting, the step now x{self.factor:.2f}"
            trial = self.trial()
            if self.moves(trial):
                return trial, said_now


class PutRightTuner(Tuner):
    """Today's tuner, its first window's gains put right reaching the coarse kp too: brought down to the bound when
    above it (kept only if cheaper, as the fine gains put right are); never tuned up."""

    def __init__(self, *args, coarse_k: float = None, raise_to_bound: bool = False, **kwargs):
        super().__init__(*args, axes="kp+kd", **kwargs)
        self.coarse_k = coarse_k
        self.raise_to_bound = raise_to_bound

    def put_right(self):
        right = super().put_right() or self.base
        gain = right.coarse_kp * self.coarse_k * self.lag if self.coarse_k and self.lag else None
        if gain is not None and (gain > BOUND_GAIN or (self.raise_to_bound and gain < BOUND_GAIN * 0.99)):
            right = dataclasses.replace(right, coarse_kp=BOUND_GAIN / (self.coarse_k * self.lag))
        return None if right is self.base else right


def run(job: tuple) -> tuple:
    arm, seed, records, start_kp = job
    rig = rigs.get_rig(RIG)
    profile = dataclasses.replace(BENCH, coarse_kp=start_kp)
    session = Session(rig, seed)
    controller = step2.Step2(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none",
                             cut="reading")
    lag = statistics.median(lag for _, lag in records.lag)
    if arm == ARMS[0]:
        tuner = Tuner(profile.fine_stop_threshold, z=0.0, axes="kp+kd", share="modelled")
    elif arm in (ARMS[3], ARMS[4]):
        tuner = PutRightTuner(profile.fine_stop_threshold, z=0.0, share="modelled", coarse_k=records.coarse_gr_turn,
                              raise_to_bound=arm == ARMS[4])
    else:
        tuner = CoarseTuner(profile.fine_stop_threshold, z=0.0, share="modelled", coarse_k=records.coarse_gr_turn,
                            bound=arm == ARMS[2])
    phases = []
    for target, throws in ((TUNE_TARGET, TUNE_THROWS), (CHANGED_TARGET, CHANGED_THROWS)):
        out, errors, times = [], [], []
        for _ in range(throws):
            controller.profile = profile
            result = step2.throw_one(session, controller, target, profile.fine_stop_threshold)
            out.append((result.true_error, result.elapsed_s, profile.coarse_kp))
            errors.append(result.true_error)
            times.append(result.elapsed_s)
            if len(errors) == step2_tuner.WINDOW:
                profile, _ = tuner(profile, errors, times, k=controller.carried(), lag=lag)
                errors, times = [], []
        phases.append(out)
    return arm, phases, (profile.coarse_kp, profile.fine_kp, profile.fine_kd)


def line(throws: list) -> str:
    errors = [e for e, _, _ in throws]
    times = sorted(t for _, t, _ in throws)
    return (f"mean {statistics.fmean(errors):+.3f} sd {statistics.pstdev(errors):.3f} worst {max(errors):+.2f} "
            f"time {times[len(times) // 2]:4.1f} s")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sessions", type=int, default=4)
    parser.add_argument("--coarse-kp", type=float, default=BENCH.coarse_kp, help="the coarse kp the sessions start on")
    args = parser.parse_args()
    with Workers() as pool:
        records = pool.map(step2.rig_records, [RIG])[0]
        results = pool.map(run, [(arm, 7700 + s, records, args.coarse_kp) for arm in ARMS
                                 for s in range(args.sessions)])
    print(f"{RIG}, the bench's gains (coarse kp {args.coarse_kp}, threshold {BENCH.coarse_stop_threshold:g}), "
          f"{args.sessions} sessions: {TUNE_THROWS} throws at {TUNE_TARGET:g} gr, then {CHANGED_THROWS} at "
          f"{CHANGED_TARGET:g} gr, the tuner on throughout; step 1's coarse weight a turn {records.coarse_gr_turn}")
    for arm in ARMS:
        mine = [r for r in results if r[0] == arm]
        print(f"\n{arm}")
        for idx, target in enumerate((TUNE_TARGET, CHANGED_TARGET)):
            last = [t for _, phases, _ in mine for t in phases[idx][-JUDGED:]]
            every = [t for _, phases, _ in mine for t in phases[idx]]
            over = sum(e > OVER_GR for e, _, _ in every)
            kps = sorted({round(kp, 4) for _, phases, _ in mine for _, _, kp in phases[idx]})
            print(f"  {target:4g} gr, last {JUDGED} each: {line(last)}; over {OVER_GR:g} gr in all {over} of "
                  f"{len(every)}; coarse kps thrown {kps[0]:g} to {kps[-1]:g}")
        ends = "; ".join(f"coarse {c:.3g} fine {f:.3g}/{d:.0f}" for _, _, (c, f, d) in mine)
        print(f"  ended on {ends}")


if __name__ == "__main__":
    main()
