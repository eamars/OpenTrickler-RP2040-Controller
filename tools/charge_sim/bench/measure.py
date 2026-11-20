"""A bench session's recordings as the figures the simulation is fitted to and checked against.

Reads one session's folders from `recordings/` (a unit, a powder, a scale: `recordings/sessions.json`) and writes
one JSON of figures: what `build_rig.py` makes a rig's record from and `fit_report.py` checks it against. Each
figure names the recordings it came from. The session's manifest (`fit/<session>.manifest.json`) names the folders
no rule finds (the watched nudges, the speed plans with no controller, the throws and their settings, the tapped
maps where the session has them) and what is stated of the rig without a recording.

A session's stops are every `*_creepstop` folder in its time span: never a folder of another powder.

Usage:
    python measure.py --session pico2w_bm2 --out ../fit/pico2w_bm2.json
"""

import argparse
import json
import statistics
from pathlib import Path

import stats

HERE = Path(__file__).resolve().parent
LONG_RUN_S = 30.0       # a creep-stop trial this long is a steady run: read for its flow, not only its stop
SHORT_RUN_S = 1.5       # a creep-stop trial shorter than this is a watched nudge, not a stop
MAP_RUN_S = 2.0         # a stop's run this long gives its tube's powder a turn
FAST_GR_S = 0.3         # a flow this fast shows the scale's delay: a few kernels more or less do not count


def load_runs(folder: Path) -> list:
    return json.loads((folder / "runs.json").read_text(encoding="utf-8"))


def load_frames(folder: Path) -> list:
    """A tapped folder's frames as (unit time in ms, weight)."""
    frames = []
    for line in (folder / "frames.csv").read_text(encoding="utf-8").splitlines()[1:]:
        t_ms, weight, _ = line.split(",")
        if weight not in ("nan", "inf"):
            frames.append((float(t_ms), float(weight)))
    return frames


def session_of(recordings: Path, name: str) -> dict:
    for session in json.loads((recordings / "sessions.json").read_text(encoding="utf-8")):
        if session["name"] == name:
            return session
    raise SystemExit(f"no session named {name} in {recordings / 'sessions.json'}")


def stop_trials(recordings: Path, session: dict) -> list:
    """Every stop of the session, each with its folder's name: (trial, folder)."""
    trials = []
    for folder in sorted(recordings.glob("*_creepstop")):
        if session["from"] <= folder.name[:15] <= session["to"]:
            trials.extend((trial, folder.name) for trial in load_runs(folder)
                          if trial["note"] == "creepstop" and trial["run_s"] >= SHORT_RUN_S)
    return trials


def kind_of(trial: dict) -> tuple:
    """(tube, kind of stop) of a creep-stop trial; folders from before either was recorded are the fine tube's."""
    return trial.get("tube", "fine"), trial.get("stop", "held" if trial["held"] else "released")


def creep_figures(trials: list) -> dict:
    """A session's stops by tube, speed, kind of stop and whether it was a long steady run."""
    groups = {}
    for trial, folder in trials:
        tube, stop = kind_of(trial)
        length = "steady" if trial["run_s"] >= LONG_RUN_S else "stop"
        key = f"{tube} {trial['speed_asked_rps']:g} {stop} {length}"
        groups.setdefault(key, {"trials": [], "folders": set()})
        groups[key]["trials"].append(trial)
        groups[key]["folders"].add(folder)
    out = {}
    for key, group in sorted(groups.items()):
        tube, speed, stop, length = key.split()
        out[key] = {"tube": tube, "speed_rps": float(speed), "stop": stop, "length": length,
                    "run_s": [min(t["run_s"] for t in group["trials"]), max(t["run_s"] for t in group["trials"])],
                    "from": sorted(group["folders"]), **stats.creep_stats(group["trials"])}
    return out


def throw_figures(recordings: Path, recorded: dict) -> dict:
    """The recorded throws of today's loop, by the settings each folder ran with."""
    out = {}
    for folder, settings in recorded.items():
        throws = [run for run in load_runs(recordings / folder) if run["note"] == "throw"]
        out[settings["name"]] = {**settings, "target": throws[0]["target"], "from": [folder],
                                 **stats.throw_stats(throws)}
    return out


def ramp_figures(recordings: Path, folders: list) -> dict:
    """The speed plans turned with no controller, by the plan's name: what arrives after each one's stop."""
    out = {}
    for folder in folders:
        by_name = {}
        for trial in load_runs(recordings / folder):
            if trial["note"] == "ramp":
                by_name.setdefault(trial["name"], []).append(trial)
        for name, trials in by_name.items():
            out[name] = {"tube": trials[0]["tube"], "plan": trials[0]["plan"], "still_s": trials[0]["after"][-1][0],
                         "from": [folder], **stats.ramp_stats(trials)}
    return out


def watch_figures(recordings: Path, folder: str, kernel_gr: float) -> dict:
    """Watched nudges (creep-stop trials of a twentieth of a second): each one's gain, and when the reading moved."""
    gains, moves, turns, speeds, watches = [], [], [], [], []
    for trial in load_runs(recordings / folder):
        if trial["note"] != "creepstop":
            continue
        base = trial["during"][0][1]
        gain = (stats.end_reading(trial) - base) / kernel_gr
        gains.append(gain)
        turns.append(trial["turns"])
        speeds.append(trial["speed_asked_rps"])
        watches.append(trial["after"][-1][0])
        moved = [t for t, w in trial["during"] + trial["after"] if w >= base + stats.DIGIT_GR / 2.0]
        if gain >= 0.7 and moved:
            moves.append((gain, moved[0] + trial["run_s"]))
    return {"turn": statistics.fmean(turns), "still_s": statistics.median(watches),
            "speed_rps": statistics.median(speeds), "from": [folder], **stats.nudge_stats(gains, moves)}


def tapped_map_figures(recordings: Path, recorded: dict) -> dict:
    """Each tube's map from a tapped folder: powder a turn at each speed, and the seconds of its flow after a stop."""
    out = {}
    for tube, name in recorded.items():
        folder = recordings / name
        frames, rows = load_frames(folder), []
        for run in load_runs(folder):
            if run["note"] != "map":
                continue
            before = [w for t, w in frames if run["t_before_ms"] - 600.0 <= t <= run["t_before_ms"]]
            at_stop = [w for t, w in frames if run["t_stop_asked_ms"] - 120.0 <= t <= run["t_stop_asked_ms"]]
            after = [w for t, w in frames if run["t_after_ms"] - 700.0 <= t <= run["t_after_ms"]]
            if not before or not at_stop or not after:
                continue
            gained = statistics.fmean(after) - statistics.fmean(before)
            rate = gained / ((run["t_stop_asked_ms"] - run["t_before_ms"]) / 1000.0)
            rows.append({"speed_rps": run["speed_asked_rps"], "gr_per_turn": gained / run["turns"],
                         "gr_per_s": rate, "late_s": (statistics.fmean(after) - at_stop[-1]) / rate})
        out[tube] = map_of(rows, [name])
    return out


def stop_map_figures(trials: list) -> dict:
    """Each tube's map from the session's own stops, for a session with no tapped map: a row a speed, the powder
    of all its stops over all their turns."""
    by_speed = {}
    for trial, folder in trials:
        tube, stop = kind_of(trial)
        if stop == "held" and trial["run_s"] >= MAP_RUN_S:
            by_speed.setdefault((tube, trial["speed_asked_rps"]), []).append((trial, folder))
    out = {}
    for tube in sorted({tube for tube, _ in by_speed}):
        rows, folders = [], set()
        for (row_tube, speed), group in sorted(by_speed.items()):
            if row_tube != tube:
                continue
            gained = sum(stats.end_reading(trial) - trial["during"][0][1] for trial, _ in group)
            after = sum(stats.end_reading(trial) - trial["during"][-1][1] for trial, _ in group)
            rate = gained / sum(trial["run_s"] for trial, _ in group)
            rows.append({"speed_rps": speed, "gr_per_turn": gained / sum(trial["turns"] for trial, _ in group),
                         "gr_per_s": rate, "late_s": after / len(group) / rate})
            folders.update(folder for _, folder in group)
        out[tube] = map_of(rows, sorted(folders))
    return out


def map_of(rows: list, folders: list) -> dict:
    rows.sort(key=lambda row: row["speed_rps"])
    fast = [row["late_s"] for row in rows if row["gr_per_s"] >= FAST_GR_S]
    return {"from": folders, "gr_per_turn": statistics.median(row["gr_per_turn"] for row in rows),
            "late_s_fast": statistics.median(fast) if fast else None, "runs": rows}


def scale_figures(recordings: Path, folder: str) -> dict:
    """The scale's rhythm from a tapped folder's frames: the usual time between two, and how much it varies."""
    times = [t for t, _ in load_frames(recordings / folder)]
    gaps = sorted(later - earlier for earlier, later in zip(times, times[1:]))
    usual = statistics.median(gaps)
    near = [gap for gap in gaps if 0.5 * usual <= gap <= 1.5 * usual]
    return {"from": [folder], "report_s": statistics.fmean(near) / 1000.0, "frames": len(times)}


def session_figures(recordings: Path, manifest: dict) -> dict:
    """
    One bench session as figures.

    Args:
        recordings: the folder holding every session's recordings and `sessions.json`.
        manifest: the session's name, the folders no rule finds, and what is stated of the rig.

    Returns:
        the figures by test, with the session's unit, powder and scale and the manifest's stated parts copied in.
        The kernel's mass is the session's. `scale` is there only for a session with a tapped map.
    """
    session = session_of(recordings, manifest["session"])
    recorded = manifest["recordings"]
    stated = json.loads(json.dumps(manifest["stated"]))
    stated["powder"]["kernel_gr"] = session["kernel_gr"]
    trials = stop_trials(recordings, session)
    figures = {"name": manifest["name"],
               "rig": {key: session[key] for key in ("unit", "powder", "powder_note", "scale", "from", "to")},
               "stated": stated,
               "maps": tapped_map_figures(recordings, recorded["maps"]) if "maps" in recorded
               else stop_map_figures(trials),
               "creep": creep_figures(trials),
               "nudges": watch_figures(recordings, recorded["nudges"], session["kernel_gr"]),
               "ramps": ramp_figures(recordings, recorded.get("ramps", [])),
               "throws": throw_figures(recordings, recorded.get("throws", {}))}
    if "maps" in recorded:
        figures["scale"] = scale_figures(recordings, recorded["maps"]["fine"])
    return figures


def manifest_of(session: str) -> dict:
    return json.loads((HERE.parent / "fit" / f"{session}.manifest.json").read_text(encoding="utf-8"))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--session", required=True, help="the session's name: fit/<session>.manifest.json")
    parser.add_argument("--recordings", type=Path, default=HERE.parent / "recordings")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    figures = session_figures(args.recordings, manifest_of(args.session))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(figures, indent=1), encoding="utf-8")
    print(f"wrote {args.out}")
    for key, row in figures["creep"].items():
        fano = ", ".join(f"{row[name]:.2f}" if row[name] is not None else "-" for name in ("fano_1s", "fano_2s", "fano_4s"))
        rate = f"{row['rate']:.2f}" if row["rate"] is not None else "-"
        print(f"  {key}: {row['stops']} stops, {rate} digits/s, variance over mean {fano}, start {row['start_1s']:.2f} "
              f"and {row['start_2s']:.2f}, rise {row['rise']:+.2f} sd {row['rise_sd']:.2f}")
    for name, row in figures["ramps"].items():
        print(f"  ramp {name}: {row['ramps']}, gain {row['gain']:.1f} sd {row['gain_sd']:.1f}, rise {row['rise']:+.2f} "
              f"sd {row['rise_sd']:.2f}")
    for name, row in figures["throws"].items():
        print(f"  throws {name}: {row['throws']}, {row['time']:.1f} s, at the stop {row['at_stop']:+.3f}, at rest "
              f"{row['at_rest']:+.3f} sd {row['at_rest_sd']:.3f}, over {row['over']:.2f}")
    nudges = figures["nudges"]
    print(f"  nudges: {nudges['nudges']}, {nudges['kernels']:.2f} kernels, variance over mean {nudges['fano']:.2f}, "
          f"nothing {nudges['nothing']:.2f}, three or more {nudges['three_up']:.2f}")
    for tube, row in figures["maps"].items():
        late = f"{row['late_s_fast']:.2f} s of flow after a stop" if row["late_s_fast"] is not None else "no fast run"
        print(f"  map {tube}: {row['gr_per_turn']:.3f} gr a turn, {late}")


if __name__ == "__main__":
    main()
