"""What a finish could reach with this powder, from the trial's own measurements and no model of the powder.

Taken from the kept throws: what each small move gave (by its size), and where the stream's stop left the first
rest. A finish is then played on those: the first rest drawn from the stops (moved to an aim), and from any rest
the best choice of "stop here" or "one more move of this size", each move's gain drawn from the moves measured.
The best choice is found backwards from the last try. It says what share of throws could end within the band, at
which aim, and with how many moves: a ceiling for this powder in this tube on this scale.

Each draw is taken as independent of the last, which powder gathering at a tube's lip is not: an estimate.

Usage, with the repository's venv, on one trial's folders or several (`bench/trial.py`'s, kept in `recordings/`):
    python best_finish.py ..\\recordings\\20261121_160810_trial_pico2w_ar2209 ..\\recordings\\20261121_162403_trial_pico2w_ar2209
"""
import json
import statistics
import sys
from functools import lru_cache
from pathlib import Path

digit = 0.02
folders = [Path(arg) for arg in sys.argv[1:]]
gains = {}      # a move's size: every gain measured, in digits
firsts = {}     # a finish: its first rests against its own mean, in digits
for folder in folders:
    for file in sorted(folder.glob("throws_*.json")):
        kept = json.loads(file.read_text(encoding="utf-8"))
        sizes = [move["turns"] for move in kept["calibration"]["moves"]]
        for throw in kept["throws"]:
            rows = throw["rows"]
            if len(rows[0]) <= 5:
                continue        # a throw of the tool before it counted the unit's steps
            firsts.setdefault(kept["finish"], []).append(round(throw["first_error_gr"] / digit))
            index = 0
            while index < len(rows) - 1:
                if rows[index][3] == 0.2 and rows[index][2] == 0.0:
                    before = rows[index][1]
                    turn = rows[index + 1][6] - rows[index][6]
                    later = index + 1
                    while later < len(rows) - 1 and rows[later][3] != 0.2:
                        later += 1
                    after = rows[later][1] if rows[later][3] == 0.2 else rows[-1][1]
                    size = ('smallest', 'middle', 'largest')[min(range(len(sizes)), key=lambda i: abs(sizes[i] - turn))]
                    gains.setdefault(size, []).append(max(0, round((after - before) / digit)))
                    index = later
                else:
                    index += 1

sizes = [size for size in ('smallest', 'middle', 'largest') if len(gains.get(size, [])) >= 20]
for size in sizes:
    made = gains[size]
    print(f"the {size} moves: {len(made)} measured; nothing in {sum(1 for g in made if g == 0) / len(made):.0%}, "
          f"mean {statistics.fmean(made):.2f} digits, four and more in {sum(1 for g in made if g >= 4) / len(made):.0%}")
TRIES = 8


def finish(band: int, wait_s: dict):
    @lru_cache(maxsize=None)
    def best(short: int, tries: int) -> tuple:
        """(chance of ending within the band, moves still to come on average) from a rest `short` digits under."""
        if short < -band:
            return 0.0, 0.0             # over: lost
        stay = (1.0 if short <= band else 0.0, 0.0)
        if tries == 0 or short <= -band:
            return stay
        options = [stay]
        for size in sizes:
            made = gains[size]
            good = moves = 0.0
            for gain in made:
                g, m = best(short - gain, tries - 1)
                good += g / len(made)
                moves += m / len(made)
            options.append((good, 1.0 + moves))
        # the better chance; of two alike, the fewer moves
        return max(options, key=lambda option: (round(option[0], 6), -option[1]))
    return best


for band in (1, 2):
    best = finish(band, {})
    print(f"\nwithin {band * digit:.2f} gr: from a rest so many digits short, the chance a throw can still end within the band "
          f"(and the moves it takes)")
    print("   " + "  ".join(f"{short}: {best(short, TRIES)[0]:.0%} ({best(short, TRIES)[1]:.1f})" for short in range(-band, 13)))
    for name, rests in sorted(firsts.items()):
        if len(rests) < 15:
            continue
        mean = statistics.fmean(rests)
        spread = statistics.pstdev(rests)
        rows = []
        for aim in range(-10, 2):
            good = moves = at_first = over_first = 0.0
            for rest in rests:
                short = -round(rest - mean + aim)
                g, m = best(short, TRIES)
                good += g / len(rests)
                moves += m / len(rests)
                at_first += (1.0 if -band <= short <= band else 0.0) / len(rests)
                over_first += (1.0 if short < -band else 0.0) / len(rests)
            rows.append((aim, good, moves, at_first, over_first))
        top = max(rows, key=lambda row: row[1])
        print(f"   the {name} stop ({len(rests)} first rests, spread {spread:.1f} digits), by where it is aimed:")
        print("      " + "  ".join(f"{aim:+d}: {good:.0%}/{moves:.1f}" for aim, good, moves, _, _ in rows))
        print(f"      best aimed {top[0]:+d} digits: {top[1]:.0%} good with {top[2]:.1f} moves a throw; {top[3]:.0%} within "
              f"at the first rest, {top[4]:.0%} over there")
