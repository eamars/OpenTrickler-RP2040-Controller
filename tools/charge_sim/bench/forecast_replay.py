"""The forecast of a bench series' throws made again on the PC, and the same with another model of the scale (ADR-009).

A series' rows (`series.py`) hold each reading, the forecast the unit acted on and the two speeds it asked for. Each
tube's powder turned out is made again from those speeds (straight between two rows, the motors' ramps, the coarse
tube's scaled to the turns the unit counted), and the forecast made from it as the controller makes it: the reading
plus every parcel not yet shown. First with the controller's own model (a parcel shows whole at the lag of its
flow), to check the remaking against the unit's logged forecast; then with a parcel that shows after a delay and a
first-order lag adding to that same lag.

    ..\\..\\..\\venv\\Scripts\\python forecast_replay.py ..\\recordings\\*_series_* --tau 0.1
"""

import argparse
import json
import math
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from staged import from_json, lag_table, table     # noqa: E402

ACCELERATION_RPS2 = 50.0
STEP_S = 0.001
PARCEL_S = 0.01


def flows(rows: list, coarse_turns: float) -> list:
    """(time, coarse turns, fine turns, coarse speed, fine speed) a millisecond apart, from the speeds asked."""
    out, turned, speed = [], [0.0, 0.0], [0.0, 0.0]
    end = rows[-1][0] / 1000.0
    k, t = 0, 0.0
    coarse_off_at = None
    while t <= end:
        while k + 1 < len(rows) and rows[k + 1][0] / 1000.0 <= t:
            k += 1
        asked = [rows[k][3], rows[k][4]]
        if t < rows[0][0] / 1000.0:
            asked = [rows[0][3], rows[0][4]]
        for i in range(2):
            change = ACCELERATION_RPS2 * STEP_S
            speed[i] = min(asked[i], speed[i] + change) if speed[i] < asked[i] else max(asked[i], speed[i] - change)
            turned[i] += speed[i] * STEP_S
        out.append((t, turned[0], turned[1], speed[0], speed[1]))
        t += STEP_S
    # the coarse tube stops for good once: its counted turns scale its made-again turns
    made = out[-1][1]
    k = coarse_turns / made if made > 0 else 1.0
    return [(t, c * k, f, sc, sf) for t, c, f, sc, sf in out], k


def shown_share(age: float, lag: float, tau: float) -> float:
    if tau <= 0.0:
        return 0.0 if age < lag else 1.0
    delay = max(0.0, lag - tau)
    return 0.0 if age < delay else 1.0 - math.exp(-(age - delay) / tau)


def make_parcels(flow: list, cal, coarse_gr_turn: float, shift: float = 0.0) -> list:
    coarse_lags = lag_table(cal.coarse_stops, cal.coarse_gr_turn)
    fine_lags = lag_table(cal.fine_stops, cal.fine_gr_turn)
    parcels = []
    last = (0.0, 0.0, 0.0)
    step = int(PARCEL_S / STEP_S)
    for i in range(step, len(flow), step):
        t, c, f, sc, sf = flow[i]
        mid = (last[0] + t) / 2.0
        if c > last[1]:
            parcels.append((mid, (c - last[1]) * coarse_gr_turn, table(coarse_lags, sc) + shift, "coarse"))
        if f > last[2]:
            parcels.append((mid, (f - last[2]) * cal.fine_gr_turn, table(fine_lags, sf) + shift, "fine"))
        last = (t, c, f)
    return parcels


def fit_scale(throws: list, window_after_coarse: float = 1.0) -> None:
    """The shift of every measured lag and the lag tau that make the readings themselves best, all throws pooled.

    A throw's coarse powder a turn is its own (fitted in closed form for each shape), since the reading at the end
    of the coarse tube's stretch fixes it whatever the shape; the shape is in the rise.
    """
    results = []
    for shift in [i * 0.01 for i in range(-15, 6)]:
        for tau in [i * 0.02 for i in range(0, 11)]:
            total, count = 0.0, 0
            for rows, flow, cal, coarse_off in throws:
                parcels = make_parcels(flow, cal, 1.0, shift)
                use = [r for r in rows if r[0] / 1000.0 <= coarse_off + window_after_coarse]
                cs, fs, rs = [], [], []
                for row in use:
                    now = row[0] / 1000.0
                    c = f = 0.0
                    for at, grains, lag, tube in parcels:
                        if at > now:
                            break
                        s = grains * shown_share(now - at, lag, tau)
                        if tube == "coarse":
                            c += s
                        else:
                            f += s
                    cs.append(c)
                    fs.append(f)
                    rs.append(row[1])
                den = sum(c * c for c in cs)
                g = sum(c * (r - f) for c, f, r in zip(cs, fs, rs)) / den if den > 0 else 0.0
                total += sum((g * c + f - r) ** 2 for c, f, r in zip(cs, fs, rs))
                count += len(use)
            results.append((math.sqrt(total / count), shift, tau))
    results.sort()
    print("best fits of the readings (rms gr, shift of every lag s, tau s):")
    for r in results[:8]:
        print(f"  {r[0]:.4f}  {r[1]:+.2f}  {r[2]:.2f}")
    for shift, tau in ((0.0, 0.0), (0.0, 0.1), (-0.05, 0.0), (-0.06, 0.0), (-0.07, 0.0)):
        print("  as given:", [f"{r[0]:.4f}" for r in results if abs(r[1] - shift) < 1e-9 and abs(r[2] - tau) < 1e-9],
              f"shift {shift:+.2f} tau {tau:.2f}")


def replay(rows: list, flow: list, cal, tau: float, coarse_gr_turn: float, shift: float = 0.0) -> list:
    """The forecast at each row: the row's reading plus each parcel's share not yet shown at that row's time."""
    coarse_lags = lag_table(cal.coarse_stops, cal.coarse_gr_turn)
    fine_lags = lag_table(cal.fine_stops, cal.fine_gr_turn)
    parcels = []        # (turned out at, grains, its lag, tube)
    last = (0.0, 0.0, 0.0)
    step = int(PARCEL_S / STEP_S)
    for i in range(step, len(flow), step):
        t, c, f, sc, sf = flow[i]
        mid = (last[0] + t) / 2.0
        if c > last[1]:
            parcels.append((mid, (c - last[1]) * coarse_gr_turn, table(coarse_lags, sc) + shift, "coarse"))
        if f > last[2]:
            parcels.append((mid, (f - last[2]) * cal.fine_gr_turn, table(fine_lags, sf) + shift, "fine"))
        last = (t, c, f)
    out = []
    for row in rows:
        now = row[0] / 1000.0
        unseen = 0.0
        for at, grains, lag, tube in parcels:
            if at > now:
                break
            age = now - at
            if tau <= 0.0:
                share = 1.0 if age < lag else 0.0
            else:
                delay = max(0.0, lag - tau)
                share = 1.0 if age < delay else math.exp(-(age - delay) / tau)
            unseen += grains * share
        # parcels turned out after the row's time are not yet counted by the unit at that row: none here
        out.append(row[1] + unseen)
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    parser.add_argument("--tau", type=float, default=0.1, help="the scale's lag after its delay, s")
    parser.add_argument("--coarse-gr-turn", type=float, default=None, help="in place of the calibration's")
    parser.add_argument("--window", type=float, default=2.0, help="seconds from the start compared")
    parser.add_argument("--shift", type=float, default=0.0, help="added to every measured lag, s")
    parser.add_argument("--fit", action="store_true", help="fit the shift and tau to the readings themselves")
    args = parser.parse_args()
    if args.fit:
        throws = []
        for folder in args.folders:
            series = json.loads((folder / "series.json").read_text(encoding="utf-8"))
            cal_path = (Path(__file__).resolve().parent / series["calibration_file"].replace("\\", "/")).resolve()
            cal = from_json(json.loads(cal_path.read_text(encoding="utf-8")))
            for throw in series["throws"]:
                rows = throw["rows"]
                if len(rows) >= 40:
                    flow, _ = flows(rows, throw["coarse_turns"])
                    throws.append((rows, flow, cal, next(r[0] / 1000.0 for r in rows if r[3] == 0.0)))
        fit_scale(throws)
        return
    print("throw          k     remade rms  bulge logged  bulge remade  bulge tau  coarse gr/turn seen")
    bulges = {"logged": [], "remade": [], "tau": []}
    remade_rms = []
    for folder in args.folders:
        series = json.loads((folder / "series.json").read_text(encoding="utf-8"))
        cal_path = (Path(__file__).resolve().parent / series["calibration_file"].replace("\\", "/")).resolve()
        cal = from_json(json.loads(cal_path.read_text(encoding="utf-8")))
        gr_turn = args.coarse_gr_turn or cal.coarse_gr_turn
        for throw in series["throws"]:
            rows = throw["rows"]
            if len(rows) < 40:
                continue
            flow, k = flows(rows, throw["coarse_turns"])
            old = replay(rows, flow, cal, 0.0, cal.coarse_gr_turn)
            new = replay(rows, flow, cal, args.tau, gr_turn, args.shift)
            early = [i for i, r in enumerate(rows) if r[0] / 1000.0 <= args.window]
            remade_rms.append(math.sqrt(statistics.fmean((old[i] - rows[i][2]) ** 2 for i in early)))
            # the bulge: the forecast's highest in the first second above where it settles once the coarse tube's
            # powder has all shown, less the fine tube's powder turned out in between
            coarse_off = next(r[0] / 1000.0 for r in rows if r[3] == 0.0)
            settle_at = coarse_off + 0.9
            j_settle = min(range(len(rows)), key=lambda i: abs(rows[i][0] / 1000.0 - settle_at))
            fine_at = lambda t: flow[min(len(flow) - 1, int(t / STEP_S))][2] * cal.fine_gr_turn
            line = []
            for name, forecast in (("logged", [r[2] for r in rows]), ("remade", old), ("tau", new)):
                peak_i = max((i for i, r in enumerate(rows) if r[0] / 1000.0 <= 1.0), key=lambda i: forecast[i])
                fine_between = fine_at(rows[j_settle][0] / 1000.0) - fine_at(rows[peak_i][0] / 1000.0)
                bulge = forecast[peak_i] + fine_between - forecast[j_settle]
                bulges[name].append(bulge)
                line.append(bulge)
            # what the coarse tube poured a counted turn: the reading once its powder has shown, less the fine's shown
            t_seen = coarse_off + 1.2
            j_seen = min(range(len(rows)), key=lambda i: abs(rows[i][0] / 1000.0 - t_seen))
            fine_shown = fine_at(max(0.0, rows[j_seen][0] / 1000.0 - 0.65))
            seen = (rows[j_seen][1] - fine_shown) / throw["coarse_turns"]
            print(f"{series['name'][:9]} {int(throw['n']):3d} {k:5.2f} {remade_rms[-1]:10.3f} "
                  f"{line[0]:13.2f} {line[1]:13.2f} {line[2]:10.2f} {seen:12.2f}")
    print()
    q = lambda v: f"median {statistics.median(v):.2f}, 90% {sorted(v)[int(0.9 * (len(v) - 1))]:.2f}"
    print(f"remade against logged, rms over the first {args.window:.0f} s: {q(remade_rms)} gr")
    for name in bulges:
        within = sum(1 for b in bulges[name] if b < 0.2)
        print(f"bulge {name:7s}: {q(bulges[name])} gr; under 0.2 gr in {within} of {len(bulges[name])}")


if __name__ == "__main__":
    main()
