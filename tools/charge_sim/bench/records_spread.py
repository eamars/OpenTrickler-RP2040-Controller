"""The calibration's records measured again from every kept session of one unit, powder and scale (ADR-009 step 1).

No tube is turned: it reads the recordings (`../recordings`), each folder's session from `sessions.json`, and gives by
session what the calibration's parts would record, so their spread from day to day is seen without the bench:
- the settle time of small fine moves (from the reading's first move to its rest), with each move's run length, as
  a move whose powder leaves over a longer run comes to rest later;
- the lag by flow, from steady runs of a second or more;
- each tube's slow speeds' pour rate as a share of its stream's, the least speed's test.

Usage, with the repository's venv, from this folder:
    python records_spread.py [--powder AR2209]
"""

import argparse
import json
import statistics
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import calibration as cal  # noqa: E402  (the simulation's folder, put on the path just above)
from staged import behind  # noqa: E402

RECORDINGS = HERE.parent / "recordings"
LEAST_WATCH_S = 1.8             # a run watched less than this is left out: its rest is not known
SMALL_MOVE_TURNS = 0.3          # a move of this or less, run 1.2 s or less, is a small move
SMALL_MOVE_RUN_S = 1.2
STEADY_RUN_S = 1.0              # a run this long or longer is steady enough for the lag
SLOW_RPS = {"fine": 0.12, "coarse": 0.2}            # the slow speeds whose pour rate is compared
STREAM_RPS = {"fine": (0.4, 1.01), "coarse": (0.9, 1.01)}   # where each tube's stream is taken


def kept_runs(powder: str):
    """Every kept run of the powder's sessions, as (session, run), the runs held and watched long enough."""
    sessions = json.loads((RECORDINGS / "sessions.json").read_text(encoding="utf-8"))
    for folder in sorted(RECORDINGS.iterdir()):
        if not (folder / "runs.json").exists():
            continue
        session = next((s for s in sessions if s["from"] <= folder.name[:15] <= s["to"]), None)
        if session is None or session["powder"] != powder:
            continue
        for run in json.loads((folder / "runs.json").read_text(encoding="utf-8")):
            if (isinstance(run, dict) and run.get("tube") in ("fine", "coarse") and run.get("after")
                    and run.get("note") in ("creepstop", None) and run.get("stop", "held") == "held"
                    and run["after"][-1][0] >= LEAST_WATCH_S and run.get("speed_asked_rps", 0.0) > 0.0):
                yield session["name"], run


def main() -> int:
    """
    Print the settle time by move length, the lag by flow and the slow pour rates, each by session.

    Returns:
        The exit code.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--powder", default="AR2209")
    args = parser.parse_args()
    moves, lags, pours = {}, {}, {}
    for session, run in kept_runs(args.powder):
        rest, first, settle = cal.settled(run["after"], run["after"][-1][0])
        before = run["during"][0][1]
        gain = rest - before
        speed, run_s = run["speed_asked_rps"], run.get("run_s", 0.0)
        turns = run.get("turns", speed * run_s)
        if turns > 0.0:
            pours.setdefault((run["tube"], session, round(speed, 3)), []).append((gain, turns))
        still_during = run["during"][-1][1] == run["during"][0][1]
        if (run["tube"] == "fine" and turns <= SMALL_MOVE_TURNS and run_s <= SMALL_MOVE_RUN_S and still_during
                and first is not None and settle > 0.0 and gain > 0.0):
            moves.setdefault(round(run_s, 1), []).append(settle - first)
        if run_s >= STEADY_RUN_S and gain > 0.05:
            lag = behind(run, before, rest, settle or run["after"][-1][0]) / gain
            lags.setdefault((run["tube"], round(speed, 2), session), []).append((gain / run_s, lag))

    print("The settle time of small fine moves (from the first move to rest), by the move's run length")
    for run_s, values in sorted(moves.items()):
        if len(values) >= 3:
            print(f"  run {run_s:.1f} s: {len(values):3d} moves, median {statistics.median(values):.2f} s")

    print("The lag by flow, steady runs: each session's mean and spread at a speed")
    by_speed = {}
    for (tube, speed, session), rows in lags.items():
        if len(rows) >= 2:
            by_speed.setdefault((tube, speed), []).append(
                (session, statistics.fmean(f for f, _ in rows), statistics.fmean(x for _, x in rows),
                 statistics.stdev(x for _, x in rows), len(rows)))
    for (tube, speed), rows in sorted(by_speed.items()):
        means = [lag for _, _, lag, _, _ in rows]
        print(f"  {tube:6s} {speed:5.2f} rev/s, {statistics.fmean(f for _, f, _, _, _ in rows):6.3f} gr/s: sessions "
              f"{min(means):.2f} to {max(means):.2f} s; within one, up to {max(sd for *_, sd, _ in rows):.2f} s "
              f"({len(rows)} sessions, {sum(n for *_, n in rows)} runs)")

    print("The slow speeds' pour rate, as a share of the tube's stream (the least's test passes at 0.5)")
    for tube in ("fine", "coarse"):
        for session in sorted({s for t, s, _ in pours if t == tube}):
            stream_rows = [row for (t, s, v), rows in pours.items() if t == tube and s == session
                           and STREAM_RPS[tube][0] <= v <= STREAM_RPS[tube][1] for row in rows]
            slow = sorted((v, rows) for (t, s, v), rows in pours.items()
                          if t == tube and s == session and v <= SLOW_RPS[tube] and len(rows) >= cal.SPEED_STOPS)
            if not stream_rows or not slow:
                continue
            stream = sum(g for g, _ in stream_rows) / sum(n for _, n in stream_rows)
            shares = "; ".join(f"{v} rev/s {cal.pour_rate([g for g, _ in rows], [n for _, n in rows]) / stream:.2f}"
                               for v, rows in slow)
            print(f"  {tube:6s} {session:26s} stream {stream:6.3f} gr/turn: {shares}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
