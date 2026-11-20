"""The bench's recordings as the figures the simulation is fitted to and checked against.

Reads the raw folders of one bench session (a unit, a powder, a scale; under <repo>/logs/adr009, not tracked) and
writes one JSON of figures (tracked): what `build_rig.py` makes a rig's record from and `fit_report.py` checks it
against. Each figure names the recordings it came from. The session's manifest says which folder is which test,
what the throws' settings were, and what is known of the rig without a recording.

Usage:
    python measure.py --logs C:\\workspace\\OpenTrickler-RP2040-Controller\\logs\\adr009
                      --manifest ../fit/bench_pico2w.manifest.json --out ../fit/bench_pico2w.json
"""

import argparse
import json
import statistics
from pathlib import Path

import stats

LONG_RUN_S = 30.0       # a creep-stop trial this long is a steady run: read for its flow, not only its stop


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


def creep_figures(logs: Path, folders: list) -> dict:
    """The creep-stop trials of the folders named (every one under the logs when none is), by tube, speed, kind
    of stop and whether it was a long steady run."""
    groups = {}
    for folder in sorted(logs / name for name in folders) if folders else sorted(logs.glob("*_creepstop")):
        if not (folder / "runs.json").exists():     # a run still under way
            continue
        for trial in load_runs(folder):
            if trial["note"] != "creepstop":
                continue
            tube = trial.get("tube", "fine")
            stop = trial.get("stop", "held" if trial["held"] else "released")
            length = "steady" if trial["run_s"] >= LONG_RUN_S else "stop"
            key = f"{tube} {trial['speed_asked_rps']:g} {stop} {length}"
            groups.setdefault(key, {"trials": [], "folders": set()})
            groups[key]["trials"].append(trial)
            groups[key]["folders"].add(folder.name)
    out = {}
    for key, group in sorted(groups.items()):
        tube, speed, stop, length = key.split()
        out[key] = {"tube": tube, "speed_rps": float(speed), "stop": stop, "length": length,
                    "run_s": [min(t["run_s"] for t in group["trials"]), max(t["run_s"] for t in group["trials"])],
                    "from": sorted(group["folders"]), **stats.creep_stats(group["trials"])}
    return out


def throw_figures(logs: Path, recorded: dict) -> dict:
    """The recorded throws of today's loop, by the settings each folder ran with."""
    out = {}
    for folder, settings in recorded.items():
        throws = [run for run in load_runs(logs / folder) if run["note"] == "throw"]
        out[settings["name"]] = {**settings, "target": throws[0]["target"], "from": [folder],
                                 **stats.throw_stats(throws)}
    return out


def nudge_figures(logs: Path, recorded: dict) -> dict:
    """A nudge series: each nudge's gain, and for those that released the time until the reading first moved."""
    folder = logs / recorded["folder"]
    frames, runs = load_frames(folder), load_runs(folder)
    gains, moves, turns = [], [], []
    for run in runs:
        span = [(t, w) for t, w in frames if run["t_before_ms"] - 600.0 <= t <= run["t_after_ms"]]
        before = [w for t, w in span if t <= run["t_before_ms"]]
        after = [w for t, w in span if t >= run["t_after_ms"] - 700.0]
        if not before or not after:
            continue
        base = statistics.fmean(before)
        gain = (statistics.fmean(after) - base) / stats.KERNEL_GR
        gains.append(gain)
        turns.append(run["turns"])
        moved = [t for t, w in span if t > run["t_before_ms"] and w >= base + stats.DIGIT_GR / 2.0]
        if gain >= 0.7 and moved:
            moves.append((gain, (moved[0] - run["t_before_ms"]) / 1000.0))
    return {"turn": statistics.fmean(turns), "still_s": recorded["still_s"], "speed_rps": recorded["speed_rps"],
            "from": [recorded["folder"]],
            **stats.nudge_stats(gains, moves)}


def map_figures(logs: Path, recorded: dict) -> dict:
    """Each tube's open-loop map: powder a turn at each speed, and the seconds of its flow that showed after a stop."""
    out = {}
    for tube, name in recorded.items():
        folder = logs / name
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
            run_s = (run["t_stop_asked_ms"] - run["t_before_ms"]) / 1000.0
            rate = gained / run_s
            rows.append({"speed_rps": run["speed_asked_rps"], "gr_per_turn": gained / run["turns"],
                         "gr_per_s": rate, "late_s": (statistics.fmean(after) - at_stop[-1]) / rate})
        rows.sort(key=lambda row: row["speed_rps"])
        out[tube] = {"from": [name], "gr_per_turn": statistics.median(row["gr_per_turn"] for row in rows),
                     "late_s_fast": statistics.median(row["late_s"] for row in rows if row["gr_per_s"] >= 0.3),
                     "runs": rows}
    return out


def scale_figures(logs: Path, folder: str) -> dict:
    """The scale's rhythm from a tapped folder's frames: the usual time between two, and how much it varies."""
    times = [t for t, _ in load_frames(logs / folder)]
    gaps = sorted(later - earlier for earlier, later in zip(times, times[1:]))
    usual = statistics.median(gaps)
    near = [gap for gap in gaps if 0.5 * usual <= gap <= 1.5 * usual]
    return {"from": [folder], "report_s": statistics.fmean(near) / 1000.0, "frames": len(times)}


def session_figures(logs: Path, manifest: dict) -> dict:
    """
    One bench session as figures.

    Args:
        logs: the folder holding the session's raw folders.
        manifest: its name, what the rig was, which folder is which test, and what is stated of the rig.

    Returns:
        the figures by test, with the manifest's name, rig and stated parts copied in.
    """
    recorded = manifest["recordings"]
    return {"name": manifest["name"], "rig": manifest["rig"], "stated": manifest["stated"],
            "scale": scale_figures(logs, recorded["maps"]["fine"]),
            "maps": map_figures(logs, recorded["maps"]),
            "creep": creep_figures(logs, recorded.get("creepstop", [])),
            "nudges": nudge_figures(logs, recorded["nudges"]),
            "throws": throw_figures(logs, recorded.get("throws", {}))}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--logs", required=True, type=Path, help="the checkout's logs/adr009")
    parser.add_argument("--manifest", required=True, type=Path, help="the session's folders, settings and rig")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    figures = session_figures(args.logs, json.loads(args.manifest.read_text(encoding="utf-8")))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(figures, indent=1), encoding="utf-8")
    print(f"wrote {args.out}")
    for key, row in figures["creep"].items():
        fano = ", ".join(f"{row[name]:.2f}" if row[name] is not None else "-" for name in ("fano_1s", "fano_2s", "fano_4s"))
        rate = f"{row['rate']:.2f}" if row["rate"] is not None else "-"
        print(f"  {key}: {row['stops']} stops, {rate} digits/s, variance over mean {fano}, start {row['start_1s']:.2f} "
              f"and {row['start_2s']:.2f}, rise {row['rise']:+.2f} sd {row['rise_sd']:.2f}")
    for name, row in figures["throws"].items():
        print(f"  throws {name}: {row['throws']}, {row['time']:.1f} s, at the stop {row['at_stop']:+.3f}, at rest "
              f"{row['at_rest']:+.3f} sd {row['at_rest_sd']:.3f}, over {row['over']:.2f}")
    nudges = figures["nudges"]
    print(f"  nudges: {nudges['nudges']}, {nudges['kernels']:.2f} kernels, variance over mean {nudges['fano']:.2f}, "
          f"nothing {nudges['nothing']:.2f}, three or more {nudges['three_up']:.2f}")
    for tube, row in figures["maps"].items():
        print(f"  map {tube}: {row['gr_per_turn']:.2f} gr a turn, {row['late_s_fast']:.2f} s of flow after a stop")


if __name__ == "__main__":
    main()
