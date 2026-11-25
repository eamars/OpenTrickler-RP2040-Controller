"""ADR-009 step 2 on the bench: real throws read back from the unit's console, window by window (gates 3 and 4).

Reads the logs `rc_run.py` keeps (`logs/rc/*.log`) of a charge with real throws, and prints each tuner window as gate 4
asks: the gains it was thrown on, its median time to the stop, its shares good, over and under, the final weight's mean
offset and SD, a histogram in 0.02 steps, its time per good charge (its median time over its share within the
tolerance, counted), and what the tuner did. The firmware's lines it reads (charge_mode.cpp):

    Charge: throw 5 on fine kp 2.0000 kd 10.0, weight a turn carried 0.7520
    Charge: throw 5 at rest, 10.020 of 10.000 in 4.61 s
    Charge: carried weight a turn 0.7498
    Tuner: kept, kp 1.3333 kd 10.0; next 0.8889 10.0, kept 1.3333 10.0
    Tuner: kp 1.3333 kd 10.00 not kept, <why>

Each `Tuner:` line closes a window; throws after the last one are an open window. Ends with the mechanics' verdict
(gate 3): every throw started with a weight a turn carried (the forecast in play: on a calibrated powder the first
throw too, from step 1's), every gains kept were stored. The direction (gate 4) is reported, not judged: the owner
judges it (2026-11-26, no target).

    venv\\Scripts\\python tools\\charge_sim\\bench\\step2_windows.py --tolerance 0.03 logs\\rc\\<session>.log ...
"""

import argparse
import math
import re
import statistics
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import step2_tuner  # noqa: E402  (tools/charge_sim/step2_tuner.py: the tuner's own judge)

STARTED = re.compile(r"Charge: throw (\d+) on fine kp ([\d.]+)(?: kd ([\d.]+))?, weight a turn "
                     r"(carried|its own, none yet) ([\d.]+)")
AT_REST = re.compile(r"Charge: throw (\d+) at rest, (-?[\d.]+) of ([\d.]+) in ([\d.]+) s")
TUNER = re.compile(r"Tuner: ([a-z ,]+), kp ([\d.]+)(?: kd ([\d.]+))?; next ([\d.]+)(?: ([\d.]+))?, kept ([\d.]+)"
                   r"(?: ([\d.]+))?")
NOT_KEPT = re.compile(r"Tuner: (kp [\d.]+(?: kd [\d.]+)?) not kept, (.*)")
AFRESH = re.compile(r"Charge: the tuner begins afresh from kp ([\d.]+)")
BIN = 0.02


def number(text) -> float:
    return float(text) if text else math.nan


@dataclass
class Throw:
    number: int
    kp: float
    kd: float
    carried: bool
    error: float = math.nan
    time_s: float = math.nan


@dataclass
class Window:
    throws: list = field(default_factory=list)
    step: str = "open"
    next_kp: float = math.nan
    next_kd: float = math.nan
    kept_kp: float = math.nan
    kept_kd: float = math.nan


def read(paths: list) -> tuple:
    """The windows in the logs' order, and the faults the mechanics' verdict counts."""
    windows, faults = [Window()], []
    started = {}
    for path in paths:
        for text in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
            if m := STARTED.search(text):
                started[int(m[1])] = Throw(int(m[1]), float(m[2]), number(m[3]), m[4] == "carried")
            elif m := AT_REST.search(text):
                throw = started.pop(int(m[1]), None)
                if throw is None:
                    faults.append(f"throw {m[1]} at rest with no start in the logs")
                    continue
                throw.error, throw.time_s = float(m[2]) - float(m[3]), float(m[4])
                windows[-1].throws.append(throw)
            elif m := TUNER.search(text):
                window = windows[-1]
                window.step, window.next_kp, window.next_kd = m[1], float(m[4]), number(m[5])
                window.kept_kp, window.kept_kd = float(m[6]), number(m[7])
                windows.append(Window())
            elif m := NOT_KEPT.search(text):
                faults.append(f"{m[1]} not stored: {m[2]}")
            elif m := AFRESH.search(text):
                if windows[-1].throws:
                    windows[-1].step = "cut by a fresh start"
                    windows.append(Window())
    return [w for w in windows if w.throws], faults


def histogram(errors: list) -> str:
    """Counts by 0.02 steps of the final weight less the target, from the lowest step to the highest."""
    bins = [round(e / BIN) for e in errors]
    low, high = min(bins), max(bins)
    return " ".join(f"{b * BIN:+.2f}:{bins.count(b)}" for b in range(low, high + 1) if bins.count(b))


def report(window: Window, n: int, tolerance: float) -> float:
    errors = [t.error for t in window.throws]
    times = [t.time_s for t in window.throws]
    good = sum(abs(e) < tolerance for e in errors) / len(errors)
    over = sum(e >= tolerance for e in errors) / len(errors)
    under = sum(e <= -tolerance for e in errors) / len(errors)
    median = statistics.median(times)
    cost = median / good if good else math.inf
    modelled = step2_tuner.cost(errors, times, tolerance, step2_tuner.modelled)[0]
    gains = sorted({(t.kp, t.kd) for t in window.throws})
    print(f"window {n}: {len(errors)} throws on kp/kd {', '.join(f'{kp:.4f}/{kd:.1f}' for kp, kd in gains)}; "
          f"{window.step}" + (f", next {window.next_kp:.4f}/{window.next_kd:.1f}, kept "
                              f"{window.kept_kp:.4f}/{window.kept_kd:.1f}" if window.step != "open" else ""))
    print(f"  {cost:6.1f} s a good charge (the tuner's, its share modelled: {modelled:.1f} s)   good {good:4.0%}  "
          f"over {over:4.0%}  under {under:4.0%}   mean {statistics.fmean(errors):+.3f}  "
          f"sd {statistics.pstdev(errors):.3f}   time {median:.2f} s")
    print(f"  {histogram(errors)}")
    return cost


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("logs", nargs="+", help="rc_run.py's session logs, in the order they ran")
    parser.add_argument("--tolerance", type=float, required=True, help="the unit's fine threshold (its charge settings)")
    args = parser.parse_args()
    windows, faults = read(args.logs)
    if not windows:
        print("VERDICT FAIL no throw at rest in the logs")
        return 1
    costs = [report(w, n + 1, args.tolerance) for n, w in enumerate(windows)]
    throws = [t for w in windows for t in w.throws]
    blind = [t.number for t in throws if not t.carried]
    kept = [(w.kept_kp, w.kept_kd) for w in windows if w.step != "open"]
    print(f"direction: a good charge {' -> '.join(f'{c:.1f}' for c in costs)} s; "
          f"the kept kp/kd {' -> '.join(f'{kp:.4f}/{kd:.1f}' for kp, kd in kept) or 'none yet'}")
    if blind:
        faults.append(f"throws with no weight a turn carried (no forecast): {blind}")
    if faults:
        print("VERDICT FAIL " + "; ".join(faults))
        return 1
    print(f"VERDICT PASS {len(throws)} throws, each on a carried weight a turn; "
          f"{len(kept)} windows closed, every gains kept stored")
    return 0


if __name__ == "__main__":
    sys.exit(main())
