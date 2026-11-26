"""The scale's answer to a tube's flow, run by run: a delay, then a first-order lag (ADR-009, 2026-11-22).

A run turned out its powder evenly (the motors' ramps included) and the reading followed. Each run is fitted with
two numbers: the delay before its first powder shows, and the lag tau with which it then shows. Their sum is the
run's mean lag, which `staged.behind` measures with no shape assumed; it is printed beside them as a check.

    ..\\..\\..\\venv\\Scripts\\python scale_shape.py ..\\recordings\\20261121_145252_trial_pico2w_ar2209
"""

import argparse
import json
import math
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from staged import behind     # noqa: E402

ACCELERATION_RPS2 = 50.0
GRID_S = 0.002


def turned_out(run_s: float, speed: float, t0: float, t1: float) -> list:
    """(time, turns) on a grid: from rest at -run_s up to speed by the ramp, held, ramped down from 0."""
    points, turns, v, t = [], 0.0, 0.0, t0
    while t <= t1:
        asked = speed if -run_s <= t < 0.0 else 0.0
        change = ACCELERATION_RPS2 * GRID_S
        v = min(asked, v + change) if v < asked else max(asked, v - change)
        turns += v * GRID_S
        points.append(turns)
        t += GRID_S
    return points


def lagged(points: list, tau: float) -> list:
    if tau <= 0.0:
        return points
    keep = math.exp(-GRID_S / tau)
    out, q = [], 0.0
    for p in points:
        q = p + (q - p) * keep
        out.append(q)
    return out


def at(series: list, t0: float, t: float) -> float:
    x = (t - t0) / GRID_S
    if x <= 0.0:
        return series[0]
    i = int(x)
    if i >= len(series) - 1:
        return series[-1]
    return series[i] + (series[i + 1] - series[i]) * (x - i)


def fit(run: dict, digit: float) -> dict:
    samples = sorted({(t, w) for t, w in run["during"] + run["after"]})
    before = statistics.median(w for t, w in run["during"] if t <= -run["run_s"] + 0.02) \
        if any(t <= -run["run_s"] + 0.02 for t, w in run["during"]) else run["during"][0][1]
    tail = [w for t, w in run["after"] if t >= run["after"][-1][0] - 0.6]
    end = statistics.median(tail)
    gain = end - before
    if gain < 5 * digit:
        return None
    quiet = next((t for t, w in run["after"] if w >= end - digit / 2.0), run["after"][-1][0])
    use = [(t, w - before) for t, w in samples if t <= quiet + 1.0]
    t0 = -run["run_s"] - 0.05
    points = turned_out(run["run_s"], run["speed_asked_rps"], t0, use[-1][0] + 0.05)
    scale = gain / points[-1]

    def cost(delay, tau, series):
        return sum((scale * at(series, t0, t - delay) - w) ** 2 for t, w in use)

    best = None
    for tau in [i * 0.02 for i in range(0, 31)]:
        series = lagged(points, tau)
        for delay in [0.1 + i * 0.01 for i in range(0, 121)]:
            c = cost(delay, tau, series)
            if best is None or c < best[0]:
                best = (c, delay, tau)
    _, d0, k0 = best
    for tau in [max(0.0, k0 + i * 0.004) for i in range(-5, 6)]:
        series = lagged(points, tau)
        for delay in [d0 + i * 0.002 for i in range(-5, 6)]:
            c = cost(delay, tau, series)
            if c < best[0]:
                best = (c, delay, tau)
    c, delay, tau = best
    # the same fit with no lag: how much the second number buys
    flat = min(cost(0.1 + i * 0.005, 0.0, points) for i in range(0, 241))
    lag = behind(run, before, end, quiet) / gain
    return {"tube": run["tube"], "speed": run["speed_asked_rps"], "gain": gain, "delay": delay, "tau": tau,
            "rms": math.sqrt(c / len(use)), "rms_delay_only": math.sqrt(flat / len(use)), "lag": lag}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    parser.add_argument("--digit", type=float, default=0.02)
    args = parser.parse_args()
    fits = []
    for folder in args.folders:
        for run in json.loads((folder / "runs.json").read_text(encoding="utf-8")):
            if run.get("note") != "creepstop" or "during" not in run:
                continue
            result = fit(run, args.digit)
            if result is not None:
                fits.append(result)
    groups = {}
    for f in fits:
        groups.setdefault((f["tube"], round(f["speed"], 3)), []).append(f)
    print("tube    rev/s  runs  gain gr  delay s  tau s (min-max)    delay+tau  mean lag  rms digits  delay alone")
    for (tube, speed), group in sorted(groups.items()):
        med = lambda key: statistics.median(g[key] for g in group)
        taus = [g["tau"] for g in group]
        print(f"{tube:7s} {speed:5.3f} {len(group):5d} {med('gain'):8.2f} {med('delay'):8.3f} {med('tau'):6.3f} "
              f"({min(taus):.3f}-{max(taus):.3f}) {med('delay') + med('tau'):9.3f} {med('lag'):9.3f} "
              f"{med('rms') / args.digit:10.2f} {med('rms_delay_only') / args.digit:12.2f}")


if __name__ == "__main__":
    main()
