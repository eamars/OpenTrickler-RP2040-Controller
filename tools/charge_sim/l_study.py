"""Proposal L's study (ADR-009, gate 3; the manager's yes, 2026-11-24): the forecast in stages, in the reference.

`StagedStages` is the controller with its parcels shown along the scale's stages: nothing before the dead time `d`,
then the share (1 - s2) e^(-(t-d)/tau) + s2 e^(-(t-d)/tau2) still to come; a parcel leaves the forecast once under a
hundredth of it is to come. Every sum of the forecast weighs a parcel by its share still to come.

The study, its pass written first (progress.md, 2026-11-24, 02:25 and 02:50): on every stand-in, the learnt source given
the scale's stages as a calibration on that scale fits them (the median of its fine stops' fits within four times
the creep, `lag_stages.fit_run`), against the same given the scale's one measured lag (F.4: on `slower` 15 points over
a calibrated unit): the first three throws of 40 fresh sessions at 10 gr, over as often as a calibrated unit there,
within the noise (two SDs of the difference of two shares).

    python l_study.py [--sessions 40]
"""

import argparse
import math
import statistics
import sys

import f4_cases
import lag_stages
import staged
from throw import Session
from workers import Workers

SHOWN_LEAST = 0.01
CREEP_TIMES = 4.0
TARGET_GR = 10.0


class StagedStages(staged.Staged):
    """The controller, its parcels shown along the stages (d, tau, s2, tau2) rather than whole at their lag."""

    def __init__(self, cal, acceleration_rps2, stages):
        super().__init__(cal, acceleration_rps2)
        self.stages = stages
        d, tau, s2, tau2 = stages
        self.mean_lag = d + (1.0 - s2) * tau + s2 * tau2

    def _unshown(self, age: float) -> float:
        d, tau, s2, tau2 = self.stages
        if age < d:
            return 1.0
        late = age - d
        out = (1.0 - s2) * math.exp(-late / tau) if tau > 0 else 0.0
        if s2 > 0 and tau2 > 0:
            out += s2 * math.exp(-late / tau2)
        return out

    def _stamp(self, now: float) -> None:
        """A parcel made since the last stamp gets its making time and its share still to come."""
        for parcel in self.parcels:
            if len(parcel) == 3:
                parcel.extend([parcel[0] - self.mean_lag, 1.0])

    def _parcel(self, now: float) -> None:
        count = len(self.parcels)
        super()._parcel(now)
        for parcel in self.parcels[count:]:     # turned out over the stretch since the last: its middle
            parcel.extend([now - staged.PARCEL_S / 2.0, 1.0])

    def precharge(self, grains, speed_rps, now_ms):
        super().precharge(grains, speed_rps, now_ms)
        self._stamp(now_ms / 1000.0)

    def _left(self, parcel) -> float:
        return parcel[1] * parcel[4] if len(parcel) == 5 else parcel[1]

    def _take_due(self, now: float) -> tuple:
        self._stamp(now)
        due_gr, first_due = 0.0, None
        turning = {"coarse": self.coarse_rps > 0.0, "fine": self.fine_rps > 0.0}
        figured = {"coarse": self._figured("coarse"), "fine": self._figured("fine")}
        kept = []
        for parcel in self.parcels:
            share = self._unshown(now - parcel[3])
            if share < SHOWN_LEAST:
                share = 0.0
            shown = parcel[1] * (parcel[4] - share)
            parcel[4] = share
            if shown > 0.0:
                if not figured[parcel[2]]:
                    if self.count_mark[parcel[2]] is not None:
                        self.shown_turns[parcel[2]] += shown
                elif turning[parcel[2]]:
                    due_gr += shown
                    due_at = parcel[3] + self.stages[0]
                    first_due = due_at if first_due is None else min(first_due, due_at)
            if share > 0.0:
                kept.append(parcel)
        self.parcels = kept
        self._sum_unseen()
        return due_gr, first_due

    def _sum_unseen(self) -> None:
        figured = {"coarse": self._figured("coarse"), "fine": self._figured("fine")}
        self.unseen = sum(self._left(p) for p in self.parcels if figured[p[2]])
        self.coarse_unseen = sum(self._left(p) for p in self.parcels if p[2] == "coarse" and figured["coarse"])

    def _rest_ready(self, now: float, noise: float) -> bool:
        """The verdict waits until the forecast's powder still to show is under the scale's noise (C24): by the stages,
        the late powder is known, so a rest is not judged while it is still landing (the manager, on E9)."""
        self._take_due(now)
        return self.unseen < noise

    def _fine_unseen_turns(self) -> float:
        return sum(self._left(p) for p in self.parcels if p[2] == "fine") / self.cal.fine_gr_turn


def _unshown(model, age: float) -> float:
    d, tau, s2, tau2 = model
    if age < d:
        return 1.0
    late = age - d
    out = (1.0 - s2) * math.exp(-late / tau) if tau > 0 else 0.0
    if s2 > 0 and tau2 > 0:
        out += s2 * math.exp(-late / tau2)
    return out


def _read(table: list, x: float) -> tuple:
    """A model read at x from (x, d, tau, s2, tau2) rows by x: each parameter straight between two rows, the end
    row's outside them, as the lag table is read."""
    if x <= table[0][0]:
        return table[0][1:]
    for lo, hi in zip(table, table[1:]):
        if x <= hi[0]:
            share = (x - lo[0]) / (hi[0] - lo[0]) if hi[0] > lo[0] else 0.0
            return tuple(a + (b - a) * share for a, b in zip(lo[1:], hi[1:]))
    return table[-1][1:]


class StagedByFlow(StagedStages):
    """Form (a): each parcel shown by the model of its own speed (a calibrated unit) or flow (the learnt source), each
    stop's own fit read between the stops; the verdict waits until the forecast's powder still to show is under the
    noise, by each parcel's own model."""

    def __init__(self, cal, acceleration_rps2, tables: dict, by_flow: bool = False):
        staged.Staged.__init__(self, cal, acceleration_rps2)
        self.tables = tables            # {"coarse": [(x, d, tau, s2, tau2)], "fine": [...]}, x a speed or a flow
        self.by_flow = by_flow

    def _model(self, tube: str, speed: float) -> tuple:
        figure = self.cal.coarse_gr_turn if tube == "coarse" else self.cal.fine_gr_turn
        if self.by_flow:
            # by flow, from another powder's stops on this scale; before the tube's figure is born, its slowest
            x = speed * figure if figure > 0.0 else 0.0
        else:
            x = speed
        return _read(self.tables[tube], x)

    def _stamp(self, now: float) -> None:
        for parcel in self.parcels:
            if len(parcel) == 3:        # a precharge's or a back-off's: made now, at the coarse floor's model
                parcel.extend([now, 1.0, self._model(parcel[2], self.cal.coarse_floor_rps)])

    def _parcel(self, now: float) -> None:
        count = len(self.parcels)
        staged.Staged._parcel(self, now)
        for parcel in self.parcels[count:]:
            parcel.extend([now - staged.PARCEL_S / 2.0, 1.0, self._model(parcel[2], self.speed[parcel[2]])])

    def _take_due(self, now: float) -> tuple:
        self._stamp(now)
        due_gr, first_due = 0.0, None
        turning = {"coarse": self.coarse_rps > 0.0, "fine": self.fine_rps > 0.0}
        figured = {"coarse": self._figured("coarse"), "fine": self._figured("fine")}
        kept = []
        for parcel in self.parcels:
            share = _unshown(parcel[5], now - parcel[3])
            if share < SHOWN_LEAST:
                share = 0.0
            shown = parcel[1] * (parcel[4] - share)
            parcel[4] = share
            if shown > 0.0:
                if not figured[parcel[2]]:
                    if self.count_mark[parcel[2]] is not None:
                        self.shown_turns[parcel[2]] += shown
                elif turning[parcel[2]]:
                    due_gr += shown
                    due_at = parcel[3] + parcel[5][0]
                    first_due = due_at if first_due is None else min(first_due, due_at)
            if share > 0.0:
                kept.append(parcel)
        self.parcels = kept
        self._sum_unseen()
        return due_gr, first_due


def stop_tables(cal, runs: list, digit: float, gr_turns: dict = None) -> dict:
    """Each stop's own fit, the median of its runs' fits, as rows by speed (or by flow, the speed times the tube's
    figure given in gr_turns)."""
    tables = {}
    for tube, stops in (("coarse", cal.coarse_stops), ("fine", cal.fine_stops)):
        rows = []
        for stop in stops:
            fits = [lag_stages.fit_run(run, digit) for run in runs
                    if run["stop"] == "held" and run["tube"] == tube and abs(run["speed_asked_rps"] - stop.speed) < 1e-9]
            fits = [f for f in fits if f["flow"] > 0.0 and f["tau"] > 0.0]     # a run that poured nothing fits nothing
            if not fits:
                continue
            model = tuple(statistics.median(f[k] for f in fits) for k in ("d", "tau", "share2", "tau2"))
            x = stop.speed * gr_turns[tube] if gr_turns else stop.speed
            rows.append((x,) + model)
        tables[tube] = sorted(rows)
    return tables


def calibrated_by_flow(rig, seed: int, band: float) -> tuple:
    """A calibration on this rig and its stops' own fits by speed: (calibration, tables)."""
    digit = rig.scale.digit_gr
    bench = lag_stages.Recorded(staged.TwinBench(rig, seed))
    cal = staged.measure_on(bench, digit, rig.scale.report_s)
    staged.set_rules(cal, "glide", band)
    staged.rehearse(cal, Session(rig, seed + 1), rig.acceleration_rps2)
    return cal, stop_tables(cal, bench.runs, digit)


def scale_by_flow(rig, seed: int) -> tuple:
    """The scale's fits by flow, as a calibration of another session on it measures them, and its rest."""
    digit = rig.scale.digit_gr
    band = f4_cases.TOLERANCE_GR - digit / 2.0
    bench = lag_stages.Recorded(staged.TwinBench(rig, seed + 50))
    measured = staged.for_finish(staged.measure_on(bench, digit, rig.scale.report_s), "glide", band)
    tables = stop_tables(measured, bench.runs, digit, {"coarse": measured.coarse_gr_turn, "fine": measured.fine_gr_turn})
    return tables, measured.rest_s


def scale_stages(rig, seed: int) -> tuple:
    """The scale's stages and its one lag and rest, as a calibration on it measures them: the median of the fits of
    its fine stops within CREEP_TIMES of its creep; the lag the fine stops' longest, the rest the calibration's."""
    digit = rig.scale.digit_gr
    band = f4_cases.TOLERANCE_GR - digit / 2.0
    bench = lag_stages.Recorded(staged.TwinBench(rig, seed + 50))
    measured = staged.for_finish(staged.measure_on(bench, digit, rig.scale.report_s), "glide", band)
    creep = measured.creep_rps
    fits = [lag_stages.fit_run(run, digit) for run in bench.runs
            if run["stop"] == "held" and run["tube"] == "fine" and run["run_s"] >= 1.0
            and run["speed_asked_rps"] <= CREEP_TIMES * max(creep, 1e-6) + 1e-9]
    stages = (statistics.median(f["d"] for f in fits), statistics.median(f["tau"] for f in fits),
              statistics.median(f["share2"] for f in fits), statistics.median(f["tau2"] for f in fits))
    lag = max(l for _, l in staged.lag_table(measured.fine_stops, measured.fine_gr_turn))
    return stages, lag, measured.rest_s


def calibrated_stages(rig, seed: int, band: float) -> tuple:
    """A calibration on this rig, as staged.calibrate makes it, and the stages its own fine stops fit (the median of
    those within CREEP_TIMES of its creep): (calibration, stages)."""
    digit = rig.scale.digit_gr
    bench = lag_stages.Recorded(staged.TwinBench(rig, seed))
    cal = staged.measure_on(bench, digit, rig.scale.report_s)
    staged.set_rules(cal, "glide", band)
    staged.rehearse(cal, Session(rig, seed + 1), rig.acceleration_rps2)
    fits = [lag_stages.fit_run(run, digit) for run in bench.runs
            if run["stop"] == "held" and run["tube"] == "fine" and run["run_s"] >= 1.0
            and run["speed_asked_rps"] <= CREEP_TIMES * max(cal.creep_rps, 1e-6) + 1e-9]
    stages = (statistics.median(f["d"] for f in fits), statistics.median(f["tau"] for f in fits),
              statistics.median(f["share2"] for f in fits), statistics.median(f["tau2"] for f in fits))
    return cal, stages


def job(args: tuple) -> list:
    name, kind, seed, count = args
    rig = f4_cases.stand_in(name)
    digit = rig.scale.digit_gr
    band = f4_cases.TOLERANCE_GR - digit / 2.0
    limits = staged.sim_limits()
    if kind == "calibrated":
        controller = staged.Staged(staged.calibrate(rig, seed, "glide", band), rig.acceleration_rps2)
        controller.pid = staged.profile_pid(limits)
    elif kind == "calibrated_stages":
        cal, tables = calibrated_by_flow(rig, seed, band)
        controller = StagedByFlow(cal, rig.acceleration_rps2, tables)
        controller.pid = staged.profile_pid(limits)
    else:
        stages, lag, rest = scale_stages(rig, seed)
        if kind == "stages":
            tables, rest = scale_by_flow(rig, seed)
            mean = max(row[1] + (1 - row[3]) * row[2] + row[3] * row[4] for row in tables["fine"])
            cal = staged.learnt_calibration(digit, rig.scale.report_s, band, mean, rest, limits)
            controller = StagedByFlow(cal, rig.acceleration_rps2, tables, by_flow=True)
        else:
            cal = staged.learnt_calibration(digit, rig.scale.report_s, band, lag, rest, limits)
            controller = staged.Staged(cal, rig.acceleration_rps2)
        controller.pid = staged.profile_pid(limits)
    session = Session(rig, seed + 100)
    out = []
    for _ in range(count):
        r = session.throw(controller, TARGET_GR, f4_cases.TOLERANCE_GR, timeout_s=130.0)
        out.append({"over": r.true_error >= f4_cases.TOLERANCE_GR, "result": controller.result if controller.done else "timeout",
                    "elapsed": r.elapsed_s})
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sessions", type=int, default=40)
    args = parser.parse_args()
    kinds = ("calibrated", "calibrated_stages", "one_lag", "stages")
    jobs = [(name, kind, seed, 3) for name in f4_cases.names() for kind in kinds for seed in range(args.sessions)]
    with Workers() as pool:
        results = pool.map(job, jobs)
    failing = []
    for name in f4_cases.names():
        shares = {}
        for kind in kinds:
            throws = [t for j, r in zip(jobs, results) if j[0] == name and j[1] == kind for t in r]
            shares[kind] = (sum(t["over"] for t in throws) / len(throws), len(throws),
                            sum(t["result"] not in ("good", "over", "short") for t in throws),
                            statistics.median(t["elapsed"] for t in throws))
        base, n = shares["calibrated"][0], shares["calibrated"][1]
        p = max(base, 1.0 / n)
        bar = 2.0 * math.sqrt(2.0 * p * (1.0 - p) / n)
        words = "; ".join(f"{k} over {100 * s[0]:.0f}%{f' ({s[2]} ends)' if s[2] else ''} {s[3]:.0f} s" for k, s in shares.items())
        # the learnt source with stages within the bar of a calibrated unit; a calibrated unit with stages no worse
        verdict = "ok" if (shares["stages"][0] - base <= bar and shares["stages"][2] == 0
                           and shares["calibrated_stages"][0] - base <= bar) else "FAIL"
        if verdict == "FAIL":
            failing.append(name)
        print(f"{name:10s} {words}; bar {100 * bar:.0f}: stages {verdict}", flush=True)
    print("VERDICT PASS" if not failing else f"VERDICT FAIL on {failing}")
    return 0 if not failing else 1


if __name__ == "__main__":
    sys.exit(main())
