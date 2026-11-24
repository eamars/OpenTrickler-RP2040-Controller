"""F.4's study seen whole (ADR-009): each assumed lag and rest on every stand-in scale, the first three throws of a
fresh powder at 10 gr, good, over, short and time, against a calibrated unit's on that scale. For the reader of the
study's verdict (`f4_cases.py --study`), which judges the overs alone.

    python f4_landscape.py [--sessions 40] [--lags 0.5,1,2,3] [--rests 4,6] [--out ...]
"""

import argparse
import math
import statistics
from pathlib import Path

import f4_cases
from workers import Workers


def judged(sessions: list, calibrated: list) -> tuple:
    """The study's pass on one scale (adr009_controller's progress.md, 00:55): the first throws over no more often than
    a calibrated unit's beyond the noise (two SDs of the difference of two shares, as `fallback_study.py` judges); and,
    as the uncalibrated case asks, every one of them ended good, over or short. (verdict, its words)"""
    first = [t for s in sessions for t in s[:f4_cases.FIRST]]
    base = [t for s in calibrated for t in s[:f4_cases.FIRST]]
    n = len(first)
    over = sum(t["error"] >= f4_cases.TOLERANCE_GR for t in first) / n
    over_base = sum(t["error"] >= f4_cases.TOLERANCE_GR for t in base) / len(base)
    p = max(over_base, 1.0 / n)
    bar = 2.0 * math.sqrt(2.0 * p * (1.0 - p) / n)
    ends = sum(t["result"] not in ("good", "over", "short") for t in first)
    ok = over - over_base <= bar and ends == 0
    return ok, f"over {(over - over_base) * 100:+.0f} (bar {bar * 100:.0f})" + (f", {ends} ends" if ends else "")


def summary(sessions: list) -> str:
    first = [t for s in sessions for t in s[:f4_cases.FIRST]]
    n = len(first)
    tol = f4_cases.TOLERANCE_GR
    over = sum(t["error"] >= tol for t in first) / n
    short = sum(t["error"] <= -tol for t in first) / n
    good = 1.0 - over - short
    time = statistics.median(t["elapsed"] for t in first)
    ends = sum(t["result"] not in ("good", "over", "short") for t in first)
    return f"good {good * 100:3.0f}% over {over * 100:3.0f}% short {short * 100:3.0f}% median {time:5.1f} s" + \
        (f" ENDS {ends}" if ends else "")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sessions", type=int, default=40)
    parser.add_argument("--lags", default="0.5,1,2,3")
    parser.add_argument("--rests", default="4,6")
    parser.add_argument("--target", type=float, default=f4_cases.STUDY_TARGET_GR)
    parser.add_argument("--scale-measured", action="store_true",
                        help="also the learnt source given each scale's lag and rest as its calibration measures them")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    pairs = [(float(lag), float(rest)) for lag in args.lags.split(",") for rest in args.rests.split(",")]
    jobs = [(name, "calibrated", 0.0, 0.0, args.target, seed, f4_cases.FIRST, 1.0) for name in f4_cases.names()
            for seed in range(args.sessions)]
    jobs += [(name, "learnt", lag, rest, args.target, seed, f4_cases.FIRST, 1.0) for lag, rest in pairs
             for name in f4_cases.names() for seed in range(args.sessions)]
    if args.scale_measured:
        jobs += [(name, "learnt_scale", -1.0, -1.0, args.target, seed, f4_cases.FIRST, 1.0) for name in f4_cases.names()
                 for seed in range(args.sessions)]
        pairs.append((-1.0, -1.0))
    with Workers() as pool:
        results = pool.map(f4_cases.session_job, jobs)
    lines = [f"{args.sessions} sessions a line, the first {f4_cases.FIRST} throws of each at {args.target:g} gr"]
    sessions_of = lambda key, name: [r for j, r in zip(jobs, results) if (j[1], j[2], j[3]) == key and j[0] == name]
    passing = []
    keys = [("calibrated", 0.0, 0.0)] + [("learnt_scale" if lag < 0 else "learnt", lag, rest) for lag, rest in pairs]
    for key in keys:
        all_ok = True
        for name in f4_cases.names():
            sessions = sessions_of(key, name)
            label = key[0] if key[0] != "learnt" else f"lag {key[1]:g} rest {key[2]:g}"
            verdict = ""
            if key[0] != "calibrated":
                ok, words = judged(sessions, sessions_of(("calibrated", 0.0, 0.0), name))
                all_ok = all_ok and ok
                verdict = f"  {'ok  ' if ok else 'FAIL'} {words}"
            lines.append(f"{label:16s} {name:10s} {summary(sessions)}{verdict}")
            print(lines[-1], flush=True)
        if key[0] == "learnt" and all_ok:
            passing.append(key[1:])
    shortest = min(passing) if passing else None
    lines.append(f"pairs passing on every scale: {passing or 'none'}; the shortest: "
                 f"{shortest if shortest else 'none, the finding for the owner'}")
    print(lines[-1], flush=True)
    if args.out:
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
