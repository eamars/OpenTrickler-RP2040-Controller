"""Today's loop against the candidate as the plant's unknowns are varied: which assumptions decide the result.

Usage: python matrix.py [--throws 400] [--target 42.5]
Each line is one plant and one controller; the candidate is calibrated afresh on each plant, as a unit would be.
"""

import argparse
import dataclasses
from concurrent.futures import ProcessPoolExecutor

from controllers import CONTROLLERS
from mc import THROWS_PER_SESSION, format_summary, summarise
from rigs import make_rig
from throw import Session

CONTROLLER_NAMES = ("legacy", "flow_dose")


def cases() -> list:
    """(label, rig) for every plant in the comparison."""
    base = make_rig()
    out = [("as assumed", base)]
    for clump, regularity in ((1.0, 8.0), (1.0, 2.0), (2.0, 2.0)):
        powder = dataclasses.replace(base.powder, clump_mean=clump, regularity=regularity)
        out.append((f"clump {clump:g}, regularity {regularity:g}", dataclasses.replace(base, powder=powder)))
    for factor in (0.5, 3.0):
        fine = dataclasses.replace(base.fine, out_per_rev=base.fine.out_per_rev * factor)
        out.append((f"fine tube flow x{factor:g}", dataclasses.replace(base, fine=fine)))
    scale = dataclasses.replace(base.scale, draught_per_min=6.0, draught_gr=0.06, draught_tau_s=1.0)
    out.append(("draughts, 6 a minute", dataclasses.replace(base, scale=scale)))
    for scale_name in ("and_slow", "gng", "slow"):
        out.append((f"scale {scale_name}", make_rig("ar2209", scale_name)))
    for powder_name in ("ball", "long_stick"):
        out.append((f"powder {powder_name}", make_rig(powder_name)))
    return out


def run_case(job: tuple) -> str:
    """One plant and one controller in a worker; gives the table's line."""
    label, rig, controller_name, target, throws = job
    controller, tolerance = CONTROLLERS[controller_name](rig)
    results = []
    for index in range(max(1, throws // THROWS_PER_SESSION)):
        session = Session(rig, 9000 + index)
        results += [session.throw(controller, target, tolerance) for _ in range(THROWS_PER_SESSION)]
    return format_summary(f"{label} / {controller_name}", summarise(results, tolerance))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=400)
    parser.add_argument("--target", type=float, default=42.5)
    args = parser.parse_args()
    jobs = [(label, rig, name, args.target, args.throws) for label, rig in cases() for name in CONTROLLER_NAMES]
    with ProcessPoolExecutor() as pool:
        for line in pool.map(run_case, jobs):
            print(line)


if __name__ == "__main__":
    main()
