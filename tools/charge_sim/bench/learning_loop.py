"""The stop's learning alone, played on a rig's own stops (ADR-009, 2026-11-22).

A throw's first rest lies where its stop room put it, plus what that stop's powder did: first error = c - room +
noise. The noise is the rig's own, from series' throws (each first error plus the stop room that throw used, less
their mean), drawn at random. Sessions of throws are played with each learning rule from a placed stop, and with
the stop placed some digits wrong, to see how fast a rule mends a wrong calibration and how far it wanders on a
right one. Rules: "share", the controller's (`staged.Staged._learn`: 15% of each first error, a dead zone, a cap);
"mean", proposed on 2026-11-22 and rejected by this tool (the mean of the stop rooms that would have put each rest on
the aim, weighed against the placed stop): it wandered a third less and mended a stop placed two digits wrong in
more than 25 throws where the controller's mends it in 13 to 15.

    ..\\..\\..\\venv\\Scripts\\python learning_loop.py ..\\recordings\\*_series_glide00*_d_yields_* ..\\recordings\\*_series_glide004_e_*
"""

import argparse
import json
import random
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import staged     # noqa: E402

DIGIT = 0.02
AIM = -0.02
PLACED_DIGITS = 1.0             # the mean rule: the stop as placed taken to be right to a digit


def noise_pool(folders: list) -> list:
    pool = []
    for folder in folders:
        series = json.loads((folder / "series.json").read_text(encoding="utf-8"))
        throws = series["throws"]
        room = None
        for throw in throws:
            if room is not None and throw.get("streams", 0) == 0:
                pool.append(throw["first"] + room)
            room = throw["stop_room"]       # after this throw's learning: the next one's
    mean = statistics.fmean(pool)
    return [value - mean for value in pool]


class Rule:
    def __init__(self, learning: str, placed_room: float, prior: float):
        self.learning, self.placed, self.prior = learning, placed_room, prior
        self.room, self.n, self.sum = placed_room, 0, 0.0

    def learn(self, first: float) -> None:
        off = (first - AIM) / DIGIT
        if self.learning == "share":
            if abs(off) < staged.LEARN_DEAD_DIGITS:
                return
            off = max(-staged.LEARN_MOST_DIGITS, min(staged.LEARN_MOST_DIGITS, off))
            self.room += staged.LEARN_SHARE * off * DIGIT
            return
        off = max(-staged.LEARN_MOST_DIGITS, min(staged.LEARN_MOST_DIGITS, off))
        self.n += 1
        self.sum += self.room + off * DIGIT        # the room that would have put this rest on the aim
        self.room = (self.prior * self.placed + self.sum) / (self.prior + self.n)


def play(pool: list, learning: str, band: float, wrong_digits: float, prior: float, sessions: int, throws: int,
         seed: int) -> dict:
    rng = random.Random(seed)
    right_room = 0.034          # where the stop puts the mean first rest at the aim
    over = good = short = 0
    rooms_late = []
    mended_at = []
    for _ in range(sessions):
        rule = Rule(learning, right_room + wrong_digits * DIGIT, prior)
        mended = None
        for i in range(throws):
            first = AIM + (right_room - rule.room) + rng.choice(pool)
            first = round(first / DIGIT) * DIGIT       # a rest lies on the scale's digits
            if first > band + DIGIT / 4:
                over += 1
            elif first >= -band - DIGIT / 4:
                good += 1
            else:
                short += 1
            rule.learn(first)
            if mended is None and abs(rule.room - right_room) <= DIGIT / 2:
                mended = i + 1
            if i >= throws // 2:
                rooms_late.append(rule.room - right_room)
        mended_at.append(mended if mended is not None else throws + 1)
    total = sessions * throws
    return {"over": over / total, "good": good / total, "short": short / total,
            "room_sd": statistics.pstdev(rooms_late), "room_bias": statistics.fmean(rooms_late),
            "mended": statistics.median(mended_at)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", type=Path)
    parser.add_argument("--sessions", type=int, default=2000)
    parser.add_argument("--throws", type=int, default=25)
    args = parser.parse_args()
    pool = noise_pool(args.folders)
    prior = max(1.0, (statistics.stdev(pool) / (PLACED_DIGITS * DIGIT)) ** 2)
    print(f"the rig's stop noise: {len(pool)} throws, sd {statistics.stdev(pool):.4f} gr; the mean rule's prior "
          f"{prior:.2f} throws\n")
    print("band  placed     rule    first rest over  in band  short  | stop room, 2nd half: bias    sd   | mended by throw")
    for band in (0.02, 0.04):
        for wrong in (0.0, -2.0, 2.0):
            for learning in ("share", "mean"):
                r = play(pool, learning, band, wrong, prior, args.sessions, args.throws, 1)
                print(f"{band:.2f}  {wrong:+.0f} digits  {learning:6s} {r['over'] * 100:14.1f}% {r['good'] * 100:7.1f}% "
                      f"{r['short'] * 100:6.1f}%  | {r['room_bias']:+22.4f} {r['room_sd']:6.4f} | {r['mended']:5.0f}")


if __name__ == "__main__":
    main()
