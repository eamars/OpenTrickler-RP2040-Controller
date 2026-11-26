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
import collections
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
LANDED_S = 1.0                  # what is in the cup this long after the coarse tube's stop: its powder has landed
CAPPED = ((0.02, 5.0, None), (0.02, 7.0, None), (1.0, 5.0, (0.5, False)), (1.0, 7.0, (0.5, False)),
          (1.0, 5.0, (0.25, False)), (1.0, 5.0, (0.5, True)), (1.0, 7.0, (0.5, True)), (0.2, 5.0, (0.5, False)))
# (coarse kp, threshold, None for today's tube or (the margin's share of the threshold, the fine tube's forecast
# counting the coarse tube's unseen powder too))
CAPPED_TARGETS = (10.0, 30.0, 60.0)
SHEET = (("a wild coarse PID: kp 0.2, threshold 5", 0.2, 5.0, None), ("more room: kp 0.2, threshold 7", 0.2, 7.0, None),
         ("the PID tuned: kp 0.02, threshold 5", 0.02, 5.0, None),
         ("capped, counted: kp 1, threshold 5, margin 2.5", 1.0, 5.0, (0.5, False)))


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


class Capped(Watched):
    """The coarse tube for speed with its landing bounded (a study, not the firmware): its kp as high as it likes, its
    speed capped so that what it will have in the air cannot take the forecast past the target less a margin, a share
    of the threshold. Its turns are counted as the fine tube's are: each turned out at the motor's speed, due on the
    scale after the lag at its flow, at step 1's coarse weight a turn; the room is the target less the margin, less the
    reading, the fine tube's unseen powder and the coarse tube's; the speed at most least + room / (weight a turn x
    lag). `counted` False leaves the coarse tube's unseen powder out (the first try: the room from the reading alone).
    `shared`: the fine tube's forecast counts the coarse tube's unseen powder too, so its PID and envelope know it."""

    def __init__(self, *args, margin_share: float = 0.5, counted: bool = True, shared: bool = False, **kwargs):
        super().__init__(*args, **kwargs)
        self.margin_share = margin_share
        self.counted = counted
        self.shared = shared

    def coarse_unseen_gr(self) -> float:
        k = self.records.coarse_gr_turn
        return k * getattr(self, "coarse_unseen", 0.0) if k else 0.0

    def forecast(self) -> float:
        return super().forecast() + (self.coarse_unseen_gr() if self.shared else 0.0)

    def start(self, target_gr: float, now_ms: float) -> None:
        super().start(target_gr, now_ms)
        self.coarse_speed = 0.0                 # the coarse motor at its ramp
        self.coarse_air = collections.deque()   # (due s, turns): turned out, not yet on the scale
        self.coarse_unseen = 0.0
        self.coarse_tick = now_ms / 1000.0

    def cap(self) -> None:
        k = self.records.coarse_gr_turn
        if not (self.coarse_moves and self.coarse_rps > 0.0 and k):
            return
        unseen = self.coarse_unseen_gr() if self.counted else 0.0
        room = self.target - self.margin_share * self.profile.coarse_stop_threshold - Watched.forecast(self) - unseen
        lag = float(self.lags(max(self.coarse_speed, self.profile.coarse_min_rps) * k))
        least = self.profile.coarse_min_rps
        self.coarse_rps = max(least, min(self.coarse_rps, least + max(0.0, room) / (k * lag)))

    def on_tick(self, now_ms: float) -> None:
        now = now_ms / 1000.0
        dt = now - self.coarse_tick
        self.coarse_tick = now
        change = self.acceleration * dt
        v = self.coarse_speed
        self.coarse_speed = min(self.coarse_rps, v + change) if v < self.coarse_rps else max(self.coarse_rps, v - change)
        if self.coarse_speed > 0.0 and self.records.coarse_gr_turn:
            turns = self.coarse_speed * dt
            flow = self.coarse_speed * self.records.coarse_gr_turn
            self.coarse_air.append((now + float(self.lags(flow)), turns))
            self.coarse_unseen += turns
        while self.coarse_air and self.coarse_air[0][0] <= now:
            self.coarse_unseen -= self.coarse_air.popleft()[1]
        super().on_tick(now_ms)
        self.cap()

    def on_reading(self, now_ms: float, weight: float) -> None:
        super().on_reading(now_ms, weight)
        self.cap()


def run(job: tuple) -> tuple:
    rig_name, kp, threshold, target, seed, records, throws, keep_trace, cap = job
    rig = rigs.get_rig(rig_name)
    profile = dataclasses.replace(BENCH, coarse_kp=kp, coarse_stop_threshold=threshold)
    session = Session(rig, seed)
    if cap is None:
        controller = Watched(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none",
                             cut="reading")
    else:
        controller = Capped(profile, records, rig.acceleration_rps2, before="last", seed=True, hold="none",
                            cut="reading", margin_share=cap[0], shared=cap[1])
    out = []
    for _ in range(throws):
        result = session.throw(controller, target, profile.fine_stop_threshold, record=True,
                               verdict_after_s=controller.verdict_after_s)
        controller.at_rest(result.reading)
        stop_s = controller.coarse_stop[0]
        speed = max((c for s, _, _, c, _ in result.rows if stop_s - LATE_S <= s < stop_s), default=float("nan"))
        landed = next((cup for s, _, cup, _, _ in result.rows if s >= stop_s + LANDED_S), result.true_gr)
        left = target - (landed if stop_s == stop_s else 0.0)
        trace = []
        if keep_trace:
            next_s = 0.0
            for t, reading, *_ in result.rows:
                if reading is not None and t >= next_s:
                    trace.append((t, reading))
                    next_s = t + STEP_S
        out.append((result.true_error, result.elapsed_s, result.verdict, stop_s, speed, result.coarse_gr,
                    result.fine_gr, trace, left))
    return (rig_name, kp, threshold, target, cap), out[-JUDGED:] if not keep_trace else out


def line(throws: list) -> str:
    done = [x for x in throws if x[2] != "timeout"]
    errors = [x[0] for x in done]
    times = sorted(x[1] for x in done)
    stops = [x[3] for x in done if x[3] == x[3]]
    speeds = [x[4] for x in done if x[4] == x[4]]
    left = min(x[8] for x in done)
    coarse = statistics.median(x[5] for x in done)
    fine = statistics.median(x[6] for x in done)
    return (f"mean {statistics.fmean(errors):+.3f}  sd {statistics.pstdev(errors):.3f}  worst {max(errors):+.2f}  "
            f"time {times[len(times) // 2]:4.1f} s  coarse ran {statistics.median(stops) if stops else 0:4.1f} s, "
            f"its fastest in its last {LATE_S:g} s {statistics.median(speeds) if speeds else 0:.2f} rev/s; poured: coarse {coarse:5.2f}, fine "
            f"{fine:5.2f}; left for the fine tube, least {left:+.2f}  timeouts {len(throws) - len(done)}")


def grid(rig_name: str, sessions: int, capped: bool, coarse_k: float = 1.0) -> None:
    """Today's coarse tube, kp by threshold; or with `capped`, today's best two against the capped tube."""
    cells = CAPPED if capped else [(kp, th, None) for kp in KPS for th in THRESHOLDS]
    targets = CAPPED_TARGETS if capped else TARGETS
    with Workers() as pool:
        records = pool.map(step2.rig_records, [rig_name])[0]
        if records.coarse_gr_turn and coarse_k != 1.0:
            records = dataclasses.replace(records, coarse_gr_turn=records.coarse_gr_turn * coarse_k)
        jobs = [(rig_name, kp, th, target, 9900 + s, records, THROWS, False, cap)
                for target in targets for kp, th, cap in cells for s in range(sessions)]
        results = pool.map(run, jobs)
    kept = {}
    for key, throws in results:
        kept.setdefault(key, []).extend(throws)
    print(f"{rig_name}: the bench's fine gains, the coarse tube as given, no tuner, {sessions} x {THROWS} throws, the "
          f"last {JUDGED} each; step 1's coarse weight a turn {records.coarse_gr_turn}")
    for target in targets:
        print(f"\n{target:g} gr")
        for kp, th, cap in cells:
            what = "today's" if cap is None else f"capped, margin {cap[0]:g}" + (", fine sees it" if cap[1] else "")
            print(f"  {what:28s} coarse kp {kp:<5g} threshold {th:<3g} {line(kept[(rig_name, kp, th, target, cap)])}")


def sheet(rig_name: str, out: str) -> None:
    with Workers() as pool:
        records = pool.map(step2.rig_records, [rig_name])[0]
        jobs = [(rig_name, kp, th, target, 9900, records, 12, True, cap) for target in TARGETS
                for _, kp, th, cap in SHEET]
        results = dict(pool.map(run, jobs))
    fig, axes = plt.subplots(len(TARGETS), len(SHEET), figsize=(6.5 * len(SHEET), 4.6 * len(TARGETS)),
                             squeeze=False)
    for row, target in zip(axes, TARGETS):
        for ax, (name, kp, th, cap) in zip(row, SHEET):
            throws = results[(rig_name, kp, th, target, cap)][-8:]
            for error, elapsed, _, stop_s, _, _, _, trace, _ in throws:
                ax.plot([t for t, _ in trace], [w for _, w in trace], linewidth=0.8)
                ax.plot([stop_s], [next((w for t, w in trace if t >= stop_s), float("nan"))], "k.", markersize=4)
                ax.plot([elapsed], [next((w for t, w in reversed(trace) if t <= elapsed), float("nan"))], "kx",
                        markersize=4)
            ax.axhline(target, color="grey", linewidth=0.6)
            ax.axhline(target - th, color="grey", linewidth=0.6, linestyle=":")
            ax.set_ylim(0.0, max(target + 3.0, max(w for x in throws for _, w in x[7]) + 1.0))
            errors = [x[0] for x in throws]
            times = [x[1] for x in throws]
            ax.set_title(f"{target:g} gr, {name}\nmean {statistics.fmean(errors):+.2f}, sd "
                         f"{statistics.pstdev(errors):.3f} gr, {statistics.median(times):.1f} s", fontsize=10)
            ax.set_xlabel("s from the throw's start (dot: the coarse stop; x: the throw's stop)")
        row[0].set_ylabel("reading, gr (dotted: the coarse stop point)")
    fig.suptitle(f"The coarse tube on the twin ({rig_name}, the bench's fine gains, no tuner)")
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    plt.close(fig)
    print(f"written {out}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rig", default="pico2w_ar2209_f2")
    parser.add_argument("--sessions", type=int, default=2)
    parser.add_argument("--sheet", help="draw the sheet into this PNG instead of the grid")
    parser.add_argument("--capped", action="store_true", help="the capped coarse tube against today's best")
    parser.add_argument("--coarse-k", type=float, default=1.0,
                        help="step 1's coarse weight a turn taken as this many times what it measured (a wrong seed)")
    args = parser.parse_args()
    if args.sheet:
        sheet(args.rig, args.sheet)
    else:
        grid(args.rig, args.sessions, args.capped, args.coarse_k)


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).parent))
    main()
