"""The features a user has, in the frame (ADR-009, gate F.3): the reference.

Each case throws on the rig of record (`records/pico2w_ar2209_night.json`, its coarse tube given the bench's reverse
spill of 0.83 gr where a case needs one), the reference (`staged.Staged`) driving the rig. A case passes when its
pass, written in `progress.md` before the build (2026-11-24, 00:20 and 00:22, on branch adr009_controller), holds. On
that branch the firmware's C threw in lockstep beside the reference; the restart of ADR-009 left the C out:

- backoff_unmeasured: the back-off on (0.25 turn at 1 rev/s) and no spill measured: the coarse room is the coarse
  stop threshold's (5 gr), and the rules say so (`backoff_fallback`);
- backoff_changed: a spill measured at 0.25 turn, the back-off set to 0.5: the same;
- backoff_measured: the spill measured as set: forecast, and the outcomes of 10 x 10 gr no worse than with the
  back-off off beyond the seed spread (4 seeds): good no more than 2 a seed fewer, overs no more than 2 a seed more;
- precharge: a gate that holds what reaches it closed (a store, its travel a time), a precharge of 0.2 s at 2 rev/s after
  each throw, counted in the next throw's forecast: the good share no worse than without it beyond the seed spread;
- gate_at_coarse_stop: the gate's move at the coarse stop taken as 15 s, longer than any rest: no verdict before it ends.

Usage, with the repository's venv, from this folder:
    python f3_cases.py [--throws 10]
"""

import argparse
import collections
import dataclasses
import sys

import rigs
import staged
import throw
from throw import DT_S, Session

RECORD = "pico2w_ar2209_night"
SEEDS = (11, 22, 33, 44)
TARGET_GR = 10.0
TOLERANCE_GR = 0.03
BACKOFF = (0.25, 1.0)
BENCH_SPILL_GR = 0.83           # the bench's reverse spill of AR2209 (11-21), A11's row
PRECHARGE_S, PRECHARGE_RPS = 0.2, 2.0       # about 4 gr, under the 10 gr target
GATE_TRAVEL_S = 0.3
GATE_COARSE_S = 15.0            # longer than any rest: the hold, not a gate's real travel, is what is shown


class GateSession(Session):
    """`throw.Session` with a gate: what reaches it while closed is held, and falls when it opens; a precharge poured
    into it after each throw, its turns given to the next throw's controllers."""

    def __init__(self, rig, seed, precharge: bool):
        super().__init__(rig, seed)
        self.precharge_on = precharge
        self.held = []                  # landings held by the closed gate: (mass, speed)
        self.pending_turns = 0.0

    def throw_gated(self, twin, target, tolerance):
        coarse, fine = self.coarse, self.fine
        cup = throw.Cup()
        scale = throw.ScaleState(self.rig.scale, self.rng, DT_S)
        now = 0.0
        # the gate opens, its travel waited, then the held powder falls and the tubes start (E10)
        for mass, speed in self.held:
            cup.landings.append((now + GATE_TRAVEL_S + 0.05, mass, speed))
        cup.landings.sort()
        self.held = []
        while now < GATE_TRAVEL_S:
            now += DT_S
            reading = scale.step(now, cup.step(now, DT_S))
        start_ms = round(now * 1000.0)
        twin.start(target, start_ms)
        if self.pending_turns > 0.0:
            twin.precharge(self.pending_turns * twin.reference.cal.coarse_gr_turn, PRECHARGE_RPS, start_ms)
            self.pending_turns = 0.0
        until = now + 120.0
        while not twin.done and now < until:
            now += DT_S
            twin.on_tick(now * 1000.0)
            coarse.set_speed(twin.coarse_rps)
            fine.set_speed(twin.fine_rps)
            coarse.step(now, DT_S, cup.landings)
            fine.step(now, DT_S, cup.landings)
            reading = scale.step(now, cup.step(now, DT_S))
            if reading is not None:
                twin.on_reading(now * 1000.0, reading)
                coarse.set_speed(twin.coarse_rps)
                fine.set_speed(twin.fine_rps)
        coarse.set_speed(0.0)
        fine.set_speed(0.0)
        tail = []
        for _ in range(int(2.0 / DT_S)):       # the close, its travel and 500 ms: what still falls lands in the cup
            now += DT_S
            coarse.step(now, DT_S, cup.landings)
            fine.step(now, DT_S, cup.landings)
        if self.precharge_on:
            before = coarse.turns
            coarse.set_speed(PRECHARGE_RPS)
            for _ in range(int(PRECHARGE_S / DT_S)):
                now += DT_S
                coarse.step(now, DT_S, tail)
            coarse.set_speed(0.0)
            for _ in range(int(1.0 / DT_S)):
                now += DT_S
                coarse.step(now, DT_S, tail)
            self.pending_turns = coarse.turns - before
            self.held = [(mass, speed) for _, mass, speed in tail]
        return twin.reference.result


def series(cal, rig, limits, seed, count, precharge=False):
    """The reference's throws; returns (results, its rest times after the coarse stop)."""
    reference = staged.Staged(cal, rig.acceleration_rps2)
    reference.gate_coarse_s = limits.gate_coarse_s
    session = GateSession(rig, seed, precharge)
    results, waits = [], []
    for _ in range(count):
        watch = Watch(reference)
        results.append(session.throw_gated(watch, TARGET_GR, TOLERANCE_GR))
        if watch.coarse_off is not None and watch.rest_at is not None:
            waits.append(watch.rest_at - watch.coarse_off)
    return results, waits


class Watch:
    """The reference given the unit's whole milliseconds, the coarse stop's and the first verdict's times kept."""

    def __init__(self, reference):
        self.reference = reference
        self.coarse_off = self.rest_at = None
        self.now_ms = 0.0

    def __getattr__(self, name):
        return getattr(self.reference, name)

    def precharge(self, grains, speed_rps, now_ms):
        self.reference.precharge(grains, speed_rps, now_ms)

    def _keep(self):
        ref = self.reference
        if self.coarse_off is None and ref.coarse_off_at is not None:
            self.coarse_off = ref.coarse_off_at
        if self.rest_at is None and (ref.first_error is not None or ref.done):
            self.rest_at = self.now_ms / 1000.0

    def start(self, target_gr: float, now_ms: float) -> None:
        self.now_ms = float(round(now_ms))
        self.reference.start(target_gr, self.now_ms)
        self._keep()

    def on_tick(self, now_ms: float) -> None:
        self.now_ms = float(round(now_ms))
        self.reference.on_tick(self.now_ms)
        self._keep()

    def on_reading(self, now_ms: float, weight: float) -> None:
        self.now_ms = float(round(now_ms))
        self.reference.on_reading(self.now_ms, weight)
        self._keep()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--throws", type=int, default=10)
    args = parser.parse_args()
    base = rigs.get_rig(RECORD)
    rig = dataclasses.replace(base, coarse=dataclasses.replace(base.coarse, reverse_spill_gr=BENCH_SPILL_GR))
    digit = rig.scale.digit_gr
    band = TOLERANCE_GR - digit / 2.0
    off = staged.sim_limits()
    on = dataclasses.replace(off, backoff_turns=BACKOFF[0], backoff_rps=BACKOFF[1])
    failures = []

    # no spill measured, or measured at another back-off: the coarse stop threshold's room, said
    unmeasured = staged.measure(rig, SEEDS[0])
    for name, measured_with, set_to in (("backoff_unmeasured", off, on),
                                        ("backoff_changed", on, dataclasses.replace(on, backoff_turns=0.5))):
        measured = staged.measure_on(staged.TwinBench(rig, SEEDS[0]), digit, rig.scale.report_s, measured_with) \
            if measured_with is on else unmeasured
        cal = staged.for_finish(measured, "glide", band, set_to)
        print(f"{name}: coarse room {cal.coarse_room_gr:.3f}, fallback {cal.backoff_fallback:g}, back-off "
              f"{cal.backoff_s:.2f} s")
        if not (cal.backoff_fallback == 1.0 and abs(cal.coarse_room_gr - set_to.coarse_threshold_gr) < 1e-9):
            failures.append(f"{name}: not the coarse stop threshold's room, or not said")
        staged.rehearse(cal, Session(rig, SEEDS[0] + 1), rig.acceleration_rps2)
        series(cal, rig, set_to, SEEDS[0], 3)

    # measured as set: the spill forecast, the good share no worse than with the back-off off
    shares, over_counts = {}, {}
    for label, limits in (("back-off off", off), ("back-off measured", on)):
        goods, overs = 0, 0
        for seed in SEEDS:
            measured = staged.measure_on(staged.TwinBench(rig, seed), digit, rig.scale.report_s, limits)
            cal = staged.for_finish(measured, "glide", band, limits)
            staged.rehearse(cal, Session(rig, seed + 1), rig.acceleration_rps2)
            results, _ = series(cal, rig, limits, seed, args.throws)
            goods += results.count("good")
            overs += results.count("over")
        shares[label] = goods
        over_counts[label] = overs
        extra = f"; its spill {cal.backoff_unseen_gr:.3f} at the stop, its effect {cal.backoff_gr:+.3f}" if limits is on else ""
        print(f"backoff_measured, {label}: good {goods}, over {overs} of {len(SEEDS) * args.throws}{extra}")
    if (shares["back-off measured"] < shares["back-off off"] - len(SEEDS) * 2
            or over_counts["back-off measured"] > over_counts["back-off off"] + len(SEEDS) * 2):
        failures.append(f"backoff_measured: good {shares['back-off measured']} against {shares['back-off off']}, over "
                        f"{over_counts['back-off measured']} against {over_counts['back-off off']}")

    # the precharge, through the gate
    shares = {}
    for label, precharge in (("no precharge", False), ("precharge", True)):
        goods, overs, all_results = 0, 0, []
        for seed in SEEDS:
            cal = staged.calibrate(rig, seed, "glide", band)
            results, _ = series(cal, rig, off, seed, args.throws, precharge)
            all_results += results
            goods += results.count("good")
            overs += results.count("over")
        shares[label] = goods
        print(f"precharge, {label}: good {goods}, over {overs} of {len(SEEDS) * args.throws}; ends {dict(collections.Counter(all_results))}")
    if shares["precharge"] < shares["no precharge"] - len(SEEDS) * 2:
        failures.append(f"precharge: good {shares['precharge']} against {shares['no precharge']}")

    # the gate's move at the coarse stop
    gated = dataclasses.replace(off, gate_coarse_s=GATE_COARSE_S)
    cal = staged.calibrate(rig, SEEDS[0], "glide", band)
    results, waits = series(cal, rig, gated, SEEDS[0], args.throws)
    print(f"gate_at_coarse_stop: the first verdict {min(waits, default=0):.3f} to {max(waits, default=0):.3f} s after the "
          f"coarse stop; results {dict(collections.Counter(results))}")
    if waits and min(waits) < GATE_COARSE_S - 1e-3:
        failures.append(f"gate_at_coarse_stop: a verdict {min(waits):.3f} s after the coarse stop, before the gate's move ended")

    for failure in failures:
        print("FAIL", failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
