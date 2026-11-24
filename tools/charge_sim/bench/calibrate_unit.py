"""ADR-009, gate 1: the calibration's method from the PC on a unit, one part a run (`../calibration.py`).

A part is the scale (`scale`), or one tube's speeds (`fine`, `coarse`), run alone: a tube's speeds use the scale's
defaults unless `--scale-from` names an earlier scale run's records. The ladder of a tube's speeds runs between its
motor's own least and most speed, read from the unit. A run that would pass the cup's capacity stops the part and says
so: nothing is retried.

Usage, with the repository's venv, from this folder; the unit on the probe and the real cup checked first (rc_run):
    python calibrate_unit.py --repo <checkout> --part scale --capacity 250 [--cup-emptied "<the owner's words>"]
    python calibrate_unit.py --made-up pico2w_ar2209_night --part fine     (the simulation, through the same path)

The part's records go to `calibration_<part>.json` in the session's folder (`logs/adr009/<stamp>_calibration_<part>`),
beside its runs; the made-up unit's to `--out`.
"""

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import calibration  # noqa: E402  (../calibration.py)
import record       # noqa: E402  (beside this file)
import rigs         # noqa: E402
import trial        # noqa: E402  (beside this file: the unit's session and its bench)

MADE_UP_LIMITS = {"fine": (0.01, 0.5), "coarse": (0.08, 5.0)}   # the twin's fine tube holds to 0.5 rev/s only


def motor_limits(unit, tube: str) -> tuple:
    """A real unit's motor's own least and most speed, from its stored settings (`settings/motors/<tube>`)."""
    unit._send(f"setting settings/motors/{tube}")
    fields = {}
    for line in unit.bench.session.console.data_lines:
        if line.startswith("@setting ") and "=" in line:
            name, value = line[len("@setting "):].split("=", 1)
            fields[name] = value
    return float(fields["min_speed_rps"]), float(fields["max_speed_rps"])


def stop_when_full(poured_gr: float, most_gr: float) -> None:
    """On a unit, a full cup stops the part (gate 1 runs a part into a cup that holds it)."""
    raise trial.UnitTrouble(f"the cup holds {poured_gr:.1f} gr of this part and the next run may pour {most_gr:.1f} gr "
                            "more: past its capacity. Empty the cup and run the part again")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--part", required=True, choices=("scale", "fine", "coarse"))
    parser.add_argument("--repo", type=Path, help="the checkout whose rc_run.py matches the unit's firmware")
    parser.add_argument("--board", default="pico2_w")
    parser.add_argument("--made-up", help="a record: the part run on the simulation, no unit")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--capacity", type=float, default=calibration.DEFAULT_CUP_CAPACITY_GR,
                        help="the cup's capacity, gr: the most the part may pour into it")
    parser.add_argument("--cup-emptied", help="the owner's words that the cup was emptied")
    parser.add_argument("--elf", help="the image on the unit, when it is not the board's own build folder's")
    parser.add_argument("--scale-from", type=Path, help="an earlier scale part's records, for a tube's speeds")
    parser.add_argument("--out", type=Path, default=HERE.parent / "fit", help="where a made-up unit's records go")
    args = parser.parse_args()

    scale = json.loads(args.scale_from.read_text(encoding="utf-8"))["records"] if args.scale_from else None
    if args.made_up:
        unit = trial.MadeUpUnit(rigs.get_rig(args.made_up), args.seed)
        folder = args.out / f"calibration_made_up_{args.made_up}_seed{args.seed}"
        on_full = None
    else:
        if args.repo is None:
            parser.error("a unit's part needs --repo")
        record.CUP_EMPTIED = args.cup_emptied
        record.ELF = args.elf
        unit = trial.RealUnit(args.repo, f"calibration_{args.part}", args.board, 0.02, 0.048, 50.0, args.capacity)
        folder = unit.folder
        on_full = stop_when_full
    cup = calibration.Cup(capacity_gr=args.capacity, on_full=on_full)
    report = None
    try:
        unit.ready()
        bench = trial.UnitBench(unit)
        if args.part == "scale":
            report = calibration.scale_part(bench, cup)
        else:
            least, most = MADE_UP_LIMITS[args.part] if args.made_up else motor_limits(unit, args.part)
            report = calibration.speeds_part(bench, cup, args.part, least, most, scale)
    except trial.UnitTrouble as trouble:
        print(f"STOPPED: {trouble}", flush=True)
    finally:
        unit.stop()
    if report is not None:
        calibration.say(report)
        folder.mkdir(parents=True, exist_ok=True)
        kept = {"part": report.part, "records": report.records, "failures": report.failures,
                "seconds": report.seconds, "poured_gr": report.poured_gr, "cup_pauses": cup.pauses}
        (folder / f"calibration_{args.part}.json").write_text(json.dumps(kept, indent=1), encoding="utf-8")
        print(f"kept in {folder}", flush=True)
    return unit.close() if report is not None else 1


if __name__ == "__main__":
    sys.exit(main())
