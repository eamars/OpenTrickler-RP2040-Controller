"""The scale's lag in stages (ADR-009, gate 3, proposal L; the owner's requirement, 2026-11-24): the fit.

After a stop from a steady flow, what was turned out and not yet shown arrives as a dead time `d` and one first-order
stage `tau`: the reading keeps rising at the flow's rate until `d`, then closes on its end as 1 - e^(-(t-d)/tau). A
second, slower stage (its share `s2` and `tau2`) is added only when the first leaves a residual over a digit (RMS).
Fitted by least squares to each stop's run after the stop, the flow being the run's gain over its time.

This file is the fit and its check; the forecast reads it once it holds (progress.md).

    python lag_stages.py            each stand-in's stops fitted: their RMS against a digit, and the stages used
"""

import math
import statistics
import sys

import f4_cases
import staged

D_STEP_S = 0.01
D_MOST_S = 4.0
TAU_LEAST_S, TAU_MOST_S, TAU_STEPS = 0.02, 6.0, 80


def model(t: float, flow: float, d: float, tau: float, share2: float = 0.0, tau2: float = 0.0) -> float:
    """What has shown by t after the stop, of the powder still unseen at it (in grains)."""
    unseen = flow * (d + (1.0 - share2) * tau + share2 * tau2)
    if t < d:
        return flow * t
    late = t - d
    stage1 = (1.0 - share2) * tau * (1.0 - math.exp(-late / tau)) if tau > 0 else (1.0 - share2) * 0.0
    stage2 = share2 * tau2 * (1.0 - math.exp(-late / tau2)) if share2 > 0 and tau2 > 0 else 0.0
    return min(unseen, flow * (d + stage1 + stage2))


def taus():
    ratio = (TAU_MOST_S / TAU_LEAST_S) ** (1.0 / (TAU_STEPS - 1))
    return [TAU_LEAST_S * ratio ** i for i in range(TAU_STEPS)]


def fit(trial: dict, digit: float) -> dict:
    """One stop's stages: {d, tau, share2, tau2, rms, flow}, the rms in grains against the readings after the stop."""
    run_s = trial["run_s"]
    before = trial["during"][0][1]
    at_stop = trial["during"][-1][1]
    after = [(t, w - at_stop) for t, w in trial["after"]]
    end = statistics.median(w for t, w in after if t >= after[-1][0] - 0.6)
    flow = (at_stop + end - before) / run_s if run_s > 0 else 0.0    # the run's powder over its time
    if flow <= 0.0 or end <= 0.0:
        return {"d": 0.0, "tau": 0.0, "share2": 0.0, "tau2": 0.0, "rms": 0.0, "flow": flow}
    # the model's whole is the rise after the stop: flow x (d + tau) = end; so tau follows d (one stage)

    def rms_of(d, tau, share2=0.0, tau2=0.0):
        return math.sqrt(statistics.fmean((model(t, flow, d, tau, share2, tau2) - w) ** 2 for t, w in after))

    best = None
    d = 0.0
    total = end / flow
    while d <= min(D_MOST_S, total):
        tau = total - d
        rms = rms_of(d, tau)
        if best is None or rms < best[0]:
            best = (rms, d, tau)
        d += D_STEP_S
    rms, d, tau = best
    out = {"d": d, "tau": tau, "share2": 0.0, "tau2": 0.0, "rms": rms, "flow": flow}
    if rms <= digit:
        return out
    # a second stage: the share and the slow stage's time, the first's from the whole
    best2 = None
    for share2 in (0.1, 0.2, 0.3, 0.4, 0.5, 0.6):
        for tau2 in taus():
            d2 = d
            tau1_part = total - d2 - share2 * tau2
            if tau1_part <= 0.0:
                continue
            tau1 = tau1_part / (1.0 - share2)
            if tau1 >= tau2:
                continue
            r = rms_of(d2, tau1, share2, tau2)
            if best2 is None or r < best2[0]:
                best2 = (r, tau1, share2, tau2)
    if best2 is not None and best2[0] < rms:
        out.update(rms=best2[0], tau=best2[1], share2=best2[2], tau2=best2[3])
    return out


class Recorded:
    def __init__(self, bench):
        self.bench, self.runs = bench, []

    def __getattr__(self, name):
        return getattr(self.bench, name)

    def creep_stop(self, tube, speed_rps, run_s, watch_s, stop, **kw):
        trial = self.bench.creep_stop(tube, speed_rps, run_s, watch_s, stop, **kw)
        self.runs.append(trial)
        return trial


def main() -> int:
    worst = {}
    for name in f4_cases.names():
        rig = f4_cases.stand_in(name)
        digit = rig.scale.digit_gr
        bench = Recorded(staged.TwinBench(rig, 7))
        staged.measure_on(bench, digit, rig.scale.report_s, staged.Limits(0.1, 5.0, 0.08, 5.0))
        fits = [(run["tube"], run["speed_asked_rps"], fit(run, digit)) for run in bench.runs
                if run["stop"] == "held" and run["run_s"] >= 1.0]
        rms = [f["rms"] / digit for _, _, f in fits]
        two = sum(1 for _, _, f in fits if f["share2"] > 0)
        over = sum(1 for r in rms if r > 1.0)
        worst[name] = max(rms)
        fine = [f for tube, _, f in fits if tube == "fine"]
        print(f"{name:10s} {len(fits)} stops: RMS in digits median {statistics.median(rms):.2f}, most {max(rms):.2f}; "
              f"over a digit {over}; two stages {two}; fine d {statistics.median(f['d'] for f in fine):.2f} s, "
              f"tau {statistics.median(f['tau'] for f in fine):.2f} s", flush=True)
    failing = [name for name in ("slower", "gng", "strain") if worst[name] > 1.0]
    print("VERDICT PASS: every stop fitted within a digit on slower, gng and strain" if not failing else
          f"VERDICT FAIL: a stop over a digit on {failing}")
    return 0 if not failing else 1


if __name__ == "__main__":
    sys.exit(main())
