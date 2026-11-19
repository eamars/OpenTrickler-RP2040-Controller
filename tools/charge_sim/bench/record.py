"""Open-loop recordings on a real unit through the debug probe, for ADR-009's bench session (bench_guide.md).

Each run turns one tube at a speed for a time with the scale's tap on, and keeps the A&D's frames and the motor's
step counts, both on the unit's own clock. Uses the console only (`motor`, `scale tap`): no firmware change.

Usage, with the repository's venv, the unit on the probe and the bench rules kept:
    python record.py --repo C:\\workspace\\OpenTrickler-RP2040-Controller chain
    python record.py --repo ... nudges --count 100 --pause-ms 40 --still-s 1.5
    python record.py --repo ... runs --tube fine --speeds 0.1,0.5,2 --repeats 3 --limit-gr 5 --limit-s 60
    python record.py --repo ... steps --tube fine --plan 0.1:60,2:20
Output: <repo>/logs/adr009/<time>_<name>/frames.csv (t_ms, weight, header) and runs.json (one entry a run).
"""

import argparse
import datetime
import json
import random
import re
import statistics
import sys
import time
from pathlib import Path

MICROSTEPS_PER_MOTOR_TURN = 200 * 256
# Tube turns per motor turn, from the teeth: 40 on the motor, 32 on the large tube, 22 on the low-flow small one.
# The unit's stored ratio is not used: the Pico 2 W's fine one was a generation-1 figure on 2026-11-19.
TUBE_PER_MOTOR = {"coarse": 40.0 / 32.0, "fine": 40.0 / 22.0}
CUP_LIMIT_GR = 250.0
PRECONDITION_RPS = 1.0


class Bench:
    """A session on the unit: commands, the tap's frames, and the runs made."""

    def __init__(self, repo: Path, name: str, board: str):
        sys.path.insert(0, str(repo / "tools" / "remote_control"))
        import rc_run   # the checkout's own runner, of the firmware's time: found only once the repo is known
        self.session = rc_run.Session(argparse.Namespace(board=board, elf=None), f"adr009_{name}")
        stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.folder = repo / "logs" / "adr009" / f"{stamp}_{name}"
        self.folder.mkdir(parents=True)
        self.runs = []
        self.stored_ratio = {}
        for tube in ("coarse", "fine"):
            self.session.send(f"setting settings/motors/{tube}")
            for line in self.session.console.data_lines:
                match = re.match(r"@setting gear_ratio=([\d.]+)", line)
                if match:
                    self.stored_ratio[tube] = float(match.group(1))
        self.session.send("scale tap on")

    def motor_stats(self) -> dict:
        """The unit's clock and the steps each motor has made."""
        reply = self.session.send("motor")
        fields = dict(re.findall(r"(\w+)=([\d.]+)", reply))
        return {"t_ms": int(fields["t_us"]) / 1000.0, "coarse": int(fields["coarse_made"]),
                "fine": int(fields["fine_made"])}

    def weight(self) -> float:
        """The scale's reading now, by the console's state."""
        reply = self.session.send("state")
        return float(re.search(r"weight=(-?[\d.]+|nan|inf)", reply).group(1))

    def turn(self, tube: str, speed_rps: float, run_s: float, still_s: float, note: str = "",
             limit_gr: float = None) -> dict:
        """
        One run: the tube at a speed for a time (or until a weight has fallen), then still.

        Args:
            tube: coarse or fine.
            speed_rps: the speed asked of the unit, in its own rev/s (its stored gear ratio applies).
            run_s: how long to turn.
            still_s: how long to stay still after, for the scale and the late kernels.
            note: kept with the run.
            limit_gr: stop early once the reading has risen by this much.

        Returns:
            The run's facts: clock and steps before, at the stop and after, and the true turns of the tube.
        """
        before = self.motor_stats()
        start_weight = self.weight() if limit_gr is not None else None
        self.session.send(f"motor {tube} {speed_rps:g}")
        started = time.monotonic()
        if limit_gr is None:
            time.sleep(run_s)
        else:
            while time.monotonic() - started < run_s:
                time.sleep(0.2)
                if self.weight() - start_weight >= limit_gr:
                    break
        at_stop = self.motor_stats()
        self.session.send(f"motor {tube} 0")
        stopped = self.motor_stats()
        time.sleep(still_s)
        after = self.motor_stats()
        steps = after[tube] - before[tube]
        run = {"tube": tube, "speed_asked_rps": speed_rps, "note": note,
               "t_before_ms": before["t_ms"], "t_stop_asked_ms": at_stop["t_ms"], "t_stopped_ms": stopped["t_ms"],
               "t_after_ms": after["t_ms"], "steps": steps, "steps_at_stop_ask": at_stop[tube] - before[tube],
               "turns": steps / MICROSTEPS_PER_MOTOR_TURN * TUBE_PER_MOTOR[tube],
               "stored_gear_ratio": self.stored_ratio.get(tube)}
        self.runs.append(run)
        return run

    def precondition(self, tube: str, seconds: float) -> None:
        """The same state before every run: a time at 1 rev/s, then 5 s still. 0 for none."""
        if seconds > 0.0:
            self.turn(tube, PRECONDITION_RPS, seconds, 5.0, note="precondition")

    def close(self) -> int:
        """Stop both motors, save the frames and the runs, end the session with its verdict."""
        for tube in ("coarse", "fine"):
            self.session.send(f"motor {tube} 0")
        self.session.send("scale tap off")
        time.sleep(0.3)
        with open(self.folder / "frames.csv", "w", encoding="utf-8") as out:
            out.write("t_ms,weight,header\n")
            for _, line in self.session.console.tap_lines:
                parts = line.split()
                header = bytes.fromhex(parts[3][:4]).decode(errors="replace") if len(parts) > 3 else ""
                out.write(f"{parts[1]},{parts[2]},{header}\n")
        (self.folder / "runs.json").write_text(json.dumps(self.runs, indent=1), encoding="utf-8")
        print(f"saved {len(self.session.console.tap_lines)} frames and {len(self.runs)} runs to {self.folder}")
        return self.session.finish()


def frames_between(folder: Path, start_ms: float, end_ms: float) -> list:
    """Weights of the saved frames in a span of the unit's clock."""
    out = []
    with open(folder / "frames.csv", encoding="utf-8") as lines:
        next(lines)
        for line in lines:
            t_ms, weight, _ = line.rstrip("\n").split(",")
            if start_ms <= float(t_ms) <= end_ms and weight not in ("nan", "inf"):
                out.append(float(weight))
    return out


def summarise(folder: Path) -> None:
    """Per run that is not a precondition: turns, the weight gained by the settled end, what showed after the stop."""
    runs = json.loads((folder / "runs.json").read_text(encoding="utf-8"))
    print("tube speed_asked turns run_s gained_gr gr_per_turn gr_per_s late_gr note")
    for run in runs:
        if run["note"] == "precondition":
            continue
        before = frames_between(folder, run["t_before_ms"] - 600.0, run["t_before_ms"])
        at_stop = frames_between(folder, run["t_stop_asked_ms"] - 120.0, run["t_stop_asked_ms"])
        after = frames_between(folder, run["t_after_ms"] - 700.0, run["t_after_ms"])
        if not before or not after or not at_stop:
            print(run["tube"], run["speed_asked_rps"], "no frames")
            continue
        gained = statistics.fmean(after) - statistics.fmean(before)
        run_s = (run["t_stop_asked_ms"] - run["t_before_ms"]) / 1000.0
        late = statistics.fmean(after) - at_stop[-1]
        per_turn = gained / run["turns"] if run["turns"] else float("nan")
        print(f"{run['tube']} {run['speed_asked_rps']:g} {run['turns']:.4f} {run_s:.2f} {gained:+.3f} {per_turn:.3f} "
              f"{gained / run_s:.3f} {late:+.3f} {run['note']}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", required=True, type=Path, help="the checkout whose rc_run.py and build match the unit")
    parser.add_argument("--board", default="pico2_w")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("chain", help="five short runs of the fine tube: the recording chain's proof")
    nudges = sub.add_parser("nudges", help="small turns of the fine tube, each followed by a pause")
    nudges.add_argument("--count", type=int, default=100)
    nudges.add_argument("--pause-ms", type=float, default=40.0)
    nudges.add_argument("--speed", type=float, default=0.5)
    nudges.add_argument("--still-s", type=float, default=1.5)
    runs = sub.add_parser("runs", help="a tube at each of several speeds, with a stop and a settle after each")
    runs.add_argument("--tube", required=True, choices=("coarse", "fine"))
    runs.add_argument("--speeds", required=True, help="comma-separated rev/s, as asked of the unit")
    runs.add_argument("--repeats", type=int, default=3)
    runs.add_argument("--limit-gr", type=float, default=5.0)
    runs.add_argument("--limit-s", type=float, default=60.0)
    runs.add_argument("--still-s", type=float, default=5.0)
    runs.add_argument("--precondition-s", type=float, default=5.0)
    steps = sub.add_parser("steps", help="speeds one after another with no stop between: speed:seconds,...")
    steps.add_argument("--tube", required=True, choices=("coarse", "fine"))
    steps.add_argument("--plan", required=True)
    show = sub.add_parser("show", help="sum up a saved folder")
    show.add_argument("folder", type=Path)
    args = parser.parse_args()

    if args.command == "show":
        summarise(args.folder)
        return

    bench = Bench(args.repo, args.command, args.board)
    try:
        start = bench.weight()
        print(f"weight at the start {start:.2f}")
        if start > CUP_LIMIT_GR:
            raise SystemExit("the cup is near full: ask the owner to empty it")
        if args.command == "chain":
            for _ in range(5):
                bench.turn("fine", 0.5, 2.0, 3.0, note="chain")
        elif args.command == "nudges":
            for index in range(args.count):
                bench.turn("fine", args.speed, args.pause_ms / 1000.0, args.still_s, note=f"nudge {index}")
        elif args.command == "runs":
            plan = [float(text) for text in args.speeds.split(",")] * args.repeats
            random.Random(len(plan)).shuffle(plan)
            for speed in plan:
                if bench.weight() > CUP_LIMIT_GR:
                    print("stopping: the cup is near full")
                    break
                bench.precondition(args.tube, args.precondition_s)
                bench.turn(args.tube, speed, args.limit_s, args.still_s, note="map", limit_gr=args.limit_gr)
        elif args.command == "steps":
            bench.precondition(args.tube, 5.0)
            first = bench.motor_stats()
            for part in args.plan.split(","):
                speed, seconds = (float(text) for text in part.split(":"))
                bench.session.send(f"motor {args.tube} {speed:g}")
                mark = bench.motor_stats()
                bench.runs.append({"tube": args.tube, "speed_asked_rps": speed, "note": "step", "t_ms": mark["t_ms"],
                                   "steps_so_far": mark[args.tube] - first[args.tube]})
                time.sleep(seconds)
            bench.session.send(f"motor {args.tube} 0")
            time.sleep(5.0)
            last = bench.motor_stats()
            bench.runs.append({"tube": args.tube, "speed_asked_rps": 0.0, "note": "step end", "t_ms": last["t_ms"],
                               "steps_so_far": last[args.tube] - first[args.tube]})
        print(f"weight at the end {bench.weight():.2f}")
    finally:
        code = bench.close()
    if args.command in ("chain", "nudges", "runs"):
        summarise(bench.folder)
    sys.exit(code)


if __name__ == "__main__":
    main()
