"""An ABBA's four arms judged against the night's bars (ADR-009, F.5's pass of 2026-11-24, 01:51).

Per 20 throws, A (the controller) against B (the glide's pour): settled good at most 2 fewer, overs at most 1 more,
the median at most 2.5 s longer. A throw is good within the band and a millionth; its settled weight is the next
throw's base less its own (the last throw's, the cup after the series). For information, not the pass: each arm
against gate 2's bar (within the tolerance at least 68% good, the median at most 13 s), and each verdict against its
settled weight: whether it still held. With --distributions, for each side: its first rests and its final weights
counted by 0.02 gr from the target (-0.10 to +0.14, the ends holding what lies beyond), the first rests' mean and
spread, and each throw topped up or sent back to the stream, first rest to final, with its moves and streams. A first
rest is the reading at rest after the throw's first fine stop, from its recording (the line's `first` is the rest after
any return to the stream); in grains from the target, as the owner reads them.

Usage, with the repository's venv:
    python abba_judge.py --band 0.02 --tolerance 0.03 <A1 folder> <B1 folder> <B2 folder> <A2 folder>
"""

import argparse
import json
import statistics
import sys
from pathlib import Path


def klass(error: float, band: float) -> str:
    return "good" if abs(error) <= band + 1e-6 else "over" if error > 0 else "short"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--band", type=float, default=0.02)
    parser.add_argument("--tolerance", type=float, default=0.03)
    parser.add_argument("--distributions", action="store_true")
    parser.add_argument("folders", nargs=4, type=Path)
    args = parser.parse_args()
    arms = {}
    for name, folder in zip(("A1", "B1", "B2", "A2"), args.folders):
        record = json.loads((folder / "series.json").read_text())
        arms[name] = (record["target_gr"], record["throws"])
    if args.distributions:
        distributions(arms)
    sides = {"A": [], "B": []}
    for name, (target, throws) in arms.items():
        rows = []
        for t in throws:
            error = t["rest"] - target
            settled = t.get("settled")
            settled_error = settled - target if isinstance(settled, float) else None
            rows.append((klass(error, args.band), error, t["elapsed"],
                         klass(settled_error, args.band) if settled_error is not None else None, settled_error))
        sides[name[0]] += rows
        good = sum(r[0] == "good" for r in rows)
        within_tol = sum(abs(r[1]) <= args.tolerance - 1e-6 or abs(r[1]) <= args.band + 1e-6 for r in rows)
        median = statistics.median(r[2] for r in rows)
        held = sum(r[3] == r[0] for r in rows if r[3] is not None)
        moved = [f"{r[1]:+.2f}->{r[4]:+.2f}" for r in rows if r[3] is not None and r[3] != r[0]]
        print(f"{name}: good {good}, over {sum(r[0] == 'over' for r in rows)}, short {sum(r[0] == 'short' for r in rows)}"
              f" of {len(rows)}; settled good {sum(r[3] == 'good' for r in rows)}; median {median:.1f} s; gate 2's "
              f"bar (68% within the band, median 13 s): {'met' if good >= 0.68 * len(rows) and median <= 13 else 'not met'}; "
              f"verdicts held {held} of {len(rows)} {moved}")
    a, b = sides["A"], sides["B"]
    a_settled = sum(r[3] == "good" for r in a)
    b_settled = sum(r[3] == "good" for r in b)
    a_over = sum(r[0] == "over" for r in a)
    b_over = sum(r[0] == "over" for r in b)
    a_median = statistics.median(r[2] for r in a)
    b_median = statistics.median(r[2] for r in b)
    bars = [("settled good at most 2 fewer", a_settled >= b_settled - 2, f"A {a_settled}, B {b_settled}"),
            ("overs at most 1 more", a_over <= b_over + 1, f"A {a_over}, B {b_over}"),
            ("the median at most 2.5 s longer", a_median <= b_median + 2.5, f"A {a_median:.1f} s, B {b_median:.1f} s")]
    for words, met, figures in bars:
        print(f"bar, {words}: {figures}: {'met' if met else 'NOT MET'}")
    passed = all(met for _, met, _ in bars)
    print("VERDICT PASS" if passed else "VERDICT FAIL")
    return 0 if passed else 1


def first_rest(throw: dict, target: float):
    """The reading at rest after the first fine stop, from the throw's recording, in grains from the target."""
    recording = throw.get("recording") or {}
    rows = (recording.get("samples") or {}).get("rows")
    marks = (recording.get("entry") or {}).get("marks") or []
    stop = next((m for m in marks if m["kind"] == "fine_stop"), None)
    if not rows or stop is None:
        return None
    end = len(rows) - 1
    for k in range(stop["row"] + 1, len(rows)):
        if rows[k][3] > 0 or rows[k][4] > 0:
            end = k - 1
            break
    return rows[end][2] - target


BINS = [round(-0.10 + 0.02 * i, 2) for i in range(13)]     # -0.10 to +0.14


def binned(values: list) -> str:
    counts = {b: 0 for b in BINS}
    for v in values:
        b = min(BINS, key=lambda c: abs(c - v))
        counts[b] += 1
    return " ".join(f"{b:+.2f}:{counts[b]}" for b in BINS)


def distributions(arms: dict) -> None:
    for side in ("A", "B"):
        firsts, finals, changed = [], [], []
        for name, (target, throws) in arms.items():
            if name[0] != side:
                continue
            for i, t in enumerate(throws):
                first = first_rest(t, target)
                final = t["rest"] - target
                finals.append(final)
                if first is not None:
                    firsts.append(first)
                if int(t["moves"]) or int(t["streams"]):
                    shown = f"{first:+.2f}" if first is not None else "?"
                    changed.append(f"{name} {i + 1}: {shown} -> {final:+.2f} gr, {int(t['moves'])} moves, "
                                   f"{int(t['streams'])} streams")
        print(f"side {side}: first rests ({len(firsts)}) {binned(firsts)}")
        print(f"side {side}: finals      ({len(finals)}) {binned(finals)}")
        if len(firsts) > 1:
            print(f"side {side}: first rests mean {statistics.fmean(firsts):+.3f} gr, spread (SD) "
                  f"{statistics.stdev(firsts):.3f} gr")
        for line in changed:
            print(f"  {line}")


if __name__ == "__main__":
    sys.exit(main())
