// Host unit tests for the powder analysis tuner. No framework, just CHECK() and a summary.

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "powder_analysis_tuner.h"
#include "simulated_powder.h"


static int checks_failed = 0;
static int checks_total = 0;

#define CHECK(cond) do { \
    checks_total += 1; \
    if (!(cond)) { \
        checks_failed += 1; \
        printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_NEAR(a, b, tolerance) CHECK(fabsf((a) - (b)) <= (tolerance))


// Test fixture values
static const float TARGET = 50.0f;
static const float COARSE_STOP = 3.0f;
static const float FINE_STOP = 0.02f;
static const float STOP_POINT = TARGET - COARSE_STOP;


// Run the tuner against a simulated powder until it is done (or the step limit is hit)
static void run_analysis(powder_analysis_tuner_t *tuner, simulated_powder_t *powder) {
    for (int i = 0; i < 200 && !powder_analysis_tuner_is_done(tuner); i++) {
        powder_analysis_trial_params_t params;
        powder_analysis_trial_result_t result;

        powder_analysis_tuner_get_trial(tuner, &params);
        simulated_powder_throw(powder, &params, &result);
        powder_analysis_tuner_report_result(tuner, &result);
    }
}


// Run the tuner until it reaches the given phase
static void run_until_phase(powder_analysis_tuner_t *tuner, simulated_powder_t *powder, powder_analysis_phase_t phase) {
    for (int i = 0; i < 200 && tuner->phase != phase && !powder_analysis_tuner_is_done(tuner); i++) {
        powder_analysis_trial_params_t params;
        powder_analysis_trial_result_t result;

        powder_analysis_tuner_get_trial(tuner, &params);
        simulated_powder_throw(powder, &params, &result);
        powder_analysis_tuner_report_result(tuner, &result);
    }
}


// Report a hand-made throw result
static powder_analysis_evaluation_t report(powder_analysis_tuner_t *tuner, float weight,
                                           float coarse_time_s, float total_time_s, bool timed_out = false,
                                           float stop_weight = 0.0f) {
    powder_analysis_trial_result_t result;
    result.weight = weight;
    result.coarse_time_s = coarse_time_s;
    result.total_time_s = total_time_s;
    result.timed_out = timed_out;
    result.stop_weight = stop_weight;
    return powder_analysis_tuner_report_result(tuner, &result);
}


static void print_protocol(const powder_analysis_tuner_t *tuner) {
    for (int i = 0; i < tuner->trial_count; i++) {
        const powder_analysis_trial_t *t = &tuner->trials[i];
        printf("    #%2d %-9s kp=%-8.4f kd=%-8.4f %7.3f gr  %5.1f s / %5.1f s  %s\n",
               t->number, powder_analysis_phase_to_string(t->phase), t->kp, t->kd,
               t->weight, t->coarse_time_s, t->total_time_s,
               powder_analysis_evaluation_to_string(t->evaluation));
    }
}


// Fine Kp at which the simulated full throw takes exactly the fine target time
static float fine_kp_time_boundary(const powder_analysis_tuner_t *tuner, const simulated_powder_t *powder) {
    float fine_time_allowed = POWDER_ANALYSIS_FINE_TARGET_TIME_S - tuner->coarse_time_s - powder->fine_kd_slowdown * tuner->fine_kd;
    return powder->fine_time_base / fine_time_allowed;
}


// Throws of a phase that took longer than the fine target time (whatever their label)
static int count_slow_throws(const powder_analysis_tuner_t *tuner, powder_analysis_phase_t phase) {
    int count = 0;
    for (int i = 0; i < tuner->trial_count; i++) {
        if (tuner->trials[i].phase == phase && tuner->trials[i].total_time_s > POWDER_ANALYSIS_FINE_TARGET_TIME_S) {
            count += 1;
        }
    }
    return count;
}


static int count_evaluations(const powder_analysis_tuner_t *tuner, powder_analysis_phase_t phase,
                             powder_analysis_evaluation_t evaluation) {
    int count = 0;
    for (int i = 0; i < tuner->trial_count; i++) {
        if (tuner->trials[i].phase == phase && tuner->trials[i].evaluation == evaluation) {
            count += 1;
        }
    }
    return count;
}


static int count_phase_trials(const powder_analysis_tuner_t *tuner, powder_analysis_phase_t phase) {
    int count = 0;
    for (int i = 0; i < tuner->trial_count; i++) {
        if (tuner->trials[i].phase == phase) {
            count += 1;
        }
    }
    return count;
}


// ---------------------------------------------------------------------------------------------
// Bracketing search on its own
// ---------------------------------------------------------------------------------------------
// Search with the condition "value > boundary"
static float search_smallest_above(float start, float boundary, int *steps) {
    powder_analysis_search_t search;
    powder_analysis_search_init(&search, start);

    *steps = 0;
    while (!search.finished && *steps < 100) {
        powder_analysis_search_update(&search, search.value > boundary);
        *steps += 1;
    }
    return search.value;
}


static void test_search_smallest_start_too_low() {
    printf("test_search_smallest_start_too_low\n");

    int steps;
    float found = search_smallest_above(0.02f, 0.3f, &steps);

    // Result is the smallest "met" value: above the boundary but within 15 % of it
    CHECK(found > 0.3f);
    CHECK(found <= 0.3f * 1.15f);
    CHECK(steps <= 5 + POWDER_ANALYSIS_MAX_BISECTION_STEPS + 1);    // 5 doublings, then bisection
}


static void test_search_smallest_start_too_high() {
    printf("test_search_smallest_start_too_high\n");

    int steps;
    float found = search_smallest_above(4.0f, 0.3f, &steps);

    CHECK(found > 0.3f);
    CHECK(found <= 0.3f * 1.15f);
    CHECK(steps <= 5 + POWDER_ANALYSIS_MAX_BISECTION_STEPS + 1);    // 4 halvings, then bisection
}


static void test_search_halving_limit() {
    printf("test_search_halving_limit\n");

    // Condition always met: halving stops at start / 2^MAX_HALVING_STEPS, result is met
    int steps;
    float found = search_smallest_above(1.0f, 0.0f, &steps);
    CHECK_NEAR(found, 1.0f / 32.0f, 1e-6f);
    CHECK(steps == POWDER_ANALYSIS_MAX_HALVING_STEPS + 1);

}


static void test_search_doubling_limit() {
    printf("test_search_doubling_limit\n");

    // Condition never met: start value + 6 doublings, result is the largest value tried
    powder_analysis_search_t search;
    powder_analysis_search_init(&search, 1.0f);
    int steps = 0;
    while (!search.finished && steps < 100) {
        powder_analysis_search_update(&search, false);
        steps += 1;
    }
    CHECK(steps == POWDER_ANALYSIS_MAX_DOUBLING_STEPS + 1);
    CHECK(!search.has_met);
    CHECK_NEAR(search.value, 64.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_search_best_value(&search), 64.0f, 1e-6f);

}


static void test_search_bisection_limit() {
    printf("test_search_bisection_limit\n");

    // Start exactly one doubling below the boundary: the interval starts at 50 %, three bisection
    // steps bring it to 6.25 % at most
    powder_analysis_search_t search;
    powder_analysis_search_init(&search, 0.1f);
    powder_analysis_search_update(&search, false);     // 0.1 not met
    powder_analysis_search_update(&search, true);      // 0.2 met -> bisect
    CHECK(!search.finished);
    CHECK_NEAR(search.value, 0.15f, 1e-6f);
    powder_analysis_search_update(&search, true);      // 0.15 met
    CHECK_NEAR(search.value, 0.125f, 1e-6f);
    powder_analysis_search_update(&search, false);     // 0.125 not met
    CHECK_NEAR(search.value, 0.1375f, 1e-6f);
    powder_analysis_search_update(&search, false);     // 0.1375 not met, 3 steps done
    CHECK(search.finished);
    CHECK_NEAR(search.value, 0.15f, 1e-6f);
    CHECK_NEAR(powder_analysis_search_best_value(&search), 0.15f, 1e-6f);
}


// ---------------------------------------------------------------------------------------------
// Tuner
// ---------------------------------------------------------------------------------------------
static void test_start_values_from_matrix() {
    printf("test_start_values_from_matrix\n");

    // Spot check the matrix itself
    CHECK_NEAR(powder_analysis_coarse_kp_start[POWDER_SHAPE_STICK][GRAIN_SIZE_MEDIUM], 0.06f, 1e-6f);
    CHECK_NEAR(powder_analysis_coarse_kp_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 0.08f, 1e-6f);
    CHECK_NEAR(powder_analysis_coarse_kp_start[POWDER_SHAPE_FLAKE][GRAIN_SIZE_FINE], 0.16f, 1e-6f);
    CHECK_NEAR(powder_analysis_coarse_kp_start[POWDER_SHAPE_FLAKE][GRAIN_SIZE_COARSE], 0.18f, 1e-6f);
    CHECK_NEAR(powder_analysis_coarse_kp_start[POWDER_SHAPE_STICK][GRAIN_SIZE_COARSE], 0.12f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kp_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 3.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kp_start[POWDER_SHAPE_STICK][GRAIN_SIZE_FINE], 4.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kp_start[POWDER_SHAPE_DISC][GRAIN_SIZE_FINE], 5.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kp_start[POWDER_SHAPE_DISC][GRAIN_SIZE_COARSE], 4.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kp_start[POWDER_SHAPE_STICK][GRAIN_SIZE_COARSE], 3.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kp_start[POWDER_SHAPE_STICK][GRAIN_SIZE_MEDIUM], 3.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kd_start[POWDER_SHAPE_STICK][GRAIN_SIZE_MEDIUM], 8.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kd_start[POWDER_SHAPE_FLAKE][GRAIN_SIZE_MEDIUM], 8.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kd_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 4.0f, 1e-6f);
    CHECK_NEAR(powder_analysis_fine_kd_start[POWDER_SHAPE_DISC][GRAIN_SIZE_COARSE], 4.0f, 1e-6f);
    CHECK(strcmp(powder_shape_names[POWDER_SHAPE_FLAKE], "Flake") == 0);
    CHECK(strcmp(grain_size_names[GRAIN_SIZE_COARSE], "Coarse") == 0);

    // First trial uses the coarse Kp start value, Kd 0 and a coarse-only throw with fine gains 0
    powder_analysis_tuner_t tuner;
    powder_analysis_trial_params_t params;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_STICK, GRAIN_SIZE_MEDIUM, TARGET, COARSE_STOP, FINE_STOP);
    powder_analysis_tuner_get_trial(&tuner, &params);

    CHECK(params.trial_number == 1);
    CHECK(params.phase == POWDER_ANALYSIS_PHASE_COARSE_KP);
    CHECK(params.throw_type == POWDER_ANALYSIS_THROW_COARSE_ONLY);
    CHECK_NEAR(params.coarse_kp, 0.06f, 1e-6f);
    CHECK(params.coarse_kd == 0.0f);
    CHECK(params.fine_kp == 0.0f);
    CHECK(params.fine_kd == 0.0f);
    CHECK_NEAR(params.fine_min_flow_speed_rps, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS, 1e-6f);

    // Kd start values and the fine Kp start value show up when their phases begin
    simulated_powder_t powder;
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);

    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_COARSE_KD);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);

    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_FINE_KP);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.fine_kp, 3.0f, 1e-6f);

    // The fine Kd is never searched: the matrix value is used from the first fine throw on
    CHECK_NEAR(params.fine_kd, 8.0f, 1e-6f);     // medium column of the fine Kd matrix
    CHECK_NEAR(tuner.fine_kd, powder_analysis_fine_kd_start[POWDER_SHAPE_STICK][GRAIN_SIZE_MEDIUM], 1e-6f);
}


static void test_invalid_shape_and_grain_fall_back() {
    printf("test_invalid_shape_and_grain_fall_back\n");

    // Out-of-range values (also negative ones through the enum) fall back to Ball / Fine
    powder_analysis_tuner_t tuner;
    powder_analysis_trial_params_t params;
    powder_analysis_tuner_init(&tuner, (powder_shape_t) -1, (grain_size_t) 7, TARGET, COARSE_STOP, FINE_STOP);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.coarse_kp, powder_analysis_coarse_kp_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 1e-6f);
    CHECK_NEAR(tuner.fine_kp_start, powder_analysis_fine_kp_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 1e-6f);
    CHECK_NEAR(tuner.fine_kd, powder_analysis_fine_kd_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 1e-6f);
}


static void test_all_phases_in_order() {
    printf("test_all_phases_in_order\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);

    // Drive the tuner by hand and watch the phase of every trial
    powder_analysis_phase_t last_phase = POWDER_ANALYSIS_PHASE_COARSE_KP;
    bool phase_seen[5] = {false, false, false, false, false};
    bool fine_kd_constant = true;
    bool order_ok = true;
    bool throw_types_ok = true;
    bool gains_ok = true;

    for (int i = 0; i < 200 && !powder_analysis_tuner_is_done(&tuner); i++) {
        powder_analysis_trial_params_t params;
        powder_analysis_trial_result_t result;
        powder_analysis_tuner_get_trial(&tuner, &params);

        if (params.phase < last_phase) {
            order_ok = false;       // phases must never go backwards
        }
        last_phase = params.phase;
        phase_seen[params.phase] = true;

        bool coarse_phase = (params.phase == POWDER_ANALYSIS_PHASE_COARSE_KP ||
                             params.phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
        if (coarse_phase != (params.throw_type == POWDER_ANALYSIS_THROW_COARSE_ONLY)) {
            throw_types_ok = false;
        }
        if (coarse_phase && (params.fine_kp != 0.0f || params.fine_kd != 0.0f)) {
            gains_ok = false;       // fine trickler runs with all gains 0 in the coarse phases
        }
        if (params.phase == POWDER_ANALYSIS_PHASE_COARSE_KP && params.coarse_kd != 0.0f) {
            gains_ok = false;
        }
        if (params.throw_type == POWDER_ANALYSIS_THROW_FULL &&
            params.fine_kd != powder_analysis_fine_kd_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE]) {
            fine_kd_constant = false;   // the fine Kd is the matrix value in every full throw
        }

        simulated_powder_throw(&powder, &params, &result);
        powder_analysis_tuner_report_result(&tuner, &result);
    }

    CHECK(powder_analysis_tuner_is_done(&tuner));
    CHECK(order_ok);
    CHECK(throw_types_ok);
    CHECK(gains_ok);
    CHECK(fine_kd_constant);
    for (int phase = POWDER_ANALYSIS_PHASE_COARSE_KP; phase <= POWDER_ANALYSIS_PHASE_VERIFY; phase++) {
        CHECK(phase_seen[phase]);
    }

    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);

    CHECK(result.verified);
    CHECK(result.verify_reached);
    CHECK(result.verify_hits == POWDER_ANALYSIS_VERIFY_ROUND_THROWS);
    CHECK(!result.trial_limit_reached);
    CHECK(!result.verify_overthrow_limit_reached);
    CHECK(result.fine_corrections == 0);
    CHECK(!result.coarse_time_goal_missed);
    CHECK(!result.coarse_kd_not_found);
    CHECK(!result.coarse_overshoot_too_high);
    CHECK(!result.total_time_goal_missed);
    CHECK(result.trial_count == tuner.trial_count);
    CHECK(result.trial_count <= POWDER_ANALYSIS_MAX_TRIALS);

    // Found Kp values sit just above the boundaries of the simulated powder
    CHECK(result.coarse_kp > powder.coarse_kp_threshold);
    CHECK(result.coarse_kp <= powder.coarse_kp_threshold * 1.15f);
    // The fine Kp search ends just above the "13 s" boundary (within 15 %)
    float boundary_kp = fine_kp_time_boundary(&tuner, &powder);
    CHECK(result.fine_kp >= boundary_kp - 1e-4f);
    CHECK(result.fine_kp <= boundary_kp * 1.15f + 1e-4f);
    CHECK_NEAR(result.fine_kd, powder_analysis_fine_kd_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 1e-6f);

    // Coarse Kd probing with this powder: Kd 0.1 -> 4.14 s, 0.2 -> 4.64 s (+12 %), 0.4 -> 5.64 s,
    // 0.8 -> 7.64 s, 1.6 -> 11.64 s which is over the 10 s goal. The fastest one is the start value.
    float base_time = powder.coarse_time_base / result.coarse_kp;
    CHECK(count_phase_trials(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD) == 5);
    CHECK_NEAR(result.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);
    CHECK_NEAR(result.coarse_base_time_s, base_time, 1e-4f);
    CHECK(result.coarse_time_s > result.coarse_base_time_s);
    CHECK(result.coarse_time_s <= POWDER_ANALYSIS_COARSE_TIME_GOAL_S);
    CHECK(result.coarse_overshoot_gr > 0.0f && result.coarse_overshoot_gr <= POWDER_ANALYSIS_COARSE_MAX_OVERSHOOT_GR);

    // Fixed values
    CHECK(result.coarse_ki == 0.0f);
    CHECK(result.fine_ki == 0.0f);
    CHECK_NEAR(result.coarse_min_flow_speed_rps, 0.1f, 1e-6f);
    CHECK_NEAR(result.coarse_max_flow_speed_rps, 5.0f, 1e-6f);
    CHECK_NEAR(result.fine_min_flow_speed_rps, 0.06f, 1e-6f);
    CHECK_NEAR(result.fine_max_flow_speed_rps, 5.0f, 1e-6f);

    // Times were measured
    CHECK(result.coarse_time_s > 0.0f && result.coarse_time_s <= POWDER_ANALYSIS_COARSE_TIME_GOAL_S);
    CHECK(result.total_time_s > 0.0f && result.total_time_s < POWDER_ANALYSIS_TOTAL_TIME_GOAL_S);

    // Fall after the stop: the simulated full throws report their stop weight, coarse-only throws cannot
    CHECK(!tuner.trials[0].has_fall);
    CHECK(tuner.trials[tuner.trial_count - 1].has_fall);
    CHECK(result.fall_count == POWDER_ANALYSIS_VERIFY_ROUND_THROWS);
    CHECK(result.fall_median_gr >= 0.0f && result.fall_median_gr <= result.fall_max_gr);
    CHECK(result.recommended_fine_threshold >= 1.5f * POWDER_ANALYSIS_SCALE_RESOLUTION_GR - 1e-6f);

    // Protocol has one entry per trial
    CHECK(tuner.trials[0].number == 1);
    CHECK(tuner.trials[tuner.trial_count - 1].number == tuner.trial_count);
    CHECK(tuner.trials[tuner.trial_count - 1].phase == POWDER_ANALYSIS_PHASE_VERIFY);
    CHECK(tuner.trials[tuner.trial_count - 1].evaluation == POWDER_ANALYSIS_EVAL_HIT);

    printf("  %d trials, coarse kp=%.4f kd=%.4f (base %.2f s, with Kd %.2f s), fine kp=%.3f kd=%.2f\n",
           result.trial_count, result.coarse_kp, result.coarse_kd, result.coarse_base_time_s,
           result.coarse_time_s, result.fine_kp, result.fine_kd);
    if (checks_failed) {
        print_protocol(&tuner);
    }
}


static void test_evaluations() {
    printf("test_evaluations\n");

    powder_analysis_tuner_t tuner;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    const float kp_time_limit = POWDER_ANALYSIS_COARSE_TIME_GOAL_S / POWDER_ANALYSIS_COARSE_BRAKING_FACTOR;

    // COARSE_KP: overthrow above the stop point and fast enough to leave room for the coarse Kd, timeout is not met
    CHECK(report(&tuner, STOP_POINT, 5.0f, 5.0f) == POWDER_ANALYSIS_EVAL_UNDER);
    CHECK(report(&tuner, STOP_POINT + 0.01f, kp_time_limit + 0.1f, kp_time_limit + 0.1f) == POWDER_ANALYSIS_EVAL_TOO_SLOW);
    CHECK(report(&tuner, STOP_POINT + 0.01f, kp_time_limit, kp_time_limit) == POWDER_ANALYSIS_EVAL_OVER);
    tuner.phase = POWDER_ANALYSIS_PHASE_COARSE_KP;      // keep evaluating in this phase
    CHECK(report(&tuner, STOP_POINT - 1.0f, 30.0f, 30.0f, true) == POWDER_ANALYSIS_EVAL_TIMEOUT);

    // COARSE_KD: within the coarse time goal, whatever the base time was
    tuner.phase = POWDER_ANALYSIS_PHASE_COARSE_KD;
    tuner.coarse_base_time_s = 2.2f;                    // one lucky fast throw must not shrink the budget
    CHECK(report(&tuner, STOP_POINT + 0.5f, 8.6f, 8.6f) == POWDER_ANALYSIS_EVAL_OK);
    tuner.phase = POWDER_ANALYSIS_PHASE_COARSE_KD;
    CHECK(report(&tuner, STOP_POINT + 0.5f, 10.0f, 10.0f) == POWDER_ANALYSIS_EVAL_OK);
    tuner.phase = POWDER_ANALYSIS_PHASE_COARSE_KD;
    CHECK(report(&tuner, STOP_POINT + 0.5f, 10.1f, 10.1f) == POWDER_ANALYSIS_EVAL_TOO_SLOW);
    tuner.phase = POWDER_ANALYSIS_PHASE_COARSE_KD;
    CHECK(report(&tuner, STOP_POINT + 2.0f, 6.0f, 6.0f) == POWDER_ANALYSIS_EVAL_OK);     // overshoot is a control only

    // FINE_KP: same rating as the verification, an overthrow is an overthrow whatever the time
    tuner.phase = POWDER_ANALYSIS_PHASE_FINE_KP;
    CHECK(report(&tuner, TARGET + FINE_STOP, 5.0f, 8.0f) == POWDER_ANALYSIS_EVAL_HIT);
    tuner.phase = POWDER_ANALYSIS_PHASE_FINE_KP;
    CHECK(report(&tuner, TARGET + FINE_STOP + 0.01f, 5.0f, 15.0f) == POWDER_ANALYSIS_EVAL_OVER);
    tuner.phase = POWDER_ANALYSIS_PHASE_FINE_KP;
    CHECK(report(&tuner, TARGET + FINE_STOP + 0.01f, 5.0f, 14.9f) == POWDER_ANALYSIS_EVAL_OVER);
    tuner.phase = POWDER_ANALYSIS_PHASE_FINE_KP;
    CHECK(report(&tuner, TARGET - FINE_STOP - 0.01f, 5.0f, 8.0f) == POWDER_ANALYSIS_EVAL_UNDER);
    tuner.phase = POWDER_ANALYSIS_PHASE_FINE_KP;
    CHECK(report(&tuner, TARGET, 5.0f, 15.0f) == POWDER_ANALYSIS_EVAL_TOO_SLOW);

    // VERIFY
    tuner.phase = POWDER_ANALYSIS_PHASE_VERIFY;
    CHECK(report(&tuner, TARGET, 5.0f, 14.9f) == POWDER_ANALYSIS_EVAL_HIT);
    tuner.phase = POWDER_ANALYSIS_PHASE_VERIFY;
    CHECK(report(&tuner, TARGET - FINE_STOP - 0.01f, 5.0f, 8.0f) == POWDER_ANALYSIS_EVAL_UNDER);
    tuner.phase = POWDER_ANALYSIS_PHASE_VERIFY;
    CHECK(report(&tuner, TARGET + FINE_STOP + 0.01f, 5.0f, 8.0f) == POWDER_ANALYSIS_EVAL_OVER);
    tuner.phase = POWDER_ANALYSIS_PHASE_VERIFY;
    CHECK(report(&tuner, TARGET, 5.0f, 15.0f) == POWDER_ANALYSIS_EVAL_TOO_SLOW);
}


static void test_kp_search_respects_time() {
    printf("test_kp_search_respects_time\n");

    powder_analysis_tuner_t tuner;
    powder_analysis_trial_params_t params;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);

    // A timed out coarse throw means "not met": Kp is doubled
    report(&tuner, STOP_POINT - 2.0f, 30.0f, 30.0f, true);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_COARSE_KP);
    CHECK_NEAR(params.coarse_kp, tuner.coarse_kp_start * 2.0f, 1e-6f);
    CHECK(tuner.trials[0].evaluation == POWDER_ANALYSIS_EVAL_TIMEOUT);

    // An overthrow that is too slow (no room for the coarse Kd) is "not met" as well: Kp is doubled again
    report(&tuner, STOP_POINT + 0.5f, 8.0f, 8.0f);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.coarse_kp, tuner.coarse_kp_start * 4.0f, 1e-6f);

    // Overthrow and fast enough: met, the search starts bisecting and the base time is remembered
    report(&tuner, STOP_POINT + 0.5f, 7.0f, 7.0f);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.coarse_kp, tuner.coarse_kp_start * 3.0f, 1e-6f);     // bisection between x2 and x4
    CHECK_NEAR(tuner.coarse_base_time_s, 7.0f, 1e-6f);

    // Fine Kp with a powder whose fine phase is slow: the search doubles towards a faster Kp until
    // the throw finishes within the fine target time
    simulated_powder_t powder;
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    powder.fine_time_base = 20.0f;      // fine Kp 3.0 needs 6.7 s + coarse, 1.5 needs 13.3 s + coarse
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_VERIFY);

    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_VERIFY);
    CHECK(count_slow_throws(&tuner, POWDER_ANALYSIS_PHASE_FINE_KP) > 0);      // the search touched the 13 s boundary
    CHECK(tuner.fine_kp > 0.0f);
    CHECK(tuner.total_time_s <= POWDER_ANALYSIS_FINE_TARGET_TIME_S);
    powder_analysis_result_t slow_result;
    powder_analysis_tuner_get_result(&tuner, &slow_result);
    CHECK(!slow_result.total_time_goal_missed);
}


// Run until the coarse Kd phase and hand-feed coarse times (all within the 10 s goal unless stated)
static void enter_coarse_kd_phase(powder_analysis_tuner_t *tuner, simulated_powder_t *powder) {
    powder_analysis_tuner_init(tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(powder, TARGET, COARSE_STOP, FINE_STOP);
    run_until_phase(tuner, powder, POWDER_ANALYSIS_PHASE_COARSE_KD);
    CHECK(tuner->phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
}


static void test_coarse_kd_time_plateau_stops_early() {
    printf("test_coarse_kd_time_plateau_stops_early\n");

    // Kd has no effect on the coarse time at all: the second throw already shows the plateau
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    powder.coarse_kd_slowdown = 0.0f;

    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(count_phase_trials(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD) == 2);
    CHECK_NEAR(tuner.coarse_kd_tried[0], POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);
    CHECK_NEAR(tuner.coarse_kd_tried[1], POWDER_ANALYSIS_COARSE_KD_START * 2.0f, 1e-6f);

    // Equal times: the smallest Kd wins
    CHECK_NEAR(tuner.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);
    CHECK(tuner.coarse_kd_found);
    CHECK_NEAR(tuner.coarse_time_s, powder.coarse_time_base / tuner.coarse_kp, 1e-4f);
}


static void test_coarse_kd_time_goal_exceeded_stops() {
    printf("test_coarse_kd_time_goal_exceeded_stops\n");

    // Even the start value slows the throw far too much (deterministic powder, so the retries do not help):
    // start value + 2 retries, then the coarse Kd is given up
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    powder.coarse_kd_slowdown = 100.0f;

    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(count_phase_trials(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD) == 1 + POWDER_ANALYSIS_COARSE_KD_MAX_RETRIES);
    CHECK(count_evaluations(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD, POWDER_ANALYSIS_EVAL_TOO_SLOW) == 1 + POWDER_ANALYSIS_COARSE_KD_MAX_RETRIES);
    CHECK_NEAR(tuner.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);
    CHECK(!tuner.coarse_kd_found);

    run_analysis(&tuner, &powder);
    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.coarse_kd_not_found);
    CHECK(result.coarse_time_s > POWDER_ANALYSIS_COARSE_TIME_GOAL_S);

    // Budget exceeded on a later throw: stops there, the earlier ones are still candidates
    enter_coarse_kd_phase(&tuner, &powder);
    report(&tuner, STOP_POINT + 0.5f, 6.0f, 6.0f);     // Kd 0.1
    report(&tuner, STOP_POINT + 0.5f, 6.8f, 6.8f);     // Kd 0.2, +13 %
    report(&tuner, STOP_POINT + 0.5f, 10.5f, 10.5f);   // Kd 0.4, over the 10 s goal
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(tuner.coarse_kd_count == 3);
    CHECK_NEAR(tuner.coarse_kd, 0.1f, 1e-6f);
    CHECK_NEAR(tuner.coarse_time_s, 6.0f, 1e-6f);
    CHECK(tuner.coarse_kd_found);
}


static void test_coarse_kd_retries_start_value_until_ok() {
    printf("test_coarse_kd_retries_start_value_until_ok\n");

    // The first two throws with the start value are too slow (bad luck), the third is fine:
    // probing continues from there as usual
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_trial_params_t params;
    enter_coarse_kd_phase(&tuner, &powder);

    report(&tuner, STOP_POINT + 0.5f, 12.0f, 12.0f);   // Kd 0.1, too slow -> retry 1
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
    CHECK(tuner.coarse_kd_retries == 1);
    CHECK(tuner.coarse_kd_count == 0);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);

    report(&tuner, STOP_POINT + 0.5f, 11.0f, 11.0f);   // Kd 0.1 again, too slow -> retry 2
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
    CHECK(tuner.coarse_kd_retries == 2);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);

    report(&tuner, STOP_POINT + 0.5f, 6.0f, 6.0f);     // Kd 0.1, OK -> doubling goes on
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START * 2.0f, 1e-6f);

    report(&tuner, STOP_POINT + 0.5f, 6.4f, 6.4f);     // Kd 0.2, +7 % -> plateau, done
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(count_phase_trials(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD) == 4);
    CHECK(tuner.coarse_kd_found);
    CHECK_NEAR(tuner.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);
    CHECK_NEAR(tuner.coarse_time_s, 6.0f, 1e-6f);

    // Still too slow after the retries: coarse Kd given up, smallest Kd with the flag
    enter_coarse_kd_phase(&tuner, &powder);
    report(&tuner, STOP_POINT + 0.5f, 12.0f, 12.0f);
    report(&tuner, STOP_POINT + 0.5f, 13.0f, 13.0f);
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
    report(&tuner, STOP_POINT - 1.0f, 30.0f, 30.0f, true);     // timeout counts as too slow as well
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(count_phase_trials(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD) == 1 + POWDER_ANALYSIS_COARSE_KD_MAX_RETRIES);
    CHECK(!tuner.coarse_kd_found);
    CHECK_NEAR(tuner.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START, 1e-6f);
}


static void test_coarse_kd_doubling_cap() {
    printf("test_coarse_kd_doubling_cap\n");

    // Times keep dropping by more than 10 % and stay within the budget: start value + 5 doublings
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    enter_coarse_kd_phase(&tuner, &powder);
    const float times[] = {5.0f, 4.4f, 3.8f, 3.2f, 2.7f, 2.2f, 1.8f};
    for (int i = 0; i < 6; i++) {
        CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
        powder_analysis_trial_params_t params;
        powder_analysis_tuner_get_trial(&tuner, &params);
        CHECK_NEAR(params.coarse_kd, POWDER_ANALYSIS_COARSE_KD_START * (1 << i), 1e-6f);
        report(&tuner, STOP_POINT + 0.5f, times[i], times[i]);
    }
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(tuner.coarse_kd_count == POWDER_ANALYSIS_COARSE_KD_MAX_DOUBLINGS + 1);
    CHECK_NEAR(tuner.coarse_kd, 3.2f, 1e-6f);       // the fastest one
    CHECK_NEAR(tuner.coarse_time_s, 2.2f, 1e-6f);
}


static void test_coarse_kd_choice_lowest_time_with_tie() {
    printf("test_coarse_kd_choice_lowest_time_with_tie\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    enter_coarse_kd_phase(&tuner, &powder);
    report(&tuner, STOP_POINT + 0.5f, 5.0f, 5.0f);     // Kd 0.1
    report(&tuner, STOP_POINT + 0.5f, 4.0f, 4.0f);     // Kd 0.2, -20 %
    report(&tuner, STOP_POINT + 0.9f, 3.85f, 3.85f);   // Kd 0.4, fastest but within 0.2 s of Kd 0.2, and only -3.75 % -> plateau
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK(tuner.coarse_kd_count == 3);
    CHECK_NEAR(tuner.coarse_kd, 0.2f, 1e-6f);       // tie goes to the smaller Kd
    CHECK_NEAR(tuner.coarse_time_s, 4.0f, 1e-6f);
    CHECK_NEAR(tuner.coarse_overshoot_gr, 0.5f, 1e-4f);

    // Without the tie the fastest one wins
    enter_coarse_kd_phase(&tuner, &powder);
    report(&tuner, STOP_POINT + 0.5f, 5.0f, 5.0f);     // Kd 0.1
    report(&tuner, STOP_POINT + 0.5f, 4.0f, 4.0f);     // Kd 0.2
    report(&tuner, STOP_POINT + 0.5f, 3.7f, 3.7f);     // Kd 0.4, -7.5 % -> plateau, but clearly fastest
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
    CHECK_NEAR(tuner.coarse_kd, 0.4f, 1e-6f);
    CHECK_NEAR(tuner.coarse_time_s, 3.7f, 1e-6f);
}


static void test_coarse_kd_noisy_times() {
    printf("test_coarse_kd_noisy_times\n");

    // Coarse times scatter by +/- 20 %: the probing still ends within the cap and picks a tried Kd
    for (uint32_t seed = 1; seed <= 20; seed++) {
        powder_analysis_tuner_t tuner;
        simulated_powder_t powder;
        powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
        simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
        powder.time_noise_amplitude = 0.2f;
        powder.noise_state = seed;

        run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_FINE_KP);
        CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_FINE_KP);
        int throws = count_phase_trials(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD);
        CHECK(throws >= 1 && throws <= POWDER_ANALYSIS_COARSE_KD_MAX_DOUBLINGS + 1);

        bool is_tried_kd = false;
        for (int i = 0; i < tuner.coarse_kd_count; i++) {
            if (tuner.coarse_kd_tried[i] == tuner.coarse_kd) {
                is_tried_kd = true;
            }
        }
        CHECK(is_tried_kd);
        if (tuner.coarse_kd_found) {
            CHECK(tuner.coarse_time_s <= POWDER_ANALYSIS_COARSE_TIME_GOAL_S);
        }
    }
}


static void test_coarse_overshoot_control() {
    printf("test_coarse_overshoot_control\n");

    // A powder that overshoots a lot: the coarse Kd probing is not affected, but the result is flagged
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    powder.coarse_overshoot_base = 5.0f;

    run_analysis(&tuner, &powder);
    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);

    CHECK(result.coarse_kd > 0.0f);
    CHECK(!result.coarse_kd_not_found);
    CHECK(result.coarse_overshoot_too_high);
    CHECK(result.coarse_overshoot_gr > POWDER_ANALYSIS_COARSE_MAX_OVERSHOOT_GR);
    CHECK(count_evaluations(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD, POWDER_ANALYSIS_EVAL_OVER) == 0);
    CHECK(count_evaluations(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KD, POWDER_ANALYSIS_EVAL_OK) > 0);
}


static void test_fine_kp_smallest_within_target_time() {
    printf("test_fine_kp_smallest_within_target_time\n");

    // The start value (3.0) is fast: halve until the throw takes longer than 13 s, then bisect.
    // Simulated: total = coarse 4.14 s + 0.4 s (Kd) + 2 / Kp -> 13 s at Kp 0.236, see fine_kp_time_boundary()
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_VERIFY);
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_VERIFY);

    int first = -1;
    for (int i = 0; i < tuner.trial_count; i++) {
        if (tuner.trials[i].phase == POWDER_ANALYSIS_PHASE_FINE_KP) {
            first = i;
            break;
        }
    }
    CHECK(first >= 0);
    CHECK_NEAR(tuner.trials[first].kp, 3.0f, 1e-6f);
    CHECK_NEAR(tuner.trials[first].kd, 4.0f, 1e-6f);      // matrix Kd from the first fine throw on
    CHECK(tuner.trials[first].total_time_s <= POWDER_ANALYSIS_FINE_TARGET_TIME_S);
    CHECK_NEAR(tuner.trials[first + 1].kp, 1.5f, 1e-6f);     // halved
    CHECK(count_slow_throws(&tuner, POWDER_ANALYSIS_PHASE_FINE_KP) >= 1);

    // Result: the smallest Kp that still finishes within 13 s, within 15 % of the boundary
    float boundary_kp = fine_kp_time_boundary(&tuner, &powder);
    CHECK(tuner.fine_kp >= boundary_kp - 1e-4f);
    CHECK(tuner.fine_kp <= boundary_kp * 1.15f + 1e-4f);
    CHECK(tuner.total_time_s <= POWDER_ANALYSIS_FINE_TARGET_TIME_S);
    CHECK(tuner.total_time_s > POWDER_ANALYSIS_FINE_TARGET_TIME_S / 1.3f);

    // The overthrow does not matter for the condition: a powder that overthrows at every Kp
    // ends with the same (slow, safe) Kp, the verification handles the rest
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    powder.fine_kp_threshold = 0.0f;
    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_VERIFY);
    CHECK(count_evaluations(&tuner, POWDER_ANALYSIS_PHASE_FINE_KP, POWDER_ANALYSIS_EVAL_OVER) >= 3);
    boundary_kp = fine_kp_time_boundary(&tuner, &powder);
    CHECK(tuner.fine_kp >= boundary_kp - 1e-4f);
    CHECK(tuner.fine_kp <= boundary_kp * 1.15f + 1e-4f);

    // A start value that is already too slow doubles towards a faster Kp
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    powder.fine_time_base = 60.0f;      // 3.0 -> 20 s + coarse, 6.0 -> 10 s + coarse
    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_VERIFY);
    first = -1;
    for (int i = 0; i < tuner.trial_count; i++) {
        if (tuner.trials[i].phase == POWDER_ANALYSIS_PHASE_FINE_KP) {
            first = i;
            break;
        }
    }
    CHECK(tuner.trials[first].total_time_s > POWDER_ANALYSIS_FINE_TARGET_TIME_S);    // too slow (labelled OVER, it overthrows too)
    CHECK_NEAR(tuner.trials[first + 1].kp, 6.0f, 1e-6f);     // doubled
    CHECK(tuner.total_time_s <= POWDER_ANALYSIS_FINE_TARGET_TIME_S);
}


static void test_verify_starts_with_found_values() {
    printf("test_verify_starts_with_found_values\n");

    // VERIFY uses the fine Kp as found and the fine Kd from the matrix
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_VERIFY);
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_VERIFY);

    CHECK(tuner.search.finished);
    CHECK_NEAR(tuner.fine_kp, tuner.search.value, 1e-6f);
    CHECK_NEAR(tuner.fine_kd, powder_analysis_fine_kd_start[POWDER_SHAPE_BALL][GRAIN_SIZE_FINE], 1e-6f);
    CHECK_NEAR(tuner.fine_min_flow_speed_rps, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS, 1e-6f);

    powder_analysis_trial_params_t params;
    powder_analysis_tuner_get_trial(&tuner, &params);
    CHECK_NEAR(params.fine_kp, tuner.fine_kp, 1e-6f);
    CHECK_NEAR(params.fine_kd, tuner.fine_kd, 1e-6f);
}


// Run until the verification starts and return the gains it begins with
static void enter_verify(powder_analysis_tuner_t *tuner, simulated_powder_t *powder, float *fine_kp, float *fine_kd) {
    powder_analysis_tuner_init(tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(powder, TARGET, COARSE_STOP, FINE_STOP);
    run_until_phase(tuner, powder, POWDER_ANALYSIS_PHASE_VERIFY);
    CHECK(tuner->phase == POWDER_ANALYSIS_PHASE_VERIFY);
    *fine_kp = tuner->fine_kp;
    *fine_kd = tuner->fine_kd;
}


// The gains of the next throw must be the given ones (a round never changes them)
static void check_next_gains(powder_analysis_tuner_t *tuner, float fine_kp, float fine_kd, float fine_min_speed) {
    powder_analysis_trial_params_t params;
    powder_analysis_tuner_get_trial(tuner, &params);
    CHECK(params.phase == POWDER_ANALYSIS_PHASE_VERIFY);
    CHECK(params.throw_type == POWDER_ANALYSIS_THROW_FULL);
    CHECK_NEAR(params.fine_kp, fine_kp, 1e-6f);
    CHECK_NEAR(params.fine_kd, fine_kd, 1e-6f);
    CHECK_NEAR(params.fine_min_flow_speed_rps, fine_min_speed, 1e-6f);
}


static void test_verify_first_round_passes() {
    printf("test_verify_first_round_passes\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    float kp, kd;
    enter_verify(&tuner, &powder, &kp, &kd);
    CHECK(tuner.verify_round.number == 1);

    for (int i = 0; i < POWDER_ANALYSIS_VERIFY_ROUND_THROWS - 1; i++) {
        check_next_gains(&tuner, kp, kd, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS);
        report(&tuner, TARGET, 5.0f, 7.0f);
        CHECK(!powder_analysis_tuner_is_done(&tuner));
        CHECK(tuner.verify_round.hit_count == i + 1);
    }
    report(&tuner, TARGET, 5.0f, 7.0f);
    CHECK(powder_analysis_tuner_is_done(&tuner));

    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.verified);
    CHECK(result.verify_hits == POWDER_ANALYSIS_VERIFY_ROUND_THROWS);
    CHECK(result.fine_corrections == 0);
    CHECK_NEAR(result.fine_kp, kp, 1e-6f);
    CHECK_NEAR(result.fine_kd, kd, 1e-6f);
    CHECK_NEAR(result.fine_min_flow_speed_rps, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS, 1e-6f);
    CHECK_NEAR(result.total_time_s, 7.0f, 1e-5f);
    CHECK(!result.verify_overthrow_limit_reached);
    CHECK(!result.trial_limit_reached);
    CHECK(!result.total_time_goal_missed);
}


static void test_verify_two_overthrows_lower_fine_min_speed() {
    printf("test_verify_two_overthrows_lower_fine_min_speed\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    float kp, kd;
    enter_verify(&tuner, &powder, &kp, &kd);

    const float min_speed = POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS;
    const float lowered = min_speed * POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR;

    // Round 1: hit, overthrow, hit, overthrow -> given up, fine minimum speed -25 %, gains unchanged
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    check_next_gains(&tuner, kp, kd, min_speed);    // one overthrow changes nothing yet
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    CHECK(!powder_analysis_tuner_is_done(&tuner));
    CHECK(tuner.fine_corrections == 1);
    CHECK(tuner.verify_round.number == 2);
    CHECK(tuner.verify_round.throw_count == 0);
    check_next_gains(&tuner, kp, kd, lowered);

    // The protocol shows the new minimum speed from the next throw on, the gains stay
    report(&tuner, TARGET, 5.0f, 7.0f);
    CHECK_NEAR(tuner.trials[tuner.trial_count - 1].fine_min_flow_speed_rps, lowered, 1e-6f);
    CHECK_NEAR(tuner.trials[tuner.trial_count - 2].fine_min_flow_speed_rps, min_speed, 1e-6f);
    CHECK_NEAR(tuner.trials[tuner.trial_count - 1].kp, kp, 1e-6f);

    // Round 2 passes
    for (int i = 0; i < POWDER_ANALYSIS_VERIFY_ROUND_THROWS - 1; i++) {
        check_next_gains(&tuner, kp, kd, lowered);
        report(&tuner, TARGET, 5.0f, 7.0f);
    }
    CHECK(powder_analysis_tuner_is_done(&tuner));

    // The lowered minimum speed is part of the result (and therefore of the saved profile)
    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.verified);
    CHECK(result.verify_hits == POWDER_ANALYSIS_VERIFY_ROUND_THROWS);
    CHECK(result.fine_corrections == 1);
    CHECK_NEAR(result.fine_kp, kp, 1e-6f);
    CHECK_NEAR(result.fine_kd, kd, 1e-6f);
    CHECK_NEAR(result.fine_min_flow_speed_rps, lowered, 1e-6f);
    CHECK(!result.verify_overthrow_limit_reached);
}


static void test_verify_correction_picks_kp_or_min_speed() {
    printf("test_verify_correction_picks_kp_or_min_speed\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    float kp, kd;
    const float min_speed = POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS;
    const float kp_factor = POWDER_ANALYSIS_FINE_KP_CORRECTION_FACTOR;
    const float speed_factor = POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR;

    // Two overthrows, then check what the next round uses
    auto give_round_up = [&]() {
        report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
        report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    };

    // Kp decides the speed at the stop (4.8 * 0.02 = 0.096 > 0.06): Kp is lowered, min speed stays
    enter_verify(&tuner, &powder, &kp, &kd);
    tuner.fine_kp = 4.8f;
    tuner.verify_round.fine_kp = 4.8f;
    give_round_up();
    CHECK(tuner.fine_corrections == 1);
    CHECK(tuner.last_correction_lowered_kp);
    check_next_gains(&tuner, 4.8f * kp_factor, kd, min_speed);
    give_round_up();                                        // 4.32 * 0.02 = 0.086 > 0.06: Kp again
    CHECK(tuner.last_correction_lowered_kp);
    check_next_gains(&tuner, 4.8f * kp_factor * kp_factor, kd, min_speed);

    // Min speed decides (0.65 * 0.02 = 0.013 < 0.06): min speed is lowered, Kp stays
    enter_verify(&tuner, &powder, &kp, &kd);
    CHECK(kp * FINE_STOP < min_speed);
    give_round_up();
    CHECK(tuner.fine_corrections == 1);
    CHECK(!tuner.last_correction_lowered_kp);
    check_next_gains(&tuner, kp, kd, min_speed * speed_factor);

    // Mixed: 3.2 * 0.02 = 0.064 > 0.06 -> Kp (2.88 -> 0.0576 < 0.06) -> min speed (0.045) -> Kp again
    enter_verify(&tuner, &powder, &kp, &kd);
    tuner.fine_kp = 3.2f;
    tuner.verify_round.fine_kp = 3.2f;
    give_round_up();
    CHECK(tuner.last_correction_lowered_kp);
    check_next_gains(&tuner, 3.2f * kp_factor, kd, min_speed);
    give_round_up();
    CHECK(!tuner.last_correction_lowered_kp);
    check_next_gains(&tuner, 3.2f * kp_factor, kd, min_speed * speed_factor);
    give_round_up();
    CHECK(tuner.last_correction_lowered_kp);
    CHECK(tuner.fine_corrections == 3);
    check_next_gains(&tuner, 3.2f * kp_factor * kp_factor, kd, min_speed * speed_factor);

    // The fourth round with two overthrows ends the analysis with the best round so far
    give_round_up();
    CHECK(powder_analysis_tuner_is_done(&tuner));
    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.verify_overthrow_limit_reached);
    CHECK(result.fine_corrections == 3);
}


static void test_verify_one_overthrow_ends_round_without_correction() {
    printf("test_verify_one_overthrow_ends_round_without_correction\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    float kp, kd;

    // One overthrow and otherwise hits: the round runs to its end, 9/10 counts as verified, no correction
    enter_verify(&tuner, &powder, &kp, &kd);
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    for (int i = 0; i < 7; i++) {
        report(&tuner, TARGET, 5.0f, 8.0f);
    }
    CHECK(!powder_analysis_tuner_is_done(&tuner));
    check_next_gains(&tuner, kp, kd, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS);
    report(&tuner, TARGET, 5.0f, 8.0f);
    CHECK(powder_analysis_tuner_is_done(&tuner));

    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.verified);
    CHECK(result.verify_reached);
    CHECK(result.verify_hits == 9);
    CHECK(result.fine_corrections == 0);
    CHECK(!result.verify_overthrow_limit_reached);
    CHECK_NEAR(result.fine_kp, kp, 1e-6f);
    CHECK_NEAR(result.total_time_s, 8.0f, 1e-5f);

    // One overthrow and one slow throw: 8/10, not verified, no correction
    enter_verify(&tuner, &powder, &kp, &kd);
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    report(&tuner, TARGET, 5.0f, 20.0f);                    // too slow
    for (int i = 0; i < 6; i++) {
        report(&tuner, TARGET, 5.0f, 8.0f);
    }
    CHECK(!powder_analysis_tuner_is_done(&tuner));
    report(&tuner, TARGET, 5.0f, 8.0f);
    CHECK(powder_analysis_tuner_is_done(&tuner));
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(!result.verified);
    CHECK(result.verify_hits == 8);
    CHECK(result.fine_corrections == 0);

    // Slow / under / timeout misses end the round the same way, 7/10 here
    enter_verify(&tuner, &powder, &kp, &kd);
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET, 5.0f, 20.0f);                    // too slow
    report(&tuner, TARGET - 0.1f, 5.0f, 8.0f);              // under
    report(&tuner, TARGET - 1.0f, 30.0f, 30.0f, true);      // timeout
    for (int i = 0; i < 5; i++) {
        report(&tuner, TARGET, 5.0f, 8.0f);
    }
    CHECK(!powder_analysis_tuner_is_done(&tuner));
    report(&tuner, TARGET, 5.0f, 8.0f);
    CHECK(powder_analysis_tuner_is_done(&tuner));
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(!result.verified);
    CHECK(result.verify_hits == 7);
    CHECK(result.fine_corrections == 0);
}


static void test_exact_scale_step() {
    printf("test_exact_scale_step\n");

    // Multiples of the 0.02 gr resolution are fragile, values halfway between two steps are not
    CHECK(powder_analysis_is_exact_scale_step(0.02f));
    CHECK(powder_analysis_is_exact_scale_step(0.04f));
    CHECK(powder_analysis_is_exact_scale_step(0.1f));
    CHECK(!powder_analysis_is_exact_scale_step(0.03f));
    CHECK(!powder_analysis_is_exact_scale_step(0.05f));
    CHECK(!powder_analysis_is_exact_scale_step(0.015f));
    CHECK(!powder_analysis_is_exact_scale_step(0.0f));
}


static void test_fall_after_stop_statistics() {
    printf("test_fall_after_stop_statistics\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    float kp, kd;
    const float stop = TARGET - FINE_STOP;      // the fine trickler stops at target - threshold

    // Five verification throws with falls 0.03, 0.01, 0.02, 0.056, 0.016 -> median 0.02 = one scale step:
    // recommended threshold halfway to the next step (0.03), throws land on the target on average
    enter_verify(&tuner, &powder, &kp, &kd);
    report(&tuner, stop + 0.03f, 5.0f, 8.0f, false, stop);
    report(&tuner, stop + 0.01f, 5.0f, 8.0f, false, stop);
    report(&tuner, stop + 0.02f, 5.0f, 8.0f, false, stop);
    report(&tuner, stop + 0.056f, 5.0f, 8.0f, false, stop);     // an overthrow, the fall still counts
    report(&tuner, stop + 0.016f, 5.0f, 8.0f, false, stop);
    CHECK(!powder_analysis_tuner_is_done(&tuner));          // a round has 10 throws, the statistics work anytime

    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.fall_count == 5);
    CHECK_NEAR(result.fall_median_gr, 0.02f, 1e-5f);
    CHECK_NEAR(result.fall_max_gr, 0.056f, 1e-5f);
    CHECK_NEAR(result.recommended_fine_threshold, 0.03f, 1e-5f);
    CHECK_NEAR(result.expected_final_offset_gr, 0.0f, 1e-5f);
    CHECK(tuner.trials[tuner.trial_count - 1].has_fall);
    CHECK_NEAR(tuner.trials[tuner.trial_count - 1].fall_gr, 0.016f, 1e-5f);

    // Rounding of the fall to whole scale steps (half up), at least one step:
    //   0.028 -> 1 step -> 0.03, lands +0.008   0.016 -> 1 step -> 0.03, lands -0.004
    //   0.004 -> 1 step (minimum) -> 0.03       0.07 -> 3.5 -> 4 steps -> 0.09, lands -0.010
    const float falls[] = {0.028f, 0.016f, 0.004f, 0.07f};
    const float thresholds[] = {0.03f, 0.03f, 0.03f, 0.09f};
    const float offsets[] = {0.008f, -0.004f, -0.016f, -0.01f};
    for (int i = 0; i < 4; i++) {
        enter_verify(&tuner, &powder, &kp, &kd);
        for (int n = 0; n < 5; n++) {
            report(&tuner, stop + falls[i], 5.0f, 8.0f, false, stop);
        }
        powder_analysis_tuner_get_result(&tuner, &result);
        CHECK_NEAR(result.fall_median_gr, falls[i], 1e-5f);
        CHECK_NEAR(result.recommended_fine_threshold, thresholds[i], 1e-5f);
        CHECK_NEAR(result.expected_final_offset_gr, offsets[i], 1e-5f);
    }

    // Throws without a stop weight (coarse-only, timeout) or an unknown stop weight carry no fall
    enter_verify(&tuner, &powder, &kp, &kd);
    report(&tuner, TARGET - 1.0f, 30.0f, 30.0f, true, stop);    // timeout
    report(&tuner, TARGET, 5.0f, 8.0f);                         // stop weight unknown
    CHECK(!tuner.trials[tuner.trial_count - 2].has_fall);
    CHECK(!tuner.trials[tuner.trial_count - 1].has_fall);
    CHECK(!tuner.trials[0].has_fall);                           // the first coarse-only throw

    // No verification throw with a fall: the fine Kp throws are used instead
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_BALL, GRAIN_SIZE_FINE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_VERIFY);
    tuner.trial_count = POWDER_ANALYSIS_MAX_TRIALS - 1;         // the limit ends the analysis after one verify throw
    report(&tuner, TARGET - 1.0f, 30.0f, 30.0f, true);          // a timeout, no fall
    CHECK(powder_analysis_tuner_is_done(&tuner));
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.fall_count > 0);                               // from the FINE_KP throws
    CHECK(result.recommended_fine_threshold >= 1.5f * POWDER_ANALYSIS_SCALE_RESOLUTION_GR - 1e-6f);
}


static void test_verify_correction_limit_picks_best_round() {
    printf("test_verify_correction_limit_picks_best_round\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    float kp, kd;
    enter_verify(&tuner, &powder, &kp, &kd);

    // Round 1 (Kp): 3 hits, then 2 overthrows
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    CHECK(tuner.fine_corrections == 1);

    // Round 2 (min speed * 0.75): 3 hits as well but faster, then 2 overthrows -> this is the best round
    report(&tuner, TARGET, 5.0f, 6.0f);
    report(&tuner, TARGET, 5.0f, 6.0f);
    report(&tuner, TARGET, 5.0f, 6.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 6.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 6.0f);
    CHECK(tuner.fine_corrections == 2);

    // Round 3 (min speed * 0.56): 1 hit, 2 overthrows -> third and last correction
    report(&tuner, TARGET, 5.0f, 5.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 5.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 5.0f);
    CHECK(tuner.fine_corrections == 3);
    CHECK(!powder_analysis_tuner_is_done(&tuner));
    check_next_gains(&tuner, kp, kd, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS * POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR * POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR * POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR);

    // Round 4 (min speed * 0.42): 2 overthrows again -> no further correction, best round wins
    report(&tuner, TARGET + 0.1f, 5.0f, 5.0f);
    CHECK(!powder_analysis_tuner_is_done(&tuner));
    report(&tuner, TARGET + 0.1f, 5.0f, 5.0f);
    CHECK(powder_analysis_tuner_is_done(&tuner));

    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(!result.verified);
    CHECK(result.verify_reached);
    CHECK(result.verify_overthrow_limit_reached);
    CHECK(result.fine_corrections == 3);
    CHECK(result.verify_hits == 3);
    CHECK_NEAR(result.fine_kp, kp, 1e-6f);                 // gains never changed
    CHECK_NEAR(result.fine_kd, kd, 1e-6f);
    CHECK_NEAR(result.fine_min_flow_speed_rps, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS * POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR, 1e-6f);     // round 2
    CHECK_NEAR(result.total_time_s, 6.0f, 1e-5f);
    CHECK(!result.trial_limit_reached);
}


static void test_doubling_cap_ends_coarse_kp_search() {
    printf("test_doubling_cap_ends_coarse_kp_search\n");

    // A powder that never overthrows: coarse Kp doubles up to the cap, then the search ends with
    // the largest Kp tried and the analysis goes on (instead of burning all 60 trials)
    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_STICK, GRAIN_SIZE_COARSE, TARGET, COARSE_STOP, FINE_STOP);
    simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
    powder.coarse_kp_threshold = 1e30f;

    run_until_phase(&tuner, &powder, POWDER_ANALYSIS_PHASE_COARSE_KD);
    CHECK(tuner.phase == POWDER_ANALYSIS_PHASE_COARSE_KD);
    CHECK(count_phase_trials(&tuner, POWDER_ANALYSIS_PHASE_COARSE_KP) == POWDER_ANALYSIS_MAX_DOUBLING_STEPS + 1);
    CHECK_NEAR(tuner.coarse_kp, 0.12f * 64.0f, 1e-4f);
    CHECK_NEAR(tuner.trials[tuner.trial_count - 1].kp, 0.12f * 64.0f, 1e-4f);

    run_analysis(&tuner, &powder);
    CHECK(powder_analysis_tuner_is_done(&tuner));
    CHECK(tuner.trial_count < POWDER_ANALYSIS_MAX_TRIALS);

    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(!result.trial_limit_reached);
    CHECK_NEAR(result.coarse_kp, 0.12f * 64.0f, 1e-4f);

    // Reporting after done changes nothing
    uint8_t trials_before = tuner.trial_count;
    report(&tuner, TARGET, 5.0f, 5.0f);
    CHECK(tuner.trial_count == trials_before);
}


static void test_trial_limit_in_verify() {
    printf("test_trial_limit_in_verify\n");

    powder_analysis_tuner_t tuner;
    simulated_powder_t powder;
    float kp, kd;
    enter_verify(&tuner, &powder, &kp, &kd);

    // Pretend the searches took much longer, so the limit hits during the second round
    tuner.trial_count = POWDER_ANALYSIS_MAX_TRIALS - 7;

    // Round 1: 2 hits, 2 overthrows -> correction
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    report(&tuner, TARGET + 0.1f, 5.0f, 8.0f);
    CHECK(tuner.fine_corrections == 1);

    // Round 2: 3 hits, then the trial limit ends the analysis
    report(&tuner, TARGET, 5.0f, 7.0f);
    report(&tuner, TARGET, 5.0f, 7.0f);
    CHECK(!powder_analysis_tuner_is_done(&tuner));
    report(&tuner, TARGET, 5.0f, 7.0f);
    CHECK(powder_analysis_tuner_is_done(&tuner));
    CHECK(tuner.trial_count == POWDER_ANALYSIS_MAX_TRIALS);

    // Best round = round 2 (3 hits), its gains are the result
    powder_analysis_result_t result;
    powder_analysis_tuner_get_result(&tuner, &result);
    CHECK(result.trial_limit_reached);
    CHECK(!result.verified);
    CHECK(result.verify_reached);
    CHECK(result.verify_hits == 3);
    CHECK_NEAR(result.fine_kp, kp, 1e-6f);
    CHECK_NEAR(result.fine_min_flow_speed_rps, POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS * POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR, 1e-6f);
    CHECK_NEAR(result.total_time_s, 7.0f, 1e-5f);
}


static void test_with_noise() {
    printf("test_with_noise\n");

    // A noisy powder must still lead to a finished analysis within the limits, with any seed
    for (uint32_t seed = 1; seed <= 20; seed++) {
        powder_analysis_tuner_t tuner;
        simulated_powder_t powder;
        powder_analysis_tuner_init(&tuner, POWDER_SHAPE_STICK, GRAIN_SIZE_MEDIUM, TARGET, COARSE_STOP, FINE_STOP);
        simulated_powder_init_default(&powder, TARGET, COARSE_STOP, FINE_STOP);
        powder.noise_amplitude = 0.2f;
        powder.time_noise_amplitude = 0.2f;
        powder.noise_state = seed;

        run_analysis(&tuner, &powder);

        CHECK(powder_analysis_tuner_is_done(&tuner));
        CHECK(tuner.trial_count <= POWDER_ANALYSIS_MAX_TRIALS);

        powder_analysis_result_t result;
        powder_analysis_tuner_get_result(&tuner, &result);
        CHECK(result.verify_reached);
        CHECK(result.coarse_kp > powder.coarse_kp_threshold);
        CHECK(result.coarse_kd > 0.0f);
        // Smallest Kp within the fine target time: a loose bound only, the search sees +-20 % time
        // noise on every throw (corrections may lower the Kp further)
        float coarse_time_s = powder.coarse_time_base / result.coarse_kp + powder.coarse_kd_slowdown * result.coarse_kd;
        float boundary_kp = powder.fine_time_base / (POWDER_ANALYSIS_FINE_TARGET_TIME_S - coarse_time_s - powder.fine_kd_slowdown * result.fine_kd);
        CHECK(result.fine_kp > 0.0f);
        CHECK(result.fine_kp <= boundary_kp * 1.5f);
        if (checks_failed) {
            printf("  seed %u: %d trials\n", seed, result.trial_count);
            print_protocol(&tuner);
            break;
        }
    }
}


int main() {
    test_search_smallest_start_too_low();
    test_search_smallest_start_too_high();
    test_search_halving_limit();
    test_search_doubling_limit();
    test_search_bisection_limit();
    test_start_values_from_matrix();
    test_invalid_shape_and_grain_fall_back();
    test_all_phases_in_order();
    test_evaluations();
    test_kp_search_respects_time();
    test_coarse_kd_time_plateau_stops_early();
    test_coarse_kd_time_goal_exceeded_stops();
    test_coarse_kd_retries_start_value_until_ok();
    test_coarse_kd_doubling_cap();
    test_coarse_kd_choice_lowest_time_with_tie();
    test_coarse_kd_noisy_times();
    test_coarse_overshoot_control();
    test_fine_kp_smallest_within_target_time();
    test_verify_starts_with_found_values();
    test_verify_first_round_passes();
    test_verify_two_overthrows_lower_fine_min_speed();
    test_verify_correction_picks_kp_or_min_speed();
    test_verify_one_overthrow_ends_round_without_correction();
    test_exact_scale_step();
    test_fall_after_stop_statistics();
    test_verify_correction_limit_picks_best_round();
    test_doubling_cap_ends_coarse_kp_search();
    test_trial_limit_in_verify();
    test_with_noise();

    printf("\n%d checks, %d failed\n", checks_total, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
