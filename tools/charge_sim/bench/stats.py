"""The figures a recording is summed up in: the same code for the bench's recordings and the simulation's.

A recording is a list of trials as `record.py` saves them (`runs.json`). The simulation's twin of each bench test
(`bench_twin.py`) makes trials of the same shape, so a figure measured and a figure simulated are computed alike
and can be set side by side (`fit_report.py`).
"""

import bisect
import statistics

DIGIT_GR = 0.02
FINE_STOP_THRESHOLD_GR = 0.03
RISE_MARKS_S = (0.4, 0.8, 1.2, 2.0)
START_SKIP_S = 2.0      # a turn's first seconds are its start, not its steady flow
FOLD_BINS = 10
FOLD_LEAST_S = 30.0     # a run shorter than this is not folded by angle
RIFLING_STARTS = 5      # both of the project's tubes
SINGLE_LATE_S = 1.5     # a single kernel first shown later than this after its nudge is a late one


def reading_at(series: list, t: float) -> float:
    """The last reading at or before a time; the first one when the time is before them all."""
    index = bisect.bisect_right(series, t, key=lambda sample: sample[0]) - 1
    return series[max(index, 0)][1]


def spread(values: list) -> float:
    """The population standard deviation, 0 for fewer than two values."""
    return statistics.pstdev(values) if len(values) > 1 else 0.0


def end_reading(trial: dict) -> float:
    """A creep-stop trial's reading at rest: the median of its last 0.6 s."""
    last = trial["after"][-1][0]
    return statistics.median(weight for t, weight in trial["after"] if t >= last - 0.6)


def window_gains(trial: dict, window_s: float) -> list:
    """Digits gained in each whole window of the steady part of a trial's turn, counted back from its stop."""
    gains = []
    end = 0.0
    while end - window_s >= -trial["run_s"] + START_SKIP_S:
        gains.append((reading_at(trial["during"], end) - reading_at(trial["during"], end - window_s)) / DIGIT_GR)
        end -= window_s
    return gains


def fold(trial: dict, period_turns: float) -> list:
    """Digits gained in each of FOLD_BINS equal parts of a period of the tube's turn, summed over the steady part."""
    bin_s = period_turns / FOLD_BINS / trial["speed_asked_rps"]
    start = -trial["run_s"] + START_SKIP_S
    sums = [0.0] * FOLD_BINS
    index = 0
    while start + bin_s <= 0.0:
        gain = reading_at(trial["during"], start + bin_s) - reading_at(trial["during"], start)
        sums[index % FOLD_BINS] += gain / DIGIT_GR
        start += bin_s
        index += 1
    return sums


def fuller_half(sums: list):
    """The largest share of the flow that any half of the period (consecutive bins, wrapping) holds; None for no flow."""
    total = sum(sums)
    if total <= 0.0:
        return None
    half = FOLD_BINS // 2
    return max(sum(sums[(first + offset) % FOLD_BINS] for offset in range(half)) for first in range(FOLD_BINS)) / total


def fold_figure(trials: list, period_turns: float):
    """The mean fuller half of the long trials' flow folded onto a period; 0.5 is a flow that ignores the angle."""
    shares = [fuller_half(fold(trial, period_turns)) for trial in trials if trial["run_s"] >= FOLD_LEAST_S]
    shares = [share for share in shares if share is not None]
    return statistics.fmean(shares) if shares else None


def creep_stats(trials: list) -> dict:
    """
    One kind of creep-stop trial (a tube, a speed, a kind of stop) as figures.

    Args:
        trials: the kind's trials, each with `run_s`, `during` and `after` as (seconds from the stop, reading).

    Returns:
        stops; rate (digits a second, steady); start_1s and start_2s (digits gained in the turn's first one and
        two seconds); fano_1s, fano_2s, fano_4s (variance over mean of the digits a window; None with under eight
        windows); most_1s (the largest gain in a second); rise and rise_sd (digits after the stop); rise_by (the
        mean rise by each of RISE_MARKS_S); three_hold (the largest share of stops whose rise three digits hold);
        rises (every stop's, rounded).
    """
    rates, first, second, rises = [], [], [], []
    by_mark = {mark: [] for mark in RISE_MARKS_S}
    windows = {1.0: [], 2.0: [], 4.0: []}
    for trial in trials:
        during, start = trial["during"], -trial["run_s"]
        at_stop = during[-1][1]
        if trial["run_s"] > START_SKIP_S + 1.0:
            rates.append((at_stop - reading_at(during, start + START_SKIP_S)) / (trial["run_s"] - START_SKIP_S)
                         / DIGIT_GR)
        first.append((reading_at(during, start + 1.0) - during[0][1]) / DIGIT_GR)
        second.append((reading_at(during, start + 2.0) - during[0][1]) / DIGIT_GR)
        rises.append((end_reading(trial) - at_stop) / DIGIT_GR)
        for mark in RISE_MARKS_S:
            by_mark[mark].append((reading_at(trial["after"], mark) - at_stop) / DIGIT_GR)
        for window_s, gains in windows.items():
            gains.extend(window_gains(trial, window_s))
    rounded = [round(rise) for rise in rises]
    best = max(sum(1 for rise in rounded if low <= rise <= low + 2) for low in range(min(rounded), max(rounded) + 1))
    out = {"stops": len(trials), "rate": statistics.fmean(rates) if rates else None,
           "start_1s": statistics.fmean(first), "start_2s": statistics.fmean(second),
           "most_1s": max(windows[1.0]) if windows[1.0] else None,
           "rise": statistics.fmean(rises), "rise_sd": spread(rises),
           "rise_by": {str(mark): statistics.fmean(values) for mark, values in by_mark.items()},
           "three_hold": best / len(rounded), "rises": sorted(rounded),
           "fold_turn": fold_figure(trials, 1.0), "fold_rifling": fold_figure(trials, 1.0 / RIFLING_STARTS)}
    for window_s, gains in windows.items():
        mean = statistics.fmean(gains) if gains else 0.0
        out[f"fano_{window_s:g}s"] = statistics.pvariance(gains) / mean if len(gains) >= 8 and mean > 0.0 else None
    return out


def ramp_stats(trials: list) -> dict:
    """
    Trials of one speed plan with no controller (`record.py ramp`), stopped at its end, as figures.

    Args:
        trials: each with `during` as (seconds from the stop, reading, speed) and `after` as (seconds, reading).

    Returns:
        ramps; gain and gain_sd (digits from the plan's start to the reading at rest); rise and rise_sd (digits
        after the stop); rise_by (the mean rise by each of RISE_MARKS_S).
    """
    gains = [(end_reading(trial) - trial["during"][0][1]) / DIGIT_GR for trial in trials]
    rises = [(end_reading(trial) - trial["during"][-1][1]) / DIGIT_GR for trial in trials]
    by_mark = {str(mark): statistics.fmean((reading_at(trial["after"], mark) - trial["during"][-1][1]) / DIGIT_GR
                                           for trial in trials) for mark in RISE_MARKS_S}
    return {"ramps": len(trials), "gain": statistics.fmean(gains), "gain_sd": spread(gains),
            "rise": statistics.fmean(rises), "rise_sd": spread(rises), "rise_by": by_mark}


def throw_result(error_gr: float) -> str:
    """Over, under or good, by the unit's own rule at its default stop threshold."""
    if error_gr >= FINE_STOP_THRESHOLD_GR - 1e-6:
        return "over"
    if error_gr <= -FINE_STOP_THRESHOLD_GR + 1e-6:
        return "under"
    return "good"


def throw_stats(throws: list) -> dict:
    """
    Throws of one target and one set of settings as figures.

    Args:
        throws: each with `target`, `time_s`, `coarse_s`, `at_stop` and `at_rest` (readings; `at_rest` None when
            the cup was lifted before the watch ended).

    Returns:
        throws; time and time_sd (seconds from the charge's start to its stop); coarse_s; at_stop (the reading at
        the stop less the target); at_rest and at_rest_sd (the reading at rest less the target); rise and rise_sd
        (digits from the stop to rest); over and under (shares, at rest).
    """
    rested = [throw for throw in throws if throw["at_rest"] is not None]
    errors = [throw["at_rest"] - throw["target"] for throw in rested]
    rises = [(throw["at_rest"] - throw["at_stop"]) / DIGIT_GR for throw in rested]
    results = [throw_result(error) for error in errors]
    times = [throw["time_s"] for throw in rested]
    return {"throws": len(rested), "time": statistics.fmean(times), "time_sd": spread(times),
            "coarse_s": statistics.fmean(throw["coarse_s"] for throw in rested),
            "at_stop": statistics.fmean(throw["at_stop"] - throw["target"] for throw in rested),
            "at_rest": statistics.fmean(errors), "at_rest_sd": spread(errors),
            "rise": statistics.fmean(rises), "rise_sd": spread(rises),
            "over": results.count("over") / len(results), "under": results.count("under") / len(results)}


def nudge_stats(gains_kernels: list, first_moves_s: list) -> dict:
    """
    A nudge series as figures.

    Args:
        gains_kernels: each nudge's gain in kernels.
        first_moves_s: for each nudge that released, (kernels, seconds from the nudge until the reading first moved).

    Returns:
        nudges; kernels (the mean a nudge); fano (variance over mean of the gains); nothing, one, two, three_up
        (shares of the nudges); first_single_s and first_many_s (the median time to the reading's first move, for
        releases of one kernel and of three or more); single_late (the share of single kernels whose first move
        came later than SINGLE_LATE_S: how slowly the scale takes a small change).
    """
    counts = [round(gain) for gain in gains_kernels]
    total = len(counts)
    mean = statistics.fmean(gains_kernels)
    single = [seconds for kernels, seconds in first_moves_s if round(kernels) == 1]
    many = [seconds for kernels, seconds in first_moves_s if round(kernels) >= 3]
    return {"nudges": total, "kernels": mean, "fano": statistics.pvariance(gains_kernels) / mean,
            "nothing": sum(1 for count in counts if count <= 0) / total,
            "one": counts.count(1) / total, "two": counts.count(2) / total,
            "three_up": sum(1 for count in counts if count >= 3) / total,
            "first_single_s": statistics.median(single) if single else None,
            "single_late": sum(1 for seconds in single if seconds > SINGLE_LATE_S) / len(single) if single else None,
            "first_many_s": statistics.median(many) if many else None}
