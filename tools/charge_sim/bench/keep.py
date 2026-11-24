"""The bench's recordings copied into the repository, so that anyone can analyse them.

A bench session writes into <repo>/logs/adr009, which git does not track. This copies what an analysis reads
(each folder's runs.json, frames.csv and batch.json, and a trial's calibration and throws) into `tools/charge_sim/recordings/`, and writes the index of
what each folder is. Left out: the console's own logs (four times the size, and no tool reads them), and from a
copy of the unit's settings its Wi-Fi, its update source and its id.

Which unit, powder and scale a folder belongs to is in `recordings/sessions.json`, written by hand from the
session's notes: a folder is the session's whose time span holds its stamp.

A series of the unit's own controller (`series.py`, a folder holding `series.json`: each throw's line and rows) is
copied to `<folder>_<session>`, as the series of 2026-11-21 and 22 were, and listed in the index as a series.

Usage, after a bench session:
    python keep.py --logs C:\\workspace\\OpenTrickler-RP2040-Controller\\logs\\adr009
"""

import argparse
import json
import shutil
from pathlib import Path

KEPT = ("runs.json", "frames.csv", "batch.json", "calibration.json", "trial.json")
KEPT_LIKE = ("throws_*.json",   # a trial's sets (`trial.py`)
             "calibration_*.json")    # a calibration part's records (`calibrate_unit.py`, ADR-009 step 1)
# Resources of a settings copy that say where the unit is and not how it charges
PRIVATE_RESOURCES = ("/api/v1/settings/wifi", "/api/v1/settings/update")
PRIVATE_KEYS = ("unit_id",)


def session_of(stamp: str, sessions: list) -> dict:
    """The session whose time span holds the folder's stamp (YYYYMMDD_HHMMSS)."""
    for session in sessions:
        if session["from"] <= stamp <= session["to"]:
            return session
    raise SystemExit(f"no session in sessions.json holds {stamp}: add the session it belongs to")


def trials_of(runs: list) -> list:
    return [run for run in runs if run.get("note") != "precondition"]


def speeds_of(trials: list) -> str:
    """The speeds a folder's trials asked for, or how many speed plans it ran."""
    speeds = sorted({trial["speed_asked_rps"] for trial in trials if "speed_asked_rps" in trial})
    plans = sorted({trial["name"] for trial in trials if "name" in trial})
    if speeds:
        return ", ".join(f"{speed:g}" for speed in speeds) + " rev/s"
    if plans:
        return f"{len(plans)} speed plans"
    return ""


def index_line(folder: Path, session: dict, what: str) -> str:
    runs = json.loads((folder / "runs.json").read_text(encoding="utf-8"))
    trials = trials_of(runs)
    tubes = ", ".join(sorted({trial["tube"] for trial in trials if "tube" in trial}))
    test = folder.name.split("_", 2)[2]
    return f"| `{folder.name}` | {session['powder']} | {test} | {tubes} | {len(trials)} | {speeds_of(trials)} | {what} |"


def keep_settings(source: Path, target: Path) -> None:
    settings = json.loads(source.read_text(encoding="utf-8"))
    for key in PRIVATE_KEYS:
        settings.pop(key, None)
    for resource in PRIVATE_RESOURCES:
        settings["resources"].pop(resource, None)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(settings, indent=1), encoding="utf-8")


def main() -> None:
    here = Path(__file__).resolve().parent.parent / "recordings"
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--logs", type=Path, required=True, help="the bench's folder: <repo>/logs/adr009")
    parser.add_argument("--out", type=Path, default=here)
    args = parser.parse_args()

    sessions = json.loads((args.out / "sessions.json").read_text(encoding="utf-8"))
    whats = {}      # what a batch said each of its folders was
    for batch_file in sorted(args.logs.glob("*_batch_*/batch.json")):
        for step in json.loads(batch_file.read_text(encoding="utf-8"))["steps"]:
            whats[step["folder"]] = f"step {step['step']}: {step['what']}"

    lines = []
    for folder in sorted(path for path in args.logs.iterdir() if path.is_dir()):
        files = [folder / name for name in KEPT if (folder / name).exists()] + sorted(
            path for like in KEPT_LIKE for path in folder.glob(like))
        if folder.name.startswith("settings"):
            keep_settings(folder / "settings.json", args.out / folder.name / "settings.json")
            continue
        series_file = folder / "series.json"
        if series_file.exists():
            record = json.loads(series_file.read_text(encoding="utf-8"))
            if not record.get("throws"):
                continue
            session = session_of(folder.name[:15], sessions)
            # A series kept before keeps its folder (those of 2026-11-21 and 22 were kept by hand as
            # `<folder>_pico2w_ar2209`); a new one is named after its session
            kept_before = sorted(path for path in args.out.glob(f"{folder.name}_*") if path.is_dir() and
                                 not path.name.endswith(f"_{session['name']}"))
            target = kept_before[0] if kept_before else args.out / f"{folder.name}_{session['name']}"
            target.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(series_file, target / "series.json")
            law = record.get("controller") or "staged"
            if "speed_law=1" in (record.get("ccon_asked") or ""):
                law += ", PID law"
            if record.get("pour"):      # since ADR-009's gate F.5: the controller, or the console's glide or legacy
                law = record["pour"]
            lines.append(f"| `{target.name}` | {session['powder']} | series ({law}) | | {len(record['throws'])} | "
                         f"{record.get('target_gr', '')} gr | {record.get('ccon_asked') or ''} |")
            continue
        if not files:
            continue
        runs_file = folder / "runs.json"
        if all(file.name in ("runs.json", "frames.csv") for file in files) and runs_file.exists() \
                and not json.loads(runs_file.read_text(encoding="utf-8")):
            continue        # a session that turned nothing: a trial gone on with --resume keeps its throws elsewhere
        (args.out / folder.name).mkdir(parents=True, exist_ok=True)
        for file in files:
            shutil.copyfile(file, args.out / folder.name / file.name)
        if (folder / "runs.json").exists():
            lines.append(index_line(folder, session_of(folder.name[:15], sessions), whats.get(folder.name, "")))

    index = ["# The recordings, a line a folder", "",
             "Made by `bench/keep.py`; do not edit. A folder's unit and scale are its session's, in `sessions.json`.",
             "Trials are counted without the turn that preconditions the tube.", "",
             "| Folder | Powder | Test | Tube | Trials | Speeds asked | What its batch said it was |",
             "|---|---|---|---|---|---|---|"] + lines
    (args.out / "index.md").write_text("\n".join(index) + "\n", encoding="utf-8")
    print(f"{len(lines)} recordings kept in {args.out}")


if __name__ == "__main__":
    main()
