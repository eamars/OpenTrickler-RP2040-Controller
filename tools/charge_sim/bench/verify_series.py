"""A series of the unit's own throws against the calibration they were thrown on (ADR-009, gate 2).

Two questions, answered from the throws' rows alone (`series.py` keeps them), with nothing turned:
1. **Is each calibrated number right, as the throws see it?** Each tube's grains a turn and the coarse tube's spread
   a throw (what landed, on the turns the unit counted), and what still arrives after the fine tube's stop from its
   creep, each beside the calibration's figure.
2. **Is the forecast right where the controller decides on it?** The forecast against the truth (what finally
   landed, less what the fine tube had still to turn out), from the coarse tube's stop on; and at the fine tube's
   stop, against where the throw came to rest.

A throw's truth is its settled weight (the next throw's base less its own); the fine tube's powder after a moment is
its asked speeds made again (`forecast_replay.flows`) at the grains a turn the throws give it.

    ..\\..\\..\\venv\\Scripts\\python verify_series.py ..\\recordings\\20261122_115105_series_glide002_c_lagshift_pico2w_ar2209
"""

import argparse
import json
import statistics
import sys
from pathlib import Path

import numpy

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from staged import from_json, lag_table, set_rules, shift_lags, table     # noqa: E402
from forecast_replay import flows, STEP_S     # noqa: E402

AFTER_COARSE_S = (0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.5)


def load(folders: list) -> tuple:
    throws, cal = [], None
    for folder in folders:
        series = json.loads((folder / "series.json").read_text(encoding="utf-8"))
        cal_path = (Path(__file__).resolve().parent / series["calibration_file"].replace("\\", "/")).resolve()
        cal = from_json(json.loads(cal_path.read_text(encoding="utf-8")))
        if series.get("lag_shift_s"):
            shift_lags(cal, series["lag_shift_s"])
        cal.coarse_gr_turn = series.get("coarse_gr_turn", cal.coarse_gr_turn)
        cal.fine_gr_turn = series.get("fine_gr_turn", cal.fine_gr_turn)
        set_rules(cal, series["finish"], series["band_gr"])     # as the unit makes them from what is written
        for throw in series["throws"]:
            if len(throw["rows"]) >= 40 and throw.get("settled") is not None:
                throws.append(throw)
    return throws, cal


def spread(values: list) -> str:
    if len(values) < 2:
        return f"n {len(values)}"
    return f"mean {statistics.fmean(values):+.3f}, sd {statistics.stdev(values):.3f}, n {len(values)}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    args = parser.parse_args()
    throws, cal = load(args.folders)
    print(f"{len(throws)} throws with their rows and settled weight\n")

    # 1. Grains a turn. In closed loop every throw lands at its target, so the two tubes' turns cannot be told apart
    # by a regression; each tube is read where only its own powder is arriving.
    #   coarse: the reading once the coarse tube is quiet after its stop, less the fine tube's powder shown by then
    #   fine: from there to the first rest, on the fine tube's turns between (each at its own lag)
    fine_lags = lag_table(cal.fine_stops, cal.fine_gr_turn)
    coarse_yield, fine_yield = [], []
    for throw in throws:
        rows = throw["rows"]
        flow, _ = flows(rows, throw["coarse_turns"])
        coarse_off = next(r[0] / 1000.0 for r in rows if r[3] == 0.0)
        settle = coarse_off + cal.coarse_stops[0].quiet_s + 0.3
        j = min(range(len(rows)), key=lambda i: abs(rows[i][0] / 1000.0 - settle))
        t1 = rows[j][0] / 1000.0
        i1 = min(len(flow) - 1, int((t1 - table(fine_lags, flow[int(t1 / STEP_S)][4])) / STEP_S))
        fine_shown = flow[i1][2] * cal.fine_gr_turn
        coarse_yield.append((rows[j][1] - fine_shown) / throw["coarse_turns"])
        if throw["moves"] == 0 and throw["streams"] == 0:
            fine_rest = throw["target"] + throw["first"] - rows[j][1]
            fine_turns = flow[-1][2] - flow[i1][2]
            if fine_turns > 0.2:
                fine_yield.append(fine_rest / fine_turns)
    print("THE CALIBRATION, AS THE THROWS SEE IT")
    print(f"  coarse grains a turn   calibration {cal.coarse_gr_turn:.3f}   throws "
          f"{statistics.median(coarse_yield):.3f} ({(statistics.median(coarse_yield) / cal.coarse_gr_turn - 1) * 100:+.1f}%)"
          f", n {len(coarse_yield)}")
    print(f"  coarse spread a throw  calibration {cal.coarse_share * 100:.1f}%     throws "
          f"{statistics.stdev(coarse_yield) / statistics.fmean(coarse_yield) * 100:.1f}%")
    if len(fine_yield) >= 2:
        print(f"  fine grains a turn     calibration {cal.fine_gr_turn:.3f}   throws {statistics.median(fine_yield):.3f} "
              f"({(statistics.median(fine_yield) / cal.fine_gr_turn - 1) * 100:+.1f}%; sd {statistics.stdev(fine_yield):.3f}), n {len(fine_yield)} "
              f"(throws with no move, from the coarse tube's quiet to the first rest)")
    fine_gr = statistics.median(fine_yield) if len(fine_yield) >= 2 else cal.fine_gr_turn

    # 2. The forecast from the coarse tube's stop on, against the truth
    errors = {offset: [] for offset in AFTER_COARSE_S}
    at_fine_stop = {"coarse quiet": [], "coarse still arriving": []}
    after_creep = []
    for throw in throws:
        rows = throw["rows"]
        flow, _ = flows(rows, throw["coarse_turns"])
        fine_total = flow[-1][2]
        fine_scale = throw["fine_turns"] / fine_total if fine_total > 0 else 1.0

        def fine_after(t: float) -> float:
            i = min(len(flow) - 1, max(0, int(t / STEP_S)))
            return (fine_total - flow[i][2]) * fine_scale * fine_gr

        coarse_off = next(r[0] / 1000.0 for r in rows if r[3] == 0.0)
        for offset in AFTER_COARSE_S:
            j = min(range(len(rows)), key=lambda i: abs(rows[i][0] / 1000.0 - (coarse_off + offset)))
            truth = throw["settled"] - fine_after(rows[j][0] / 1000.0)
            errors[offset].append(rows[j][2] - truth)
        # the fine tube's stop: the first row from which it stays still for 1.5 s
        stop = None
        for i in range(len(rows)):
            if rows[i][0] / 1000.0 > coarse_off and rows[i][4] == 0.0:
                j = i
                while j < len(rows) and rows[j][4] == 0.0 and rows[j][0] - rows[i][0] < 1500:
                    j += 1
                if j == len(rows) or rows[j][0] - rows[i][0] >= 1500:
                    stop = i
                    break
        if stop is None:
            continue
        first_rest = throw["target"] + throw["first"]
        quiet = cal.coarse_stops[0].quiet_s
        kind = "coarse quiet" if rows[stop][0] / 1000.0 >= coarse_off + quiet else "coarse still arriving"
        at_fine_stop[kind].append(first_rest - rows[stop][2])
        if kind == "coarse quiet" and rows[stop - 1][4] <= 2.0 * cal.floor_rps:
            after_creep.append(first_rest - rows[stop][1])
    creep = min(cal.fine_stops, key=lambda s: abs(s.speed - cal.floor_rps))
    print(f"  after a creep stop     calibration {creep.unseen_gr:+.3f} (sd {creep.spread_gr:.3f}, at {creep.speed:.3f} rev/s)"
          f"   throws {spread(after_creep)}")
    print(f"  the coarse tube quiet  calibration {cal.coarse_stops[0].quiet_s:.2f} s after its stop from "
          f"{cal.coarse_stops[0].speed} rev/s\n")

    print("THE FORECAST, LESS THE TRUTH (gr), FROM THE COARSE TUBE'S STOP ON")
    for offset in AFTER_COARSE_S:
        print(f"  +{offset:.1f} s   {spread(errors[offset])}")
    print("\nAT THE FINE TUBE'S STOP: WHERE THE THROW RESTED, LESS THE FORECAST (gr)")
    for kind, values in at_fine_stop.items():
        if len(values) >= 2:
            print(f"  {kind:22s} {spread(values)}")


if __name__ == "__main__":
    main()
