"""Step 2's forecast check (ADR-009, step 2, gate 1): the fine tube's weight a turn estimated within each throw,
against the first build's stored one, on the kept bench series.

The predictor matters only for a throw's last grain or gram, so it runs with the fine tube alone, below the coarse
tube's stop threshold (the owner, 2026-11-25). Each throw's rows (`series.py` kept them: the time, the reading, the
unit's own forecast, each tube's speed asked) are replayed: the fine tube's turns turned out and not yet shown, in
10 ms parcels of the speeds asked, each shown after the lag of its speed (the unit's own lag table). At the fine
tube's stop, with the coarse tube quiet and at least `--alone` turns shown since, the forecast is the reading plus
those turns times a weight a turn:
- **stored:** the calibration's, as the unit held it (its echo, `@cal head`): the first build's forecast; and the
  same 15% under and over, to show how much the stop depends on it;
- **none:** the reading alone;
- **in-throw:** from the throw's own readings since the coarse tube went quiet: the slope of the reading on the fine
  turns shown (a regression), or the rise over the turns between its two ends.

Judged as where the throw rested, less the forecast. (The run of 17:58, with the coarse tube's stop and the slope's
fallback, is the commit 23d85a3's version of this tool.)

    venv\\Scripts\\python tools\\charge_sim\\bench\\forecast_in_throw.py
"""

import argparse
import re
import statistics
import sys
from pathlib import Path

import numpy

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from forecast_replay import flows, PARCEL_S, STEP_S     # noqa: E402
from replay import series_of     # noqa: E402

HERE = Path(__file__).resolve().parent
RECORDINGS = HERE.parent / "recordings"
KNOWN_TURNS = 0.15          # fine turns shown alone before the throw's own weight a turn is taken
QUIET_HOLD_S = 1.5          # the fine tube's stop: still for this long
CREEP_RPS = 0.08            # a stop from below this is a stop from the creep


def numbers(line: str) -> dict:
    return {name: float(value) for name, value in re.findall(r"(\w+)=(-?[\d.eE+-]+)", line)}


def echo(lines: list) -> dict:
    """The calibration the unit held, from its echo: each tube's weight a turn, its lag table, the coarse quiet."""
    out = {"coarse": [], "fine": []}
    for line in lines:
        words = line.split()
        if len(words) < 3 or words[0] != "@cal":
            continue
        if words[1] == "head":
            out["head"] = numbers(line)
        elif words[1] == "stop":
            n = numbers(line)
            out[words[2]].append((n["speed"], n["lag"], n["quiet"]))
    return out


def lag_of(points: list, speed: numpy.ndarray) -> numpy.ndarray:
    return numpy.interp(speed, [p[0] for p in points], [p[1] for p in points])


class Throw:
    """A throw's fine turns, shown and not, at each of its rows."""

    def __init__(self, throw: dict, cal: dict):
        rows = numpy.array(throw["rows"], dtype=float)
        self.t = rows[:, 0] / 1000.0
        self.w = rows[:, 1]
        self.coarse_asked = rows[:, 3]
        self.fine_asked = rows[:, 4]
        flow, _ = flows(throw["rows"], throw["coarse_turns"])
        flow = numpy.array(flow)
        step = int(round(PARCEL_S / STEP_S))
        pick = flow[step::step]
        before = numpy.vstack([numpy.zeros(5), pick[:-1]])
        at = (before[:, 0] + pick[:, 0]) / 2.0
        turns = numpy.maximum(0.0, pick[:, 2] - before[:, 2])
        due = at + lag_of(cal["fine"], pick[:, 4])
        now = self.t[:, None]
        out = at[None, :] <= now
        self.shown = ((due[None, :] <= now) & out) @ turns
        self.unseen = ((due[None, :] > now) & out) @ turns
        coarse_off = float(self.t[numpy.argmax(self.coarse_asked == 0.0)])
        quiet = coarse_off + cal["coarse"][0][2] if throw["coarse_turns"] > 0.0 else float(self.t[0])
        self.q = int(numpy.searchsorted(self.t, quiet))
        self.first_rest = throw["target"] + throw["first"]
        self.stored = cal["head"]["fine_gr_turn"]
        self.stop = self.fine_stop(coarse_off)

    def fine_stop(self, coarse_off: float):
        """The first row after the coarse tube's quiet from which the fine tube stays still for QUIET_HOLD_S."""
        for i in range(self.q, len(self.t)):
            if self.t[i] <= coarse_off or self.fine_asked[i] != 0.0:
                continue
            j = i
            while j < len(self.t) and self.fine_asked[j] == 0.0 and self.t[j] - self.t[i] < QUIET_HOLD_S:
                j += 1
            if j == len(self.t) or self.t[j] - self.t[i] >= QUIET_HOLD_S:
                return i
        return None

    def alone(self, i: int) -> float:
        """Fine turns shown since the coarse tube went quiet."""
        return float(self.shown[i] - self.shown[self.q])

    def regression(self, i: int):
        """The slope of the reading on the fine turns shown, from the coarse tube's quiet to row i."""
        if i - self.q < 4 or self.alone(i) < KNOWN_TURNS:
            return None
        return float(numpy.polyfit(self.shown[self.q:i + 1], self.w[self.q:i + 1], 1)[0])

    def two_point(self, i: int):
        if self.alone(i) < KNOWN_TURNS:
            return None
        return float((self.w[i] - self.w[self.q]) / self.alone(i))

    def rest_less_forecast(self, i: int, gr_turn: float) -> float:
        return float(self.first_rest - (self.w[i] + gr_turn * self.unseen[i]))


def load(folder: Path) -> list:
    series = series_of(folder.name)
    cal = echo(series.get("ccal", []))
    if "head" not in cal or not cal["coarse"] or not cal["fine"]:
        return []
    return [Throw(raw, cal) for raw in series["throws"]
            if len(raw.get("rows", [])) >= 40 and raw.get("settled") is not None]


def spread(values: list) -> str:
    if len(values) < 2:
        return f"n {len(values)}"
    return f"mean {statistics.fmean(values):+.3f}  sd {statistics.stdev(values):.3f}  n {len(values)}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="*", type=Path, help="series folders (all kept series by default)")
    parser.add_argument("--alone", type=float, default=0.5, help="the least fine turns shown alone before the stop")
    args = parser.parse_args()
    folders = args.folders or sorted(p for p in RECORDINGS.glob("*_series_*") if (p / "series.json").exists())
    ways = ("stored", "stored x0.85", "stored x1.15", "none", "in-throw, regression", "in-throw, two ends")
    at_stop = {way: [] for way in ways}
    at_creep = {way: [] for way in ways}
    unseen, used, skipped = [], 0, 0
    for folder in folders:
        throws = load(folder)
        if not throws:
            skipped += 1
            continue
        used += 1
        for throw in throws:
            s = throw.stop
            if s is None or throw.alone(s) < args.alone:
                continue
            unseen.append(float(throw.unseen[s]))
            weights = {"stored": throw.stored, "stored x0.85": throw.stored * 0.85,
                       "stored x1.15": throw.stored * 1.15, "none": 0.0,
                       "in-throw, regression": throw.regression(s), "in-throw, two ends": throw.two_point(s)}
            for way, gr_turn in weights.items():
                value = throw.rest_less_forecast(s, gr_turn)
                at_stop[way].append(value)
                if throw.fine_asked[s - 1] < CREEP_RPS:
                    at_creep[way].append(value)
    print(f"{used} series ({skipped} with no calibration echo left out); {len(unseen)} fine stops with at least "
          f"{args.alone} turns shown alone; fine turns unseen at the stop: median {statistics.median(unseen):.3f}, "
          f"nine in ten under {numpy.percentile(unseen, 90):.3f}")
    print("\nAT THE FINE TUBE'S STOP: WHERE THE THROW RESTED, LESS THE FORECAST (gr)")
    for way in ways:
        print(f"  {way:22s} all: {spread(at_stop[way])}   from the creep: {spread(at_creep[way])}")


if __name__ == "__main__":
    main()
