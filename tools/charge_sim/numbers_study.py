"""Gate N's sensitivity study (ADR-009, numbers.md): each rule's number at half and twice its value, the rest as built.

The controller is the one ADR-009 delivers: the PID law in the staged frame, the envelope on, the owner's fine gains
and the coarse tube's from the calibration (`staged.pid_law_from`, option 2), calibrated afresh on each rig with the
number changed, so the calibration's rules see it too. Every record and every stand-in scale of `scales_like.py`, at
both bands, 10 gr, 800 throws a case.

The pass, written before the run (numbers.md, "The research for each row"): a number is a Rule where, at half and at
twice its value, on every rig and band, the good share and the over share each move by less than the simulation's
noise, and the median by less than 0.5 s. The noise: two standard deviations of the difference between two
independent runs of 800 throws, 2 x sqrt(2 p (1 - p) / 800), p the baseline's share; the baseline is also run on a
second seed, to show it.

    python numbers_study.py [--throws 800] [--rigs ...] [--scales ...] [--only NAME,...] [--out fit/...]

Gate N.3 (`--nothing`): the three fixed numbers that are times or weights chosen on the bench's scale, each set to
nothing, and all three together. Its pass is in numbers.md, "N.3". Each run ends with a tally: for every value
tried, in how many cases the good share, the over share and the median moved past their bars, and the mean moves;
the baseline's second seed is tallied the same way, to show what chance alone does.

    python numbers_study.py --nothing --out fit/study_20261123/nothing.txt
"""

import argparse
import math
from pathlib import Path

import controllers
import mc
import rigs
import scales_like
import staged
from workers import Workers

# numbers.md's row, the constant in staged.py, and the two values tried when it is not halved and doubled
NUMBERS = [
    ("C7", "COARSE_SPREADS", None), ("C8", "CREEP_BAND_SHARE", None), ("C9", "CREEP_MOST_S", None),
    ("C10", "LEARN_SHARE", None), ("C11", "LEARN_MOST_DIGITS", None), ("C12", "LEARN_DEAD_DIGITS", None),
    ("C16", "STILL_LEAST_S", None), ("D1", "SLOWEST_LAG_LEAST_S", None), ("D2", "CREEP_LEAST_SHARE", None),
    ("D3", "CREEP_TOP_STOP", (1, 3)), ("D6", "GLIDE_SPREADS", None), ("D6", "GLIDE_FLOW_TIMES", None),
    ("D7", "GLIDE_LAG_SHARE", None), ("D7", "GLIDE_LEAST_S", None), ("D7", "GLIDE_MOST_S", None),
    ("D8", "COARSE_GLIDE_LEAST_S", None), ("D8", "COARSE_GLIDE_MOST_S", None), ("D9", "COARSE_ROOM_SPREADS", None),
    ("D9", "COARSE_ROOM_EXTRA_GR", None), ("D10", "REST_FLOOR_TIMES", None), ("D10", "REST_EXTRA_S", None),
    ("D12", "COARSE_LEAST_TURNS", None), ("D15", "STILL_INTERVALS", None),
]
# The rows that went at ADR-009's gate F.5 (C16, D1, D3's index, D7's and D8's limits, D9's and D10's extras) are no
# longer in the reference: the study of record ran them (numbers.md); a run now skips them
NUMBERS = [row for row in NUMBERS if hasattr(staged, row[1])]
# Gate N.3: the bench's leftovers at nothing, each alone and all three together
ALL_THREE = "ALL_THREE"
NOTHING = [("C16", "STILL_LEAST_S", (0.0,)), ("D9", "COARSE_ROOM_EXTRA_GR", (0.0,)), ("D10", "REST_EXTRA_S", (0.0,)),
           ("N.3", ALL_THREE, (0.0,))]
NOTHING = [] if not hasattr(staged, "STILL_LEAST_S") else NOTHING      # N.3's three went at F.5
BASELINE = {name: getattr(staged, name) for _, name, _ in NUMBERS}
BANDS = ((0.02, controllers.VERDICT_TOLERANCE_GR), (0.04, controllers.LOOSE_TOLERANCE_GR))
TARGET_GR = 10.0
MEDIAN_BAR_S = 0.5


def values(name: str, given) -> tuple:
    """The two values a number is tried at."""
    if given is not None:
        return given
    return BASELINE[name] * 0.5, BASELINE[name] * 2.0


def _make(name, value, band, tolerance):
    def make(rig, seed=0):
        for constant, baseline in BASELINE.items():      # a worker runs many cases: each sets every number afresh
            setattr(staged, constant, baseline)
        if name == ALL_THREE:
            for _, constant, _ in NOTHING[:-1]:
                setattr(staged, constant, value)
        elif name is not None:
            setattr(staged, name, value)
        cal = staged.calibrate(rig, seed, "glide", band)
        controller = staged.Staged(cal, rig.acceleration_rps2)
        controller.pid = staged.pid_law_from(cal)
        return controller, tolerance
    return make


def case_name(name, value, band) -> str:
    return f"numbers:{name}={value}@{band}"


for _band, _tol in BANDS:
    controllers.CONTROLLERS[case_name(None, None, _band)] = _make(None, None, _band, _tol)
    for _row, _name, _given in NUMBERS + NOTHING:
        for _value in values(_name, _given):
            controllers.CONTROLLERS[case_name(_name, _value, _band)] = _make(_name, _value, _band, _tol)


def noise(p: float, throws: int) -> float:
    """Two SDs of the difference between two independent shares of `throws`."""
    return 2.0 * math.sqrt(2.0 * p * (1.0 - p) / throws)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--throws", type=int, default=800)
    parser.add_argument("--rigs", default="pico2w_ar2209,pico2w_bm2")
    parser.add_argument("--scales", default=",".join(scales_like.SCALES))
    parser.add_argument("--only", default="", help="constants to study, by name (all when empty)")
    parser.add_argument("--nothing", action="store_true", help="gate N.3: the bench's leftovers at nothing")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    only = set(filter(None, args.only.split(",")))
    studied = [n for n in (NOTHING if args.nothing else NUMBERS) if not only or n[1] in only]
    lines = [f"{args.throws} throws a case at {TARGET_GR:.0f} gr; the PID law in the frame (option 2), envelope on; "
             f"pass: good and over within 2 SD of two runs' difference, median within {MEDIAN_BAR_S} s"]
    verdicts = {name: True for _, name, _ in studied}
    worst = {name: "" for _, name, _ in studied}
    tally = {}      # (name, value): [cases, good moved, over moved, median moved, sum good, sum over, sum median]

    def count(key, d_good, d_over, d_med, n_good, n_over, median_bar):
        t = tally.setdefault(key, [0, 0, 0, 0, 0.0, 0.0, 0.0])
        t[0] += 1
        t[1] += abs(d_good) >= n_good and d_good != 0.0     # a bar of nothing (a share of 0 or 100%) is not passed by no move
        t[2] += abs(d_over) >= n_over and d_over != 0.0
        t[3] += abs(d_med) >= median_bar
        t[4] += d_good
        t[5] += d_over
        t[6] += d_med
    with Workers() as pool:
        for rig_name in args.rigs.split(","):
            for scale in args.scales.split(","):
                rig = scales_like.like(rigs.get_rig(rig_name), scale)
                for band, tolerance in BANDS:
                    # Every case of this rig and band in one pass over the workers: the same throws as one by one
                    cases = [(None, None, 1), (None, None, 2)] + [(name, value, 1) for _, name, given in studied
                                                                    for value in values(name, given)]
                    jobs = [mc.session_jobs(rig, case_name(name, value, band), TARGET_GR, args.throws, seed)
                            for name, value, seed in cases]
                    flat = [job for case_jobs in jobs for job in case_jobs]
                    results = pool.map(mc.run_session, flat)
                    done, at = {}, 0
                    for case, case_jobs in zip(cases, jobs):
                        throws = [r for rs in results[at:at + len(case_jobs)] for r in rs]
                        at += len(case_jobs)
                        s = mc.summarise(throws, tolerance)
                        done[case] = (s["good_true"], s["over_true"], s["time_median"])

                    def run(name, value, seed=1):
                        return done[(name, value, seed)]
                    base = run(None, None)
                    again = run(None, None, seed=2)
                    n_good, n_over = noise(base[0], args.throws), noise(base[1], args.throws)
                    median_bar = max(MEDIAN_BAR_S, abs(again[2] - base[2]))  # the seeds' own spread, if more
                    lines.append(f"{rig_name} {scale} band {band}: baseline good {base[0] * 100:.1f}% over "
                                 f"{base[1] * 100:.1f}% median {base[2]:.1f} s; second seed {again[0] * 100:.1f}% "
                                 f"{again[1] * 100:.1f}% {again[2]:.1f} s; noise good {n_good * 100:.1f} over "
                                 f"{n_over * 100:.1f} points, median bar {median_bar:.1f} s")
                    print(lines[-1], flush=True)
                    # The second seed against the first, by the first's own bars (its median bar never under their
                    # difference, so its median is judged against the fixed bar): what chance alone moves
                    count(("second seed", 0.0), again[0] - base[0], again[1] - base[1], again[2] - base[2], n_good,
                          n_over, MEDIAN_BAR_S)
                    for row, name, given in studied:
                        for value in values(name, given):
                            got = run(name, value)
                            d_good, d_over, d_med = got[0] - base[0], got[1] - base[1], got[2] - base[2]
                            ok = abs(d_good) < n_good and abs(d_over) < n_over and abs(d_med) < median_bar
                            count((name, value), d_good, d_over, d_med, n_good, n_over, median_bar)
                            if not ok:
                                verdicts[name] = False
                                worst[name] += (f" [{rig_name} {scale} {band} at {value:g}: good {d_good * 100:+.1f} "
                                                f"over {d_over * 100:+.1f} median {d_med:+.1f} s]")
                            lines.append(f"  {row:4s}{name:22s}{value:8.3g}  good {d_good * 100:+6.1f}  over "
                                         f"{d_over * 100:+6.1f}  median {d_med:+5.1f} s  {'ok' if ok else 'MOVES'}")
                            print(lines[-1], flush=True)
    lines.append("VERDICTS")
    for row, name, _ in studied:
        lines.append(f"{row:4s}{name:22s}{'Rule: insensitive' if verdicts[name] else 'sensitive:' + worst[name]}")
        print(lines[-1], flush=True)
    lines.append("TALLY: cases; in how many good, over and the median moved past their bars; the mean moves")
    for (name, value), t in tally.items():
        lines.append(f"{name:22s}{value:8.3g}  cases {t[0]:2d}  good {t[1]:2d}  over {t[2]:2d}  median {t[3]:2d}  "
                     f"mean good {t[4] / t[0] * 100:+5.2f}  over {t[5] / t[0] * 100:+5.2f} points  "
                     f"median {t[6] / t[0]:+5.2f} s")
        print(lines[-1], flush=True)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
