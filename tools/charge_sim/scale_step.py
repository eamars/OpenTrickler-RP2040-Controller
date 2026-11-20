"""How long each scale model takes to show a weight put on its pan: the measurement's lag, in its parts.

For a step of one kernel and of one grain, placed at a known moment: when the reading first moves, when it is
half-way, and when it stays within a digit of the end. The rig's own scale is its record's, measured; the others
are the estimates in scale.py. A known weight set on a real pan gives the same three times to set beside these.

Usage: python scale_step.py [--rig pico2w_ar2209]
"""

import argparse
import random
import statistics

import rigs
from scale import SCALES, ScaleState

DT_S = 0.001
RUNS = 40


def step_times(model, step_gr: float, seed: int) -> tuple:
    """(first change, half-way, settled) in seconds after a step lands on the pan, as the charge loop sees it."""
    rng = random.Random(seed)
    scale = ScaleState(model, rng, DT_S)
    now = 0.0
    start = 1.0 + rng.random() * model.report_s
    first = half = None
    last_outside = start
    while now < start + 6.0:
        now += DT_S
        reading = scale.step(now, step_gr if now >= start else 0.0)
        if reading is None or now < start:
            continue
        if first is None and reading > 0.5 * model.digit_gr:
            first = now - start
        if half is None and reading >= 0.5 * step_gr:
            half = now - start
        if abs(reading - step_gr) > 1.01 * model.digit_gr:
            last_outside = now
    return first, half, last_outside - start


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="pico2w_ar2209")
    args = parser.parse_args()
    print("model, step: first change / half-way / within a digit of the end (median seconds over %d runs)" % RUNS)
    for model in (rigs.get_rig(args.rig).scale, *SCALES.values()):
        for step_gr in (0.022, 1.0):
            runs = [step_times(model, step_gr, seed) for seed in range(RUNS)]
            medians = [statistics.median(r[index] for r in runs if r[index] is not None) for index in range(3)]
            print(f"{model.name:28s} {step_gr:5.3f} gr: {medians[0]:.2f} / {medians[1]:.2f} / {medians[2]:.2f}")


if __name__ == "__main__":
    main()
