"""B3's study (ADR-009, gate 3; the manager's yes, 2026-11-24): the grains a turn by speed against one figure.

A calibrated unit on every stand-in scale, and on the record whose fine tube pours by speed (`+fine_by_speed`, the
11-22 finding: 0.68 gr a turn at the finish, 0.83 at 1 rev/s): 6 sessions of 20 throws at 10 gr, the PID law, with
the forecast reading each stop's own figure by speed, and with the one figure. Pass, written first (progress.md):
by speed no worse on every scale (good no more than 2 a session fewer, over no more than 2 a session more).

    python b3_study.py [--sessions 6] [--throws 20]
"""

import argparse
import os
import subprocess
import sys
import json

import f4_cases
import rigs
import staged
from throw import Session
from workers import Workers

TARGET_GR = 10.0


def rig_of(name):
    if name == "fine_by_speed":
        return rigs.get_rig(f4_cases.RECORD + "+fine_by_speed")
    return f4_cases.stand_in(name)


def job(args):
    name, seed, count = args
    rig = rig_of(name)
    band = f4_cases.TOLERANCE_GR - rig.scale.digit_gr / 2.0
    controller = staged.Staged(staged.calibrate(rig, seed, "glide", band), rig.acceleration_rps2)
    controller.pid = staged.profile_pid()
    session = Session(rig, seed + 400)
    good = over = 0
    for _ in range(count):
        r = session.throw(controller, TARGET_GR, f4_cases.TOLERANCE_GR)
        good += abs(r.true_error) < f4_cases.TOLERANCE_GR
        over += r.true_error >= f4_cases.TOLERANCE_GR
    return good, over


def run(sessions, throws):
    names = f4_cases.names() + ["fine_by_speed"]
    jobs = [(name, seed, throws) for name in names for seed in range(sessions)]
    with Workers() as pool:
        results = pool.map(job, jobs)
    out = {}
    for name in names:
        rows = [r for j, r in zip(jobs, results) if j[0] == name]
        out[name] = (sum(r[0] for r in rows), sum(r[1] for r in rows))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sessions", type=int, default=6)
    parser.add_argument("--throws", type=int, default=20)
    parser.add_argument("--one", action="store_true", help="(inner) run one arm and print it as JSON")
    args = parser.parse_args()
    if args.one:
        print(json.dumps(run(args.sessions, args.throws)))
        return 0
    arms = {}
    for label, value in (("by speed", "1"), ("one figure", "0")):
        env = dict(os.environ, ADR009_FIGURE_BY_SPEED=value)
        reply = subprocess.run([sys.executable, __file__, "--one", "--sessions", str(args.sessions), "--throws",
                                str(args.throws)], env=env, capture_output=True, text=True, check=True)
        arms[label] = json.loads(reply.stdout.strip().splitlines()[-1])
    failing = []
    for name in arms["by speed"]:
        (g1, o1), (g0, o0) = arms["by speed"][name], arms["one figure"][name]
        ok = g1 >= g0 - 2 * args.sessions and o1 <= o0 + 2 * args.sessions
        if not ok:
            failing.append(name)
        print(f"{name:14s} by speed good {g1} over {o1}; one figure good {g0} over {o0} of {args.sessions * args.throws}: "
              f"{'ok' if ok else 'WORSE'}", flush=True)
    print("VERDICT PASS" if not failing else f"VERDICT FAIL on {failing}")
    return 0 if not failing else 1


if __name__ == "__main__":
    sys.exit(main())
