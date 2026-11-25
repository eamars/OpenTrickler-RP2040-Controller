"""ADR-009 step 2: the bench's A/B of 2026-11-26 beside the twin's (the owner: "capture as much data as you can, then
go back to the simulation").

The bench threw real 10 gr throws one a session, the cup never lifted, kp 2 (the stored baseline) and kp 1.33 (the
tuner's first trial from an over-heavy window) in ABBA order, the seed at the bench's own weight a turn, the tuner off.
This throws the same on each rig of the twin (each with its own step 1 records and seed) and reads the bench's logs,
and prints both by arm: the final weight's mean and SD, the shares good, over and under, the median time to the stop,
the share of throws that gave their own weight a turn to the carry, and what landed after the stop (the final weight
less the reading at the stop: what was in the air, or still poured unseen, when the controller stopped).

    venv\\Scripts\\python tools\\charge_sim\\step2_bench_ab.py [--logs logs\\rc\\<session>.log ...] [--throws 40]
"""

import argparse
import dataclasses
import re
import statistics
from datetime import datetime
from pathlib import Path

import rigs
import step2
from step2_variants import BENCH
from throw import Session
from workers import Workers

KP = {"A": 2.0, "B": 1.3333}
TARGET = 10.0
ORDER = "ABBA"
START = re.compile(r"^(\S+) unit +Charge: throw (\d+) on fine kp ([\d.]+), weight a turn (carried|its own, none yet) "
                   r"([\d.]+)")
AT_REST = re.compile(r"^(\S+) unit +Charge: throw (\d+) at rest, (-?[\d.]+) of ([\d.]+) in ([\d.]+) s")
CARRIED = re.compile(r"Charge: carried weight a turn ([\d.]+)")
FRAME = re.compile(r"^(\S+) unit +@sf \d+ (-?[\d.]+) ")


@dataclasses.dataclass
class Throw:
    arm: str
    error: float
    time_s: float
    renewed: bool               # gave its own weight a turn to the carry
    after_stop: float           # the final weight less the reading at the stop; nan where unknown


def twin_case(job: tuple) -> list:
    rig_name, throws, seed, records = job
    rig = rigs.get_rig(rig_name)
    session = Session(rig, seed)
    controller = step2.Step2(BENCH, records, rig.acceleration_rps2, before="last", seed=True, hold="none")
    out = []
    for n in range(throws):
        arm = ORDER[n % len(ORDER)]
        controller.profile = dataclasses.replace(BENCH, fine_kp=KP[arm])
        history = list(controller.history)
        # step2.throw_one, its rows kept
        result = session.throw(controller, TARGET, BENCH.fine_stop_threshold, record=True,
                               verdict_after_s=controller.verdict_after_s)
        controller.at_rest(result.reading)
        shown = [row[1] for row in result.rows if row[0] <= result.elapsed_s and row[1] is not None]
        at_stop = shown[-1] if shown else float("nan")
        out.append(Throw(arm, result.reading - TARGET, result.elapsed_s, controller.history != history,
                         result.reading - at_stop))
    return out


def host_s(stamp: str) -> float:
    return (datetime.strptime(stamp, "%H:%M:%S.%f") - datetime(1900, 1, 1)).total_seconds()


def bench_throws(paths: list) -> list:
    """Each throw at rest in the logs, its arm by its kp, what landed after its stop from the scale's frames."""
    out = []
    for path in paths:
        lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
        frames = [(host_s(m[1]), float(m[2])) for m in map(FRAME.search, lines) if m]
        started, carried_before = None, None
        for text in lines:
            if m := START.search(text):
                started = (host_s(m[1]), float(m[3]))
                carried_before = float(m[5]) if m[4] == "carried" else None
            elif (m := AT_REST.search(text)) and started:
                stop_s = started[0] + float(m[5])
                shown = [w for t, w in frames if t <= stop_s]
                at_stop = shown[-1] if shown else float("nan")
                final = float(m[3])
                arm = min(KP, key=lambda a: abs(KP[a] - started[1]))
                out.append([arm, final - float(m[4]), float(m[5]), carried_before, final - at_stop])
            elif (m := CARRIED.search(text)) and out and out[-1][3] is not None and len(out[-1]) == 5:
                out[-1].append(abs(float(m[1]) - out[-1][3]) > 1e-4)
    return [Throw(a, e, t, len(rest) > 0 and rest[0], s) for a, e, t, _, s, *rest in out]


def line(throws: list, tolerance: float) -> str:
    errors = [t.error for t in throws]
    good = sum(abs(e) < tolerance for e in errors) / len(errors)
    over = sum(e >= tolerance for e in errors) / len(errors)
    under = sum(e <= -tolerance for e in errors) / len(errors)
    after = [t.after_stop for t in throws if t.after_stop == t.after_stop]
    return (f"n {len(errors):3d}  mean {statistics.fmean(errors):+.3f}  sd {statistics.pstdev(errors):.3f}  "
            f"good {good:4.0%}  over {over:4.0%}  under {under:4.0%}  time {statistics.median(t.time_s for t in throws):4.2f} s"
            f"  own k {sum(t.renewed for t in throws) / len(throws):4.0%}"
            f"  after the stop {statistics.median(after) if after else float('nan'):+.3f}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--logs", nargs="*", default=[], help="the bench's session logs (logs/rc/*_send.log)")
    parser.add_argument("--rigs", nargs="+", default=list(step2.DEFAULT_RIGS))
    parser.add_argument("--throws", type=int, default=40, help="throws a session on the twin")
    parser.add_argument("--sessions", type=int, default=4)
    args = parser.parse_args()
    tolerance = BENCH.fine_stop_threshold
    if args.logs:
        throws = bench_throws(args.logs)
        print("the bench (the Pico 2 W FA0C, AR2209, the A&D FX-300i):")
        for arm in KP:
            mine = [t for t in throws if t.arm == arm]
            if mine:
                print(f"  kp {KP[arm]:<6g} {line(mine, tolerance)}")
    with Workers() as pool:
        made = dict(zip(args.rigs, pool.map(step2.rig_records, list(args.rigs))))
        jobs = [(rig, args.throws, 9300 + s, made[rig]) for rig in args.rigs for s in range(args.sessions)]
        results = pool.map(twin_case, jobs)
    for rig in args.rigs:
        throws = [t for (r, *_), batch in zip(jobs, results) if r == rig for t in batch]
        print(f"{rig} (its seed {made[rig].fine_gr_turn:.3f}), {args.sessions} sessions of {args.throws}:")
        for arm in KP:
            print(f"  kp {KP[arm]:<6g} {line([t for t in throws if t.arm == arm], tolerance)}")


if __name__ == "__main__":
    main()
