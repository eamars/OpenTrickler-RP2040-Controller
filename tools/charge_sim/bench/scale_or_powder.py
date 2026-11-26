"""Is what arrives after a stop the scale's or the powder's? Three checks from the recordings, with no model.

Two powders were weighed by one scale on one unit. If what arrives after a stop is the scale's lag, a stop looks
the same at the same flow in digits a second whatever the powder, and it cannot depend on the trial before it.
If powder is left at the lip, it is the tube's speed and the powder that set it, and the start after owes it.
ADR-009's `legacy/progress.md` (2026-11-21, daytime) has the checks and their pass, written before this was run.

Usage:
    python scale_or_powder.py --recordings ../recordings --first pico2w_ar2209 --second pico2w_bm2
"""

import argparse
import json
import math
import statistics
from pathlib import Path

DIGIT = 0.02
END_FROM_S, END_TO_S = 2.4, 3.0     # the reading taken as the end of a stop: every stop was watched 3 s at least
START_S = 2.0                       # a run's first seconds, for what its start owes
LATE_FROM_S = 1.5                   # a run's flow is read from this long after its start
MARKS = (0.2, 0.4, 0.6, 0.8, 1.0, 1.5, 2.0)


def at(series: list, t: float) -> float:
    """The last reading at or before t."""
    last = series[0][1]
    for sample in series:
        if sample[0] > t:
            break
        last = sample[1]
    return last


def mean_se(values: list) -> tuple:
    if len(values) < 2:
        return (values[0] if values else math.nan), math.nan
    return statistics.fmean(values), statistics.stdev(values) / math.sqrt(len(values))


def session_stops(recordings: Path, session: dict, tube: str) -> list:
    """A session's plain stops of a tube with the driver held, each with the speed of the trial before it."""
    stops = []
    for folder in sorted(path for path in recordings.iterdir() if path.is_dir()):
        if not session["from"] <= folder.name[:15] <= session["to"] or not (folder / "runs.json").exists():
            continue
        before = None
        for trial in json.loads((folder / "runs.json").read_text(encoding="utf-8")):
            speed = trial.get("speed_asked_rps")
            if trial.get("note") == "creepstop" and trial.get("tube", "fine") == tube and trial["run_s"] >= 1.9:
                kind = trial.get("stop", "held" if trial["held"] else "released")
                after, during = trial["after"], trial["during"]
                if kind == "held" and after[-1][0] >= END_TO_S - 0.1:
                    at_stop = during[-1][1]
                    end = statistics.median(w for t, w in after if END_FROM_S <= t <= END_TO_S)
                    late = [(t, w) for t, w in during if t >= -trial["run_s"] + LATE_FROM_S]
                    start = during[0][0]
                    stops.append({"speed": speed, "before": before, "folder": folder.name, "run_s": trial["run_s"],
                                  "arrived": (end - at_stop) / DIGIT,
                                  "late_gain": (late[-1][1] - late[0][1]) / DIGIT if len(late) > 1 else 0.0,
                                  "late_s": late[-1][0] - late[0][0] if len(late) > 1 else 0.0,
                                  "start_gain": (at(during, start + START_S) - during[0][1]) / DIGIT
                                  if trial["run_s"] >= START_S else None,
                                  "by": [(at(after, mark) - at_stop) / DIGIT for mark in MARKS]})
            # the speed a tube was last stopped from, whatever the kind of trial; a turn of the other tube is not one
            if trial.get("tube", "fine") == tube and speed is not None:
                before = speed
            elif trial.get("note") != "creepstop":
                before = None
    return stops


def by_speed(stops: list) -> dict:
    """Per speed: the flow before the stop, what arrives after it with its standard error, and when."""
    groups = {}
    for speed in sorted({stop["speed"] for stop in stops}):
        group = [stop for stop in stops if stop["speed"] == speed]
        arrived, se = mean_se([stop["arrived"] for stop in group])
        flow = sum(stop["late_gain"] for stop in group) / sum(stop["late_s"] for stop in group)
        groups[speed] = {"n": len(group), "flow": flow, "arrived": arrived, "se": se,
                         "sd": statistics.stdev(stop["arrived"] for stop in group) if len(group) > 1 else math.nan,
                         "by": [statistics.fmean(stop["by"][i] for stop in group) for i in range(len(MARKS))]}
    return groups


def on_line(points: list, x: float) -> tuple:
    """(value, standard error) at x on straight lines through the points (x, value, se), which begin at nothing."""
    points = [(0.0, 0.0, 0.0)] + sorted(points)
    for (x0, y0, e0), (x1, y1, e1) in zip(points, points[1:]):
        if x <= x1:
            share = (x - x0) / (x1 - x0)
            return y0 + (y1 - y0) * share, math.hypot(e0 * (1.0 - share), e1 * share)
    (x0, y0, e0), (x1, y1, e1) = points[-2], points[-1]
    return y1 + (y1 - y0) * (x - x1) / (x1 - x0), e1


def split_by_before(stops: list, key: str) -> list:
    """Per speed: the mean of `key` after a faster trial and after a slower or equal one, and their difference."""
    rows = []
    for speed in sorted({stop["speed"] for stop in stops}):
        group = [stop for stop in stops if stop["speed"] == speed and stop["before"] is not None and stop[key] is not None]
        faster = [stop[key] for stop in group if stop["before"] > speed]
        other = [stop[key] for stop in group if stop["before"] <= speed]
        if len(faster) >= 3 and len(other) >= 3:
            (fast, fast_se), (rest, rest_se) = mean_se(faster), mean_se(other)
            rows.append((speed, len(faster), fast, len(other), rest, fast - rest, math.hypot(fast_se, rest_se)))
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--recordings", type=Path, default=Path(__file__).resolve().parent.parent / "recordings")
    parser.add_argument("--first", required=True, help="the session the forecasts are made from")
    parser.add_argument("--second", required=True, help="the session they are made for")
    args = parser.parse_args()
    sessions = {s["name"]: s for s in json.loads((args.recordings / "sessions.json").read_text(encoding="utf-8"))}
    first, second = sessions[args.first], sessions[args.second]
    kernels = second["kernel_gr"] / first["kernel_gr"]

    stops = {name: session_stops(args.recordings, sessions[name], "fine") for name in (args.first, args.second)}
    groups = {name: by_speed(stops[name]) for name in stops}
    for name in stops:
        print(f"\n{name} ({sessions[name]['powder']}), fine tube, plain stops held")
        print("  rev/s   stops  flow d/s  arrived  se    sd    s of flow   arrived by " + " ".join(f"{m:g}s" for m in MARKS))
        for speed, g in groups[name].items():
            print(f"  {speed:<6g}  {g['n']:4d}  {g['flow']:7.2f}  {g['arrived']:+7.2f}  {g['se']:.2f}  {g['sd']:.2f}  "
                  f"{g['arrived'] / g['flow']:8.2f}    " + " ".join(f"{v:5.1f}" for v in g["by"]))

    print(f"\nWhat a run's start is short of its flow by its first {START_S:g} s, beside what its stop leaves, digits")
    for name in stops:
        for speed, g in groups[name].items():
            gains = [stop["start_gain"] for stop in stops[name] if stop["speed"] == speed and stop["start_gain"] is not None]
            gain, gain_se = mean_se(gains)
            print(f"  {sessions[name]['powder']:8s} {speed:<5g} rev/s: the start short by {g['flow'] * START_S - gain:+6.2f} "
                  f"(se {gain_se:.2f}), the stop leaves {g['arrived']:+6.2f} (se {g['se']:.2f})")

    coarse = [stop for name in stops for stop in session_stops(args.recordings, sessions[name], "coarse")]
    plain_s = sum(stop["arrived"] for stop in coarse) / sum(stop["late_gain"] / stop["late_s"] for stop in coarse)
    print(f"\nthe plain delay, from {len(coarse)} stops of the coarse tube in both sessions: {plain_s:.2f} s of its flow")

    print(f"\n1. At the same flow: {second['powder']}'s stops forecast from {first['powder']}'s alone, digits")
    print("  rev/s   flow d/s  measured  se   | the scale's  off   in se | same grains  off | same kernels  off")
    line = [(g["flow"], g["arrived"], g["se"]) for g in groups[args.first].values()]
    offs = {"scale": [], "grains": [], "kernels": []}
    worst = 0.0
    for speed, g in groups[args.second].items():
        scale, scale_se = on_line(line, g["flow"])
        error = math.hypot(scale_se, g["se"])
        row = f"  {speed:<6g}  {g['flow']:7.2f}  {g['arrived']:+7.2f}  {g['se']:.2f} | {scale:+9.2f}  {g['arrived'] - scale:+5.2f}  {(g['arrived'] - scale) / error:+5.1f}"
        offs["scale"].append(g["arrived"] - scale)
        if abs(g["arrived"] - scale) > 2.0 and abs(g["arrived"] - scale) > 2.0 * error:
            worst = max(worst, abs(g["arrived"] - scale))
        if speed in groups[args.first]:
            same = groups[args.first][speed]
            grains = same["arrived"] - plain_s * (same["flow"] - g["flow"])
            kernel = plain_s * g["flow"] + (same["arrived"] - plain_s * same["flow"]) * kernels
            offs["grains"].append(g["arrived"] - grains)
            offs["kernels"].append(g["arrived"] - kernel)
            row += f" | {grains:+9.2f}  {g['arrived'] - grains:+5.2f} | {kernel:+9.2f}  {g['arrived'] - kernel:+5.2f}"
        print(row)
    rms = {name: math.sqrt(statistics.fmean(off * off for off in values)) for name, values in offs.items()}
    print("  root mean square off: " + ", ".join(f"{name} {value:.2f}" for name, value in rms.items()))
    nearest = min(rms, key=rms.get)
    passed = rms["scale"] <= 1.0 and worst == 0.0 and nearest == "scale"
    print(f"  CHECK 1 {'PASS' if passed else 'FAIL'} for the scale: off {rms['scale']:.2f} digits (1 allowed), "
          f"{'none' if worst == 0.0 else f'one by {worst:.1f}'} past two digits and twice its error, nearest is {nearest}")

    for number, key, what in ((2, "arrived", "what arrives after a stop"),
                              (3, "start_gain", f"the gain over a run's first {START_S:g} s")):
        print(f"\n{number}. The trial before: {what}, after a faster trial and after a slower or equal one, digits")
        shown = False
        for name in stops:
            for speed, n_fast, fast, n_rest, rest, difference, error in split_by_before(stops[name], key):
                flag = abs(difference) > 1.0 and abs(difference) > 2.0 * error
                shown = shown or flag
                print(f"  {sessions[name]['powder']:8s} {speed:<5g} rev/s: after faster {fast:+6.2f} ({n_fast}), "
                      f"after slower or equal {rest:+6.2f} ({n_rest}), difference {difference:+5.2f} se {error:.2f}"
                      f"{'   <-- past a digit and twice its error' if flag else ''}")
        print(f"  CHECK {number} {'FAIL' if shown else 'PASS'} for the scale")


if __name__ == "__main__":
    main()
