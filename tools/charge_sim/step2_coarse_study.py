"""ADR-009 step 2: the coarse tube's two knobs on the twin, its stop threshold against its kp (2026-11-26).

The owner: "I don't understand your conclusion on coarse trickler decision, can you explain a bit more to me, on which
method works better, i.e,. between raising the stop threshold versus better tune of PID."

The coarse tube runs today's PID on the reading's error to its stop point (the target less the threshold), at no less
than its least speed, and stops when the reading reaches it. Each cell is step 2 as it ships with the tuner off (the
fine tube's gains the bench's), on a rig, from the bench's gains with the coarse kp and the threshold of the cell, at a
target. Each prints the throws' mean, SD and worst, the median time, how long the coarse tube ran, its speed at its
last half second (the motor's, its powder still in the air at the stop), and what each tube poured (medians).

    venv\\Scripts\\python tools\\charge_sim\\step2_coarse_study.py [--rig pico2w_ar2209_f2] [--sessions 2]
    venv\\Scripts\\python tools\\charge_sim\\step2_coarse_study.py --sheet <png>
"""

import argparse
import dataclasses
import statistics
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

import rigs  # noqa: E402
import step2  # noqa: E402
from step2_variants import BENCH  # noqa: E402
from throw import Session  # noqa: E402
from workers import Workers  # noqa: E402

KPS = (0.02, 0.05, 0.1, 0.2)
THRESHOLDS = (3.0, 5.0, 7.0, 9.0)
TARGETS = (10.0, 30.0)
THROWS = 40
JUDGED = 35                     # the last throws of each session (the forecast's carry has settled)
STEP_S = 0.02                   # a sheet's trace kept every 20 ms
LATE_S = 0.5                    # the coarse tube's last half second before its stop: its powder still in the air
SHEET = (("a wild coarse PID: kp 0.2, threshold 5", 0.2, 5.0), ("more room: kp 0.2, threshold 7", 0.2, 7.0),
         ("the PID tuned: kp 0.02, threshold 5", 0.02, 5.0))


class Watched(step2.Step2):
    """Step 2 as it ships, its coarse tube's stop kept: (its s, the speed it was asked for then, the reading then)."""

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.coarse_stop = (float("nan"), float("nan"), float("nan"))

    def on_reading(self, now_ms: float, weight: float) -> None:
        moving, speed = self.coarse_moves, self.coarse_rps
        super().on_reading(now_ms, weight)
        if moving and not self.coarse_moves:
            self.coarse_stop = (now_ms / 1000.0, speed, weight)


def run(job: tuple) -> tuple:
    rig_name, kp, threshold, target, seed, records, throws, keep_trace = job
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(BENCH, coarse_kp=kp, coarse_stop_threshold=threshold)
    session = Session(rig, seed)
    controller = Watched(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none",
                         cut="reading")
    out = []
    for _ in range(throws):
        result = session.throw(controller, target, profile.fine_stop_threshold, record=True,
                               verdict_after_s=controller.verdict_after_s)
        controller.at_rest(result.reading)
        stop_s = controller.coarse_stop[0]
        speed = max((c for s, _, _, c, _ in result.rows if stop_s - LATE_S <= s < stop_s), default=float("nan"))
        trace = []
        if keep_trace:
            next_s = 0.0
            for t, reading, *_ in result.rows:
                if reading is not None and t >= next_s:
                    trace.append((t, reading))
                    next_s = t + STEP_S
        out.append((result.true_error, result.elapsed_s, result.verdict, stop_s, speed, result.coarse_gr,
                    result.fine_gr, trace))
    return (rig_name, kp, threshold, target), out[-JUDGED:] if not keep_trace else out


def line(throws: list) -> str:
    done = [x for x in throws if x[2] != "timeout"]
    errors = [x[0] for x in done]
    times = sorted(x[1] for x in done)
    stops = [x[3] for x in done if x[3] == x[3]]
    speeds = [x[4] for x in done if x[4] == x[4]]
    coarse = statistics.median(x[5] for x in done)
    fine = statistics.median(x[6] for x in done)
    return (f"mean {statistics.fmean(errors):+.3f}  sd {statistics.pstdev(errors):.3f}  worst {max(errors):+.2f}  "
            f"time {times[len(times) // 2]:4.1f} s  coarse ran {statistics.median(stops) if stops else 0:4.1f} s, "
            f"its fastest in its last {LATE_S:g} s {statistics.median(speeds) if speeds else 0:.2f} rev/s; poured: coarse {coarse:5.2f}, fine "
            f"{fine:5.2f}  timeouts {len(throws) - len(done)}")


def grid(rig_name: str, sessions: int) -> None:
    with Workers() as pool:
        records = pool.map(step2.rig_records, [rig_name])[0]
        jobs = [(rig_name, kp, th, target, 9900 + s, records, THROWS, False)
                for target in TARGETS for kp in KPS for th in THRESHOLDS for s in range(sessions)]
        results = pool.map(run, jobs)
    cells = {}
    for key, throws in results:
        cells.setdefault(key, []).extend(throws)
    print(f"{rig_name}: the bench's gains, the coarse kp and threshold as given, no tuner, {sessions} x {THROWS} "
          f"throws, the last {JUDGED} each")
    for target in TARGETS:
        print(f"\n{target:g} gr")
        for kp in KPS:
            for th in THRESHOLDS:
                print(f"  coarse kp {kp:<5g} threshold {th:<3g} {line(cells[(rig_name, kp, th, target)])}")


def sheet(rig_name: str, out: str) -> None:
    with Workers() as pool:
        records = pool.map(step2.rig_records, [rig_name])[0]
        jobs = [(rig_name, kp, th, target, 9900, records, 12, True) for target in TARGETS for _, kp, th in SHEET]
        results = dict(pool.map(run, jobs))
    fig, axes = plt.subplots(len(TARGETS), len(SHEET), figsize=(6.5 * len(SHEET), 4.6 * len(TARGETS)),
                             squeeze=False)
    for row, target in zip(axes, TARGETS):
        for ax, (name, kp, th) in zip(row, SHEET):
            throws = results[(rig_name, kp, th, target)][-8:]
            for error, elapsed, _, stop_s, _, _, _, trace in throws:
                ax.plot([t for t, _ in trace], [w for _, w in trace], linewidth=0.8)
                ax.plot([stop_s], [next((w for t, w in trace if t >= stop_s), float("nan"))], "k.", markersize=4)
                ax.plot([elapsed], [next((w for t, w in reversed(trace) if t <= elapsed), float("nan"))], "kx",
                        markersize=4)
            ax.axhline(target, color="grey", linewidth=0.6)
            ax.axhline(target - th, color="grey", linewidth=0.6, linestyle=":")
            ax.set_ylim(0.0, max(target + 3.0, max(w for x in throws for _, w in x[-1]) + 1.0))
            errors = [x[0] for x in throws]
            times = [x[1] for x in throws]
            ax.set_title(f"{target:g} gr, {name}\nmean {statistics.fmean(errors):+.2f}, sd "
                         f"{statistics.pstdev(errors):.3f} gr, {statistics.median(times):.1f} s", fontsize=10)
            ax.set_xlabel("s from the throw's start (dot: the coarse stop; x: the throw's stop)")
        row[0].set_ylabel("reading, gr (dotted: the coarse stop point)")
    fig.suptitle(f"The coarse tube's two knobs on the twin ({rig_name}, the bench's fine gains, no tuner)")
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)
    print(f"written {out}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="pico2w_ar2209_f2")
    parser.add_argument("--sessions", type=int, default=2)
    parser.add_argument("--sheet", help="draw the sheet into this PNG instead of the grid")
    args = parser.parse_args()
    if args.sheet:
        sheet(args.rig, args.sheet)
    else:
        grid(args.rig, args.sessions)


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).parent))
    main()
