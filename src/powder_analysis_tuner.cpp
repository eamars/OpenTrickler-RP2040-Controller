#include <string.h>
#include <math.h>
#include "powder_analysis_tuner.h"


// ---------------------------------------------------------------------------------------------
// Bracketing search
// ---------------------------------------------------------------------------------------------
void powder_analysis_search_init(powder_analysis_search_t *search, float start_value) {
    memset(search, 0x0, sizeof(*search));
    search->value = start_value;
}


float powder_analysis_search_best_value(const powder_analysis_search_t *search) {
    if (search->has_met) {
        return search->met_value;       // the "met" value closest to the boundary
    }
    if (search->has_not_met) {
        return search->not_met_value;   // nothing met yet: the value tried last comes closest
    }
    return search->value;               // nothing tried yet: the start value
}


static void finish_search(powder_analysis_search_t *search) {
    search->value = powder_analysis_search_best_value(search);
    search->finished = true;
}


void powder_analysis_search_update(powder_analysis_search_t *search, bool condition_met) {
    if (search->finished) {
        return;
    }

    // Remember the outcome of the value just tried. The search only moves towards the boundary,
    // so the latest value of each side is always the one closest to the boundary.
    if (condition_met) {
        search->has_met = true;
        search->met_value = search->value;
    }
    else {
        search->has_not_met = true;
        search->not_met_value = search->value;
    }

    // Step 1a: nothing met yet -> double (bounded, a value without effect must not double forever)
    if (!search->has_met) {
        if (search->doubling_steps >= POWDER_ANALYSIS_MAX_DOUBLING_STEPS) {
            finish_search(search);
            return;
        }
        search->value *= 2.0f;
        search->doubling_steps += 1;
        return;
    }

    // Step 1b: everything met so far -> halve (bounded as well)
    if (!search->has_not_met) {
        if (search->halving_steps >= POWDER_ANALYSIS_MAX_HALVING_STEPS) {
            finish_search(search);
            return;
        }
        search->value /= 2.0f;
        search->halving_steps += 1;
        return;
    }

    // Step 2: both sides are known -> bisect until the interval is small enough or the step limit is reached
    float interval = (search->met_value - search->not_met_value) / search->met_value;
    if (interval < POWDER_ANALYSIS_BISECTION_MIN_INTERVAL ||
        search->bisection_steps >= POWDER_ANALYSIS_MAX_BISECTION_STEPS) {
        finish_search(search);
        return;
    }
    search->value = (search->met_value + search->not_met_value) / 2.0f;
    search->bisection_steps += 1;
}


// ---------------------------------------------------------------------------------------------
// Phase handling
// ---------------------------------------------------------------------------------------------
// Start value of the two Kp searches (from the start value matrix)
static float phase_start_value(const powder_analysis_tuner_t *tuner, powder_analysis_phase_t phase) {
    return phase == POWDER_ANALYSIS_PHASE_COARSE_KP ? tuner->coarse_kp_start : tuner->fine_kp_start;
}


// Copy the (best) value of the current search into the gain this phase searches for
static void store_search_result(powder_analysis_tuner_t *tuner) {
    float value = powder_analysis_search_best_value(&tuner->search);

    switch (tuner->phase) {
        case POWDER_ANALYSIS_PHASE_COARSE_KP:
            tuner->coarse_kp = value;
            break;
        case POWDER_ANALYSIS_PHASE_FINE_KP:
            tuner->fine_kp = value;
            break;
        default:
            break;
    }
}


// Begin a new verification round with the current values. They stay fixed for the whole round.
static void start_verify_round(powder_analysis_tuner_t *tuner) {
    uint8_t number = tuner->verify_round.number + 1;
    memset(&tuner->verify_round, 0x0, sizeof(tuner->verify_round));
    tuner->verify_round.number = number;
    tuner->verify_round.fine_kp = tuner->fine_kp;
    tuner->verify_round.fine_min_flow_speed_rps = tuner->fine_min_flow_speed_rps;
}


static void start_phase(powder_analysis_tuner_t *tuner, powder_analysis_phase_t phase) {
    tuner->phase = phase;

    if (phase == POWDER_ANALYSIS_PHASE_VERIFY) {
        // Verified with the fine Kp as found and the fine Kd from the matrix
        tuner->verify_reached = true;
        start_verify_round(tuner);
    }
    else if (phase == POWDER_ANALYSIS_PHASE_COARSE_KD) {
        tuner->coarse_kd_count = 0;     // probing, no bracketing search (see update_coarse_kd_phase)
        tuner->coarse_kd_retries = 0;
    }
    else if (phase == POWDER_ANALYSIS_PHASE_COARSE_KP || phase == POWDER_ANALYSIS_PHASE_FINE_KP) {
        // Smallest Kp meeting the condition of search_condition_met()
        powder_analysis_search_init(&tuner->search, phase_start_value(tuner, phase));
    }
}


// ---------------------------------------------------------------------------------------------
// Verification bookkeeping
// ---------------------------------------------------------------------------------------------
static float verify_round_average_hit_time(const powder_analysis_verify_round_t *round) {
    if (round->hit_count == 0) {
        return 0.0f;
    }
    return round->hit_time_sum_s / round->hit_count;
}


// "Best" = most hits, on a tie the faster one
static bool verify_round_is_better(const powder_analysis_verify_round_t *candidate, const powder_analysis_verify_round_t *best) {
    if (candidate->throw_count == 0) {
        return false;       // nothing was measured in the candidate round
    }
    if (best->throw_count == 0) {
        return true;        // no best yet
    }
    if (candidate->hit_count != best->hit_count) {
        return candidate->hit_count > best->hit_count;
    }
    return verify_round_average_hit_time(candidate) < verify_round_average_hit_time(best);
}


static void update_verify_best_round(powder_analysis_tuner_t *tuner) {
    if (verify_round_is_better(&tuner->verify_round, &tuner->verify_best_round)) {
        tuner->verify_best_round = tuner->verify_round;
    }
}


// ---------------------------------------------------------------------------------------------
// Coarse helpers
// ---------------------------------------------------------------------------------------------
// Same stop point as in charge mode
static float coarse_stop_point(const powder_analysis_tuner_t *tuner) {
    float stop_point = tuner->target_weight - tuner->coarse_stop_threshold;
    if (stop_point < 0.0f) {
        stop_point = 0.0f;
    }
    return stop_point;
}


// ---------------------------------------------------------------------------------------------
// Coarse Kd probing, concept section 4.6. The coarse Kd is meant as a gentle brake: it slows the
// coarse trickler down towards its stop point so the coarse throw becomes more consistent, and it
// can only ever make the throw slower. Coarse times scatter more than Kd shifts them, so instead
// of a search: double the start value (bounded), stop when the time no longer changes or exceeds
// the coarse time goal, take the fastest Kd within the goal (ties -> smallest Kd). If not even
// the start value was within the goal, it is retried a few times first.
// ---------------------------------------------------------------------------------------------
static float coarse_kd_next_value(const powder_analysis_tuner_t *tuner) {
    return POWDER_ANALYSIS_COARSE_KD_START * (float) (1 << tuner->coarse_kd_count);
}


// Did any throw of the probing stay within the time goal?
static bool coarse_kd_has_ok_throw(const powder_analysis_tuner_t *tuner) {
    for (int i = 0; i < tuner->coarse_kd_count; i++) {
        if (tuner->coarse_kd_time_s[i] <= POWDER_ANALYSIS_COARSE_TIME_GOAL_S) {
            return true;
        }
    }
    return false;
}


// Choose the coarse Kd from the throws made so far
static void finish_coarse_kd_phase(powder_analysis_tuner_t *tuner) {
    if (tuner->coarse_kd_count == 0) {
        tuner->coarse_kd = POWDER_ANALYSIS_COARSE_KD_START;     // not a single throw was made
        tuner->coarse_kd_found = false;
        return;
    }

    float time_goal = POWDER_ANALYSIS_COARSE_TIME_GOAL_S;

    // The fastest throw within the time goal
    int fastest = -1;
    for (int i = 0; i < tuner->coarse_kd_count; i++) {
        if (tuner->coarse_kd_time_s[i] <= time_goal &&
            (fastest < 0 || tuner->coarse_kd_time_s[i] < tuner->coarse_kd_time_s[fastest])) {
            fastest = i;
        }
    }

    // Of all throws about as fast as the fastest one, the smallest Kd (= first tried) wins
    int chosen = fastest;
    if (fastest >= 0) {
        for (int i = 0; i < fastest; i++) {
            if (tuner->coarse_kd_time_s[i] <= time_goal &&
                tuner->coarse_kd_time_s[i] <= tuner->coarse_kd_time_s[fastest] + POWDER_ANALYSIS_COARSE_KD_TIME_TIE_S) {
                chosen = i;
                break;
            }
        }
    }

    tuner->coarse_kd_found = (chosen >= 0);
    if (chosen < 0) {
        chosen = 0;     // nothing within the time goal: the smallest Kd tried
    }

    tuner->coarse_kd = tuner->coarse_kd_tried[chosen];
    tuner->coarse_time_s = tuner->coarse_kd_time_s[chosen];
    tuner->coarse_overshoot_gr = tuner->coarse_kd_overshoot_gr[chosen];
}


// End the analysis and keep the best values found so far
static void finish(powder_analysis_tuner_t *tuner) {
    if (tuner->phase == POWDER_ANALYSIS_PHASE_VERIFY) {
        // The result is the best round and its gains
        update_verify_best_round(tuner);
        tuner->fine_kp = tuner->verify_best_round.fine_kp;
        tuner->fine_min_flow_speed_rps = tuner->verify_best_round.fine_min_flow_speed_rps;
        tuner->total_time_s = verify_round_average_hit_time(&tuner->verify_best_round);
    }
    else if (tuner->phase == POWDER_ANALYSIS_PHASE_COARSE_KD) {
        finish_coarse_kd_phase(tuner);      // interrupted, choose from the throws made so far
    }
    else if (tuner->phase != POWDER_ANALYSIS_PHASE_DONE) {
        store_search_result(tuner);     // the search was interrupted, keep its best value
    }

    tuner->phase = POWDER_ANALYSIS_PHASE_DONE;
}


// ---------------------------------------------------------------------------------------------
// Trial evaluation
// ---------------------------------------------------------------------------------------------
static powder_analysis_evaluation_t evaluate_trial(const powder_analysis_tuner_t *tuner,
                                                   const powder_analysis_trial_result_t *result) {
    if (result->timed_out) {
        return POWDER_ANALYSIS_EVAL_TIMEOUT;
    }

    switch (tuner->phase) {
        case POWDER_ANALYSIS_PHASE_COARSE_KP:
            // Overthrow of the coarse stop point, fast enough to leave room for the coarse Kd
            if (result->weight <= coarse_stop_point(tuner)) {
                return POWDER_ANALYSIS_EVAL_UNDER;
            }
            if (result->coarse_time_s > POWDER_ANALYSIS_COARSE_TIME_GOAL_S / POWDER_ANALYSIS_COARSE_BRAKING_FACTOR) {
                return POWDER_ANALYSIS_EVAL_TOO_SLOW;
            }
            return POWDER_ANALYSIS_EVAL_OVER;

        case POWDER_ANALYSIS_PHASE_COARSE_KD:
            // The throw with Kd must stay within the coarse time goal
            if (result->coarse_time_s > POWDER_ANALYSIS_COARSE_TIME_GOAL_S) {
                return POWDER_ANALYSIS_EVAL_TOO_SLOW;
            }
            return POWDER_ANALYSIS_EVAL_OK;

        case POWDER_ANALYSIS_PHASE_FINE_KP:
        case POWDER_ANALYSIS_PHASE_VERIFY:
        default:
            // Hit = within target +/- fine_stop_threshold (same as charge mode) and fast enough
            if (result->weight > tuner->target_weight + tuner->fine_stop_threshold) {
                return POWDER_ANALYSIS_EVAL_OVER;
            }
            if (result->weight < tuner->target_weight - tuner->fine_stop_threshold) {
                return POWDER_ANALYSIS_EVAL_UNDER;
            }
            if (result->total_time_s >= POWDER_ANALYSIS_TOTAL_TIME_GOAL_S) {
                return POWDER_ANALYSIS_EVAL_TOO_SLOW;
            }
            return POWDER_ANALYSIS_EVAL_HIT;
    }
}


// Translate a throw into the yes/no answer the bracketing search needs
static bool search_condition_met(powder_analysis_phase_t phase,
                                 const powder_analysis_trial_result_t *result,
                                 powder_analysis_evaluation_t evaluation) {
    switch (phase) {
        case POWDER_ANALYSIS_PHASE_COARSE_KP:
            return evaluation == POWDER_ANALYSIS_EVAL_OVER;     // overthrow and fast enough
        case POWDER_ANALYSIS_PHASE_FINE_KP:
            // Fast enough. An overthrow does not matter here: a smaller Kp only lowers it, and
            // the verification correction handles what is left.
            return !result->timed_out && result->total_time_s <= POWDER_ANALYSIS_FINE_TARGET_TIME_S;
        default:
            return false;
    }
}


static void record_trial(powder_analysis_tuner_t *tuner,
                         const powder_analysis_trial_result_t *result,
                         powder_analysis_evaluation_t evaluation) {
    if (tuner->trial_count >= POWDER_ANALYSIS_MAX_TRIALS) {
        return;     // protocol is full, cannot happen as the tuner finishes at the limit
    }

    // The gains that were used for this throw
    powder_analysis_trial_params_t params;
    powder_analysis_tuner_get_trial(tuner, &params);

    bool coarse_phase = (tuner->phase == POWDER_ANALYSIS_PHASE_COARSE_KP ||
                         tuner->phase == POWDER_ANALYSIS_PHASE_COARSE_KD);

    powder_analysis_trial_t *trial = &tuner->trials[tuner->trial_count];
    trial->number = tuner->trial_count + 1;
    trial->phase = tuner->phase;
    trial->kp = coarse_phase ? params.coarse_kp : params.fine_kp;
    trial->kd = coarse_phase ? params.coarse_kd : params.fine_kd;
    trial->weight = result->weight;
    trial->coarse_time_s = result->coarse_time_s;
    trial->total_time_s = result->total_time_s;
    trial->evaluation = evaluation;
    trial->fine_min_flow_speed_rps = params.fine_min_flow_speed_rps;

    // Fall after the stop: only a full throw that reached the fine stop can tell
    trial->has_fall = (params.throw_type == POWDER_ANALYSIS_THROW_FULL && !result->timed_out && result->stop_weight > 0.0f);
    trial->fall_gr = trial->has_fall ? result->weight - result->stop_weight : 0.0f;

    tuner->trial_count += 1;    // after the entry: the REST handler reads up to trial_count
}


// Keep what the throw of the current best value measured. The result reports these values.
static void remember_best_throw(powder_analysis_tuner_t *tuner, const powder_analysis_trial_result_t *result) {
    switch (tuner->phase) {
        case POWDER_ANALYSIS_PHASE_COARSE_KP:
            tuner->coarse_base_time_s = result->coarse_time_s;      // time with Kd 0
            break;
        case POWDER_ANALYSIS_PHASE_FINE_KP:
            tuner->total_time_s = result->total_time_s;      // the result flags a slow throw
            break;
        default:
            break;
    }
}


// Result handling in the Kp search phases
static void update_search_phase(powder_analysis_tuner_t *tuner,
                                const powder_analysis_trial_result_t *result,
                                powder_analysis_evaluation_t evaluation) {
    bool met = search_condition_met(tuner->phase, result, evaluation);
    float tried_value = tuner->search.value;

    powder_analysis_search_update(&tuner->search, met);

    // The search moves towards the boundary, so the last throw that is the best value of the
    // search is the throw of the value the phase ends with
    if (tried_value == powder_analysis_search_best_value(&tuner->search)) {
        remember_best_throw(tuner, result);
    }

    if (tuner->search.finished) {
        store_search_result(tuner);
        start_phase(tuner, (powder_analysis_phase_t) (tuner->phase + 1));
    }
}


// Result handling in the coarse Kd phase, see the explanation above coarse_kd_next_value()
static void update_coarse_kd_phase(powder_analysis_tuner_t *tuner,
                               const powder_analysis_trial_result_t *result,
                               powder_analysis_evaluation_t evaluation) {
    uint8_t idx = tuner->coarse_kd_count;
    tuner->coarse_kd_tried[idx] = coarse_kd_next_value(tuner);
    tuner->coarse_kd_time_s[idx] = result->coarse_time_s;
    tuner->coarse_kd_overshoot_gr[idx] = result->weight - coarse_stop_point(tuner);
    tuner->coarse_kd_count += 1;

    bool stop = false;

    // Over the coarse time goal (or timed out): more Kd would only be slower
    if (evaluation != POWDER_ANALYSIS_EVAL_OK) {
        stop = true;
    }

    // The time hardly changed since the previous Kd: more Kd will not change it either
    if (idx >= 1 && tuner->coarse_kd_time_s[idx - 1] > 0.0f) {
        float previous_time = tuner->coarse_kd_time_s[idx - 1];
        float change = fabsf(result->coarse_time_s - previous_time) / previous_time;
        if (change < POWDER_ANALYSIS_COARSE_KD_MIN_TIME_CHANGE) {
            stop = true;
        }
    }

    // Start value plus the allowed doublings were tried
    if (tuner->coarse_kd_count > POWDER_ANALYSIS_COARSE_KD_MAX_DOUBLINGS) {
        stop = true;
    }

    if (stop) {
        // Not a single throw within the time goal yet (the start value was too slow)? Coarse times
        // scatter a lot, so repeat it a few times before giving the coarse Kd up.
        if (!coarse_kd_has_ok_throw(tuner) && tuner->coarse_kd_retries < POWDER_ANALYSIS_COARSE_KD_MAX_RETRIES) {
            tuner->coarse_kd_retries += 1;
            tuner->coarse_kd_count = 0;     // probing starts over from the start value
            return;
        }

        finish_coarse_kd_phase(tuner);
        start_phase(tuner, POWDER_ANALYSIS_PHASE_FINE_KP);
    }
}


// Result handling in the verification phase. A round only tests fixed gains, it never changes
// them; a correction happens between rounds.
static void update_verify_phase(powder_analysis_tuner_t *tuner,
                                const powder_analysis_trial_result_t *result,
                                powder_analysis_evaluation_t evaluation) {
    powder_analysis_verify_round_t *round = &tuner->verify_round;
    round->throw_count += 1;

    if (evaluation == POWDER_ANALYSIS_EVAL_HIT) {
        round->hit_count += 1;
        round->hit_time_sum_s += result->total_time_s;
    }

    if (evaluation == POWDER_ANALYSIS_EVAL_OVER) {
        round->overthrow_count += 1;
        if (round->overthrow_count >= POWDER_ANALYSIS_VERIFY_MAX_OVERTHROWS) {
            // Too much powder falls after the fine trickler stops. Give the round up and lower
            // the speed at the stop = max(Kp * threshold, minimum speed): whichever of the two
            // decides it (see the config), unless that was already done often enough.
            if (tuner->fine_corrections >= POWDER_ANALYSIS_MAX_FINE_CORRECTIONS) {
                tuner->verify_overthrow_limit_reached = true;
                finish(tuner);
                return;
            }
            update_verify_best_round(tuner);
            if (tuner->fine_kp * tuner->fine_stop_threshold > tuner->fine_min_flow_speed_rps) {
                tuner->fine_kp *= POWDER_ANALYSIS_FINE_KP_CORRECTION_FACTOR;
                tuner->last_correction_lowered_kp = true;
            }
            else {
                tuner->fine_min_flow_speed_rps *= POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR;
                tuner->last_correction_lowered_kp = false;
            }
            tuner->fine_corrections += 1;
            start_verify_round(tuner);
            return;
        }
    }

    // The round is over: verified with enough hits, otherwise the gains are what they are and
    // the result shows how many hits they achieved
    if (round->throw_count >= POWDER_ANALYSIS_VERIFY_ROUND_THROWS) {
        tuner->verified = (round->hit_count >= POWDER_ANALYSIS_VERIFY_HITS_FOR_VERIFIED);
        finish(tuner);
    }
}


// ---------------------------------------------------------------------------------------------
// Fall after the stop
// ---------------------------------------------------------------------------------------------
// Falls of the full throws of one phase, sorted ascending. Returns how many there are.
static int collect_falls(const powder_analysis_tuner_t *tuner, powder_analysis_phase_t phase, float *falls) {
    int count = 0;
    for (int i = 0; i < tuner->trial_count; i++) {
        if (tuner->trials[i].phase == phase && tuner->trials[i].has_fall) {
            falls[count] = tuner->trials[i].fall_gr;
            count += 1;
        }
    }

    // Insertion sort, the list is short
    for (int i = 1; i < count; i++) {
        float value = falls[i];
        int j = i - 1;
        while (j >= 0 && falls[j] > value) {
            falls[j + 1] = falls[j];
            j -= 1;
        }
        falls[j + 1] = value;
    }
    return count;
}


// A threshold that is an exact multiple of the scale resolution is fragile: float rounding decides
// whether the throw stops one step early or at the target (concept 4.10a). Same check as
// isExactScaleStep() in the web page.
bool powder_analysis_is_exact_scale_step(float threshold) {
    if (threshold <= 0.0f) {
        return false;
    }
    float steps = threshold / POWDER_ANALYSIS_SCALE_RESOLUTION_GR;
    return fabsf(steps - roundf(steps)) * POWDER_ANALYSIS_SCALE_RESOLUTION_GR < 0.001f;
}


// Fall after the stop (settled weight - weight at the fine stop) over all verification throws and
// the fine stop threshold recommendation derived from its median, concept section 4.10a: a
// threshold halfway between two scale steps stops reliably at target - n * resolution. Hint only.
static void compute_fall_statistics(const powder_analysis_tuner_t *tuner, powder_analysis_result_t *result) {
    float falls[POWDER_ANALYSIS_MAX_TRIALS];

    int count = collect_falls(tuner, POWDER_ANALYSIS_PHASE_VERIFY, falls);
    if (count == 0) {
        count = collect_falls(tuner, POWDER_ANALYSIS_PHASE_FINE_KP, falls);    // verification never reached
    }
    if (count == 0) {
        return;     // nothing measured, the result keeps its zeros
    }

    float median = (count % 2 == 1) ? falls[count / 2] : (falls[count / 2 - 1] + falls[count / 2]) / 2.0f;

    // How many scale steps before the target the trickler should stop: the fall rounded to whole
    // steps (half up), at least one step. The small tolerance makes an exact half (0.07 / 0.02 is
    // 3.4999... in float) round up as intended.
    float resolution = POWDER_ANALYSIS_SCALE_RESOLUTION_GR;
    int steps = (int) floorf(median / resolution + 0.5f + 0.001f);
    if (steps < 1) {
        steps = 1;
    }

    result->fall_count = (uint8_t) count;
    result->fall_median_gr = median;
    result->fall_max_gr = falls[count - 1];
    result->recommended_fine_threshold = ((float) steps + 0.5f) * resolution;
    result->expected_final_offset_gr = median - (float) steps * resolution;
}


// ---------------------------------------------------------------------------------------------
// Interface
// ---------------------------------------------------------------------------------------------
void powder_analysis_tuner_init(powder_analysis_tuner_t *tuner,
                                powder_shape_t shape, grain_size_t grain_size,
                                float target_weight, float coarse_stop_threshold, float fine_stop_threshold) {
    memset(tuner, 0x0, sizeof(*tuner));

    // Guard the table lookup against invalid input (the enums may be unsigned, so check as int)
    if ((int) shape < 0 || (int) shape >= POWDER_SHAPE_COUNT) {
        shape = POWDER_SHAPE_BALL;
    }
    if ((int) grain_size < 0 || (int) grain_size >= GRAIN_SIZE_COUNT) {
        grain_size = GRAIN_SIZE_FINE;
    }

    tuner->target_weight = target_weight;
    tuner->coarse_stop_threshold = coarse_stop_threshold;
    tuner->fine_stop_threshold = fine_stop_threshold;
    tuner->coarse_kp_start = powder_analysis_coarse_kp_start[shape][grain_size];
    tuner->fine_kp_start = powder_analysis_fine_kp_start[shape][grain_size];
    tuner->fine_kd = powder_analysis_fine_kd_start[shape][grain_size];      // final value, never searched
    tuner->fine_min_flow_speed_rps = POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS;

    start_phase(tuner, POWDER_ANALYSIS_PHASE_COARSE_KP);
}


void powder_analysis_tuner_get_trial(const powder_analysis_tuner_t *tuner, powder_analysis_trial_params_t *params) {
    memset(params, 0x0, sizeof(*params));

    params->trial_number = tuner->trial_count + 1;
    params->phase = tuner->phase;

    // Default: all gains found so far, the one being searched is replaced below
    params->coarse_kp = tuner->coarse_kp;
    params->coarse_kd = tuner->coarse_kd;
    params->fine_kp = tuner->fine_kp;
    params->fine_kd = tuner->fine_kd;
    params->fine_min_flow_speed_rps = tuner->fine_min_flow_speed_rps;

    switch (tuner->phase) {
        case POWDER_ANALYSIS_PHASE_COARSE_KP:
            params->throw_type = POWDER_ANALYSIS_THROW_COARSE_ONLY;
            params->coarse_kp = tuner->search.value;
            params->coarse_kd = 0.0f;
            params->fine_kp = 0.0f;
            params->fine_kd = 0.0f;
            break;

        case POWDER_ANALYSIS_PHASE_COARSE_KD:
            params->throw_type = POWDER_ANALYSIS_THROW_COARSE_ONLY;
            params->coarse_kd = coarse_kd_next_value(tuner);
            params->fine_kp = 0.0f;
            params->fine_kd = 0.0f;
            break;

        case POWDER_ANALYSIS_PHASE_FINE_KP:
            params->throw_type = POWDER_ANALYSIS_THROW_FULL;
            params->fine_kp = tuner->search.value;      // fine Kd already has its final value
            break;

        case POWDER_ANALYSIS_PHASE_VERIFY:
        default:
            params->throw_type = POWDER_ANALYSIS_THROW_FULL;
            break;
    }
}


powder_analysis_evaluation_t powder_analysis_tuner_report_result(powder_analysis_tuner_t *tuner,
                                                                 const powder_analysis_trial_result_t *result) {
    powder_analysis_evaluation_t evaluation = evaluate_trial(tuner, result);

    if (tuner->phase == POWDER_ANALYSIS_PHASE_DONE) {
        return evaluation;      // analysis is over, nothing to update
    }

    record_trial(tuner, result, evaluation);

    if (tuner->phase == POWDER_ANALYSIS_PHASE_VERIFY) {
        update_verify_phase(tuner, result, evaluation);
    }
    else if (tuner->phase == POWDER_ANALYSIS_PHASE_COARSE_KD) {
        update_coarse_kd_phase(tuner, result, evaluation);
    }
    else {
        update_search_phase(tuner, result, evaluation);
    }

    // Global limit: stop with the best result so far
    if (tuner->phase != POWDER_ANALYSIS_PHASE_DONE && tuner->trial_count >= POWDER_ANALYSIS_MAX_TRIALS) {
        tuner->trial_limit_reached = true;
        finish(tuner);
    }

    return evaluation;
}


bool powder_analysis_tuner_is_done(const powder_analysis_tuner_t *tuner) {
    return tuner->phase == POWDER_ANALYSIS_PHASE_DONE;
}


// Meaningful once the tuner is done. Before that it returns the gains of the completed phases.
void powder_analysis_tuner_get_result(const powder_analysis_tuner_t *tuner, powder_analysis_result_t *result) {
    memset(result, 0x0, sizeof(*result));

    // The 10 profile values
    result->coarse_kp = tuner->coarse_kp;
    result->coarse_ki = POWDER_ANALYSIS_KI;
    result->coarse_kd = tuner->coarse_kd;
    result->coarse_min_flow_speed_rps = POWDER_ANALYSIS_COARSE_MIN_FLOW_SPEED_RPS;
    result->coarse_max_flow_speed_rps = POWDER_ANALYSIS_COARSE_MAX_FLOW_SPEED_RPS;

    result->fine_kp = tuner->fine_kp;
    result->fine_ki = POWDER_ANALYSIS_KI;
    result->fine_kd = tuner->fine_kd;
    result->fine_min_flow_speed_rps = tuner->fine_min_flow_speed_rps;      // as lowered by the verification
    result->fine_max_flow_speed_rps = POWDER_ANALYSIS_FINE_MAX_FLOW_SPEED_RPS;

    // Verification and why the analysis ended
    result->verified = tuner->verified;
    result->verify_reached = tuner->verify_reached;
    result->verify_hits = tuner->verify_best_round.hit_count;
    result->fine_corrections = tuner->fine_corrections;
    result->trial_limit_reached = tuner->trial_limit_reached;
    result->verify_overthrow_limit_reached = tuner->verify_overthrow_limit_reached;
    result->trial_count = tuner->trial_count;

    // Measured with the found values. A time of 0 means it was never measured.
    result->coarse_base_time_s = tuner->coarse_base_time_s;
    result->coarse_time_s = tuner->coarse_time_s;
    result->coarse_overshoot_gr = tuner->coarse_overshoot_gr;
    result->total_time_s = tuner->total_time_s;

    result->coarse_time_goal_missed = (tuner->coarse_time_s <= 0.0f ||
                                       tuner->coarse_time_s > POWDER_ANALYSIS_COARSE_TIME_GOAL_S);
    result->coarse_kd_not_found = !tuner->coarse_kd_found;
    result->coarse_overshoot_too_high = (tuner->coarse_overshoot_gr > POWDER_ANALYSIS_COARSE_MAX_OVERSHOOT_GR);
    result->total_time_goal_missed = (tuner->total_time_s <= 0.0f ||
                                      tuner->total_time_s >= POWDER_ANALYSIS_TOTAL_TIME_GOAL_S);

    compute_fall_statistics(tuner, result);
}


const char * powder_analysis_phase_to_string(powder_analysis_phase_t phase) {
    switch (phase) {
        case POWDER_ANALYSIS_PHASE_COARSE_KP:
            return "Coarse Kp";
        case POWDER_ANALYSIS_PHASE_COARSE_KD:
            return "Coarse Kd";
        case POWDER_ANALYSIS_PHASE_FINE_KP:
            return "Fine Kp";
        case POWDER_ANALYSIS_PHASE_VERIFY:
            return "Verify";
        case POWDER_ANALYSIS_PHASE_DONE:
            return "Done";
        default:
            return "Unknown";
    }
}


const char * powder_analysis_evaluation_to_string(powder_analysis_evaluation_t evaluation) {
    switch (evaluation) {
        case POWDER_ANALYSIS_EVAL_UNDER:
            return "UNDER";
        case POWDER_ANALYSIS_EVAL_OVER:
            return "OVER";
        case POWDER_ANALYSIS_EVAL_HIT:
            return "HIT";
        case POWDER_ANALYSIS_EVAL_TOO_SLOW:
            return "SLOW";
        case POWDER_ANALYSIS_EVAL_OK:
            return "OK";
        case POWDER_ANALYSIS_EVAL_TIMEOUT:
            return "TIMEOUT";
        default:
            return "?";
    }
}
