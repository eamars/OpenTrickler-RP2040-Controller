"""Step 2's forecast check (ADR-009, step 2, gate 1): the weight a turn estimated within each throw, against the first
build's stored one, on the kept bench series.

Each throw's rows (`series.py` kept them: the time, the reading, the unit's own forecast, each tube's speed asked) are
replayed four ways. The forecast is always the reading plus each tube's turns turned out and not yet shown (10 ms
parcels of the speeds asked, each shown after the lag of its speed, the unit's own lag table); only the weight a turn
differs:
- **stored:** the calibration's, as the unit held it (its echo, `@cal head`): the first build's forecast;
- **slope:** no weight a turn at all: the reading plus its own rise over the last quarter second times the lag;
- **in-throw:** from the throw's own readings alone: the coarse tube's from the reading over the coarse turns shown
  while the coarse tube's powder is the bulk of it, the fine tube's from the rise once the coarse tube is quiet; the
  slope until each is known;
- **carried:** the mean of the series' earlier throws' own, each as it worked out in hindsight (what RAM could carry
  from throw to throw, nothing stored): the first throw of a series has none and is left out.

Judged where the controller decides on it:
- at the fine tube's stop with the coarse tube quiet: where the throw rested, less the forecast;
- at the coarse tube's stop: the forecast less the truth (the throw's settled weight less what the fine tube still
  turned out, at its own weight a turn in hindsight).

    venv\\Scripts\\python tools\\charge_sim\\bench\\forecast_in_throw.py
"""

import argparse
import json
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
SLOPE_S = 0.25              # the slope's window
KNOWN_TURNS = 0.15          # turns shown before a tube's own weight a turn replaces the slope
QUIET_HOLD_S = 1.5          # the fine tube's stop: still for this long
CREEP_RPS = 0.08            # a stop from below this is a stop from the creep
WAYS = ("stored", "slope", "in-throw", "carried")
SAME = ("stored, same throws",)        # the stored forecast on the throws the carried one has


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
    xs = numpy.array([p[0] for p in points])
    ys = numpy.array([p[1] for p in points])
    return numpy.interp(speed, xs, ys)


class Throw:
    """A throw's turns, shown and not, at each of its rows."""

    def __init__(self, throw: dict, cal: dict):
        self.raw = throw
        rows = numpy.array(throw["rows"], dtype=float)
        self.t = rows[:, 0] / 1000.0
        self.w = rows[:, 1]
        self.unit_forecast = rows[:, 2]
        self.coarse_asked = rows[:, 3]
        self.fine_asked = rows[:, 4]
        flow, _ = flows(throw["rows"], throw["coarse_turns"])
        flow = numpy.array(flow)
        step = int(round(PARCEL_S / STEP_S))
        pick = flow[step::step]
        before = numpy.vstack([numpy.zeros(5), pick[:-1]])
        at = (before[:, 0] + pick[:, 0]) / 2.0
        dc = numpy.maximum(0.0, pick[:, 1] - before[:, 1])
        df = numpy.maximum(0.0, pick[:, 2] - before[:, 2])
        due_c = at + lag_of(cal["coarse"], pick[:, 3])
        due_f = at + lag_of(cal["fine"], pick[:, 4])
        now = self.t[:, None]
        out = at[None, :] <= now
        self.sc = ((due_c[None, :] <= now) & out) @ dc       # coarse turns shown by each row
        self.uc = ((due_c[None, :] > now) & out) @ dc        # coarse turns out and not yet shown
        self.sf = ((due_f[None, :] <= now) & out) @ df
        self.uf = ((due_f[None, :] > now) & out) @ df
        self.lag_c = lag_of(cal["coarse"], self.coarse_asked)
        self.lag_f = lag_of(cal["fine"], self.fine_asked)
        self.fine_total = flow[-1, 2]
        self.flow_t = flow[:, 0]
        self.flow_f = flow[:, 2]
        self.coarse_off = float(self.t[numpy.argmax(self.coarse_asked == 0.0)])
        self.coarse = throw["coarse_turns"] > 0.0
        self.quiet = self.coarse_off + cal["coarse"][0][2] if self.coarse else float(self.t[0])
        self.first_rest = throw["target"] + throw["first"]
        self.stop = self.fine_stop()
        self.kf_hind, self.kc_hind = self.hindsight()

    def fine_stop(self):
        """The first row after the coarse tube's stop from which the fine tube stays still for QUIET_HOLD_S."""
        for i in range(len(self.t)):
            if self.t[i] <= self.coarse_off or self.fine_asked[i] != 0.0:
                continue
            j = i
            while j < len(self.t) and self.fine_asked[j] == 0.0 and self.t[j] - self.t[i] < QUIET_HOLD_S:
                j += 1
            if j == len(self.t) or self.t[j] - self.t[i] >= QUIET_HOLD_S:
                return i
        return None

    def hindsight(self) -> tuple:
        """Each tube's weight a turn as the whole throw shows it: the fine tube's from the coarse tube's quiet to the
        first rest, the coarse tube's from the reading at its quiet less the fine tube's shown by then."""
        q = int(numpy.argmin(numpy.abs(self.t - (self.quiet + 0.3))))
        if self.stop is None:
            return None, None
        turns = self.sf[self.stop] + self.uf[self.stop] - self.sf[q]
        if turns < 0.2:
            return None, None
        kf = (self.first_rest - self.w[q]) / turns
        kc = (self.w[q] - kf * self.sf[q]) / self.raw["coarse_turns"] if self.coarse else 0.0
        return kf, kc

    def slope(self, i: int) -> float:
        j = int(numpy.searchsorted(self.t, self.t[i] - SLOPE_S))
        span = self.t[i] - self.t[j]
        return (self.w[i] - self.w[j]) / span if span > 0 else 0.0

    def forecast(self, i: int, way: str, carried: tuple = None) -> float:
        lag = self.lag_c[i] if self.coarse_asked[i] > 0.0 or self.t[i] < self.quiet else self.lag_f[i]
        if way == "slope":
            return self.w[i] + self.slope(i) * lag
        if way == "stored":
            kc, kf = self.stored
        elif way == "carried":
            kc, kf = carried
        else:
            kc, kf = self.in_throw(i)
            if kc is None:
                return self.w[i] + self.slope(i) * lag
            if kf is None:
                # the fine tube's own powder still unknown: the coarse's forecast on its weight, the fine by the slope
                # once the coarse tube is quiet, and its share of the reading before that
                if self.t[i] < self.quiet:
                    return self.w[i] + kc * self.uc[i]
                return self.w[i] + kc * self.uc[i] + self.slope(i) * self.lag_f[i]
        return self.w[i] + kc * self.uc[i] + kf * self.uf[i]

    def in_throw(self, i: int) -> tuple:
        """Each tube's weight a turn from the rows up to i alone (None until known)."""
        if self.t[i] < self.quiet:
            if self.sc[i] < KNOWN_TURNS:
                return None, None
            return self.w[i] / (self.sc[i] + 0.0), None     # the fine tube's powder counted as coarse, small share
        q = int(numpy.searchsorted(self.t, self.quiet))
        turns = self.sf[i] - self.sf[q]
        kc = (self.w[q] / self.sc[q]) if self.sc[q] > 0 else 0.0
        if turns < KNOWN_TURNS:
            return kc, None
        kf = (self.w[i] - self.w[q]) / turns
        kc = (self.w[q] - kf * self.sf[q]) / self.sc[q] if self.sc[q] > 0 else 0.0
        return kc, kf

    def truth(self, i: int, kf: float) -> float:
        k = int(numpy.searchsorted(self.flow_t, self.t[i]))
        k = min(k, len(self.flow_f) - 1)
        return self.raw["settled"] - (self.fine_total - self.flow_f[k]) * kf


def load(folder: Path) -> list:
    series = series_of(folder.name)
    cal = echo(series.get("ccal", []))
    if "head" not in cal or not cal["coarse"] or not cal["fine"]:
        return []
    out = []
    for raw in series["throws"]:
        if len(raw.get("rows", [])) < 40 or raw.get("settled") is None:
            continue
        throw = Throw(raw, cal)
        throw.stored = (cal["head"]["coarse_gr_turn"], cal["head"]["fine_gr_turn"])
        out.append(throw)
    return out


def spread(values: list) -> str:
    values = [float(v) for v in values]
    if len(values) < 2:
        return f"n {len(values)}"
    return f"mean {statistics.fmean(values):+.3f}  sd {statistics.stdev(values):.3f}  n {len(values)}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="*", type=Path, help="series folders (all kept series by default)")
    args = parser.parse_args()
    folders = args.folders or sorted(p for p in RECORDINGS.glob("*_series_*") if (p / "series.json").exists())
    at_fine = {way: [] for way in WAYS + SAME}
    at_creep = {way: [] for way in WAYS + SAME}
    at_coarse = {way: [] for way in WAYS + SAME}
    replayed, skipped, unlearnt = [], [], 0
    for folder in folders:
        throws = load(folder)
        if not throws:
            skipped.append(folder.name)
            continue
        seen = []           # each earlier throw's own weights a turn, in hindsight: what RAM could carry
        for throw in throws:
            replayed.append(float(numpy.sqrt(numpy.mean(
                (numpy.array([throw.forecast(i, "stored") for i in range(len(throw.t))]) - throw.unit_forecast) ** 2))))
            carried = None
            if seen:
                carried = (statistics.fmean(s[0] for s in seen), statistics.fmean(s[1] for s in seen))
            ways = [way for way in WAYS if way != "carried" or carried is not None]
            if throw.stop is not None and throw.t[throw.stop] >= throw.quiet:
                if throw.in_throw(throw.stop)[1] is None:
                    unlearnt += 1
                for way in ways:
                    value = throw.first_rest - throw.forecast(throw.stop, way, carried)
                    at_fine[way].append(value)
                    if throw.fine_asked[throw.stop - 1] < CREEP_RPS:
                        at_creep[way].append(value)
                    if carried is not None and way == "stored":
                        at_fine["stored, same throws"].append(value)
                        if throw.fine_asked[throw.stop - 1] < CREEP_RPS:
                            at_creep["stored, same throws"].append(value)
            if throw.kf_hind is not None and throw.coarse:
                c = int(numpy.argmax(throw.coarse_asked == 0.0))
                truth = throw.truth(c, throw.kf_hind)
                for way in ways:
                    at_coarse[way].append(throw.forecast(c, way, carried) - truth)
                if carried is not None:
                    at_coarse["stored, same throws"].append(throw.forecast(c, "stored") - truth)
                seen.append((throw.kc_hind, throw.kf_hind))
    print(f"{len(replayed)} throws replayed; the unit's own forecast made again within rms "
          f"{statistics.median(replayed):.4f} gr (median), {max(replayed):.4f} at most")
    if skipped:
        print(f"{len(skipped)} series with no calibration echo, left out")
    print("\nAT THE FINE TUBE'S STOP, THE COARSE TUBE QUIET: WHERE THE THROW RESTED, LESS THE FORECAST (gr)")
    for way in WAYS + SAME:
        print(f"  {way:9s} {spread(at_fine[way])}")
    print(f"  in-throw: {unlearnt} stops came before the fine tube's own weight a turn was known (the slope's)")
    print(f"\n  stops from the creep (under {CREEP_RPS} rev/s):")
    for way in WAYS + SAME:
        print(f"  {way:9s} {spread(at_creep[way])}")
    print("\nAT THE COARSE TUBE'S STOP: THE FORECAST, LESS THE TRUTH (gr)")
    for way in WAYS + SAME:
        print(f"  {way:9s} {spread(at_coarse[way])}")


if __name__ == "__main__":
    main()
