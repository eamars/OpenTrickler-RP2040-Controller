#ifndef POWDER_ANALYSIS_TUNER_H_
#define POWDER_ANALYSIS_TUNER_H_

// Search logic of the powder analysis: finds coarse/fine Kp and Kd for a powder by running
// test throws and evaluating their results.
//
// This file contains no hardware access so it can be unit tested on a PC (see tests/powder_analysis).
// The caller (powder_analysis.cpp) owns the hardware and drives the tuner like this:
//
//     powder_analysis_tuner_init(&tuner, shape, grain_size, target, coarse_stop, fine_stop);
//     while (!powder_analysis_tuner_is_done(&tuner)) {
//         powder_analysis_tuner_get_trial(&tuner, &params);   // which gains to use, what kind of throw
//         ... run the throw with these gains ...
//         powder_analysis_tuner_report_result(&tuner, &result);
//     }
//     powder_analysis_tuner_get_result(&tuner, &profile_values);
//
// Phases (see docs/powder_analysis_concept.md, section 4):
//     COARSE_KP   smallest coarse Kp that overthrows the coarse stop point and leaves
//                 room for the coarse Kd (coarse time <= 10 s / braking factor)     (coarse-only throws)
//     COARSE_KD   probing: start value doubled a few times, the fastest Kd within
//                 the 10 s coarse time goal is taken (coarse times scatter, so the
//                 probing stops early when they no longer change), see the .cpp       (coarse-only throws)
//     FINE_KP     smallest fine Kp whose full throw finishes within 13 s              (full throws)
//     VERIFY      rounds of 10 throws with fixed gains, 9 must hit; a round with 2
//                 overthrows is given up and fine Kp or the fine minimum speed is
//                 lowered (max 3 times)                                                (full throws)
//     DONE
//
// The fine Kd is not searched, it comes from the matrix in the config (see there why). The Kp
// searches use the same bracketing search (double / halve, then bisect); the coarse Kd is probed,
// see update_coarse_kd_phase() in the .cpp.

#include <stdint.h>
#include <stdbool.h>
#include "powder_analysis_config.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    POWDER_ANALYSIS_PHASE_COARSE_KP = 0,
    POWDER_ANALYSIS_PHASE_COARSE_KD = 1,
    POWDER_ANALYSIS_PHASE_FINE_KP = 2,
    POWDER_ANALYSIS_PHASE_VERIFY = 3,
    POWDER_ANALYSIS_PHASE_DONE = 4,
} powder_analysis_phase_t;

typedef enum {
    // Only the coarse phase is of interest: the fine trickler runs with all gains 0 (minimum speed)
    // and the throw ends as soon as the coarse trickler has stopped.
    POWDER_ANALYSIS_THROW_COARSE_ONLY = 0,

    // Realistic throw exactly like in charge mode: coarse, then fine until the target is reached.
    POWDER_ANALYSIS_THROW_FULL = 1,
} powder_analysis_throw_type_t;

// How a single throw was rated. Which values can occur depends on the phase:
//   COARSE_KP: UNDER, OVER (= overthrow and fast enough), TOO_SLOW (overthrow but too slow), TIMEOUT
//   COARSE_KD: OK (fast enough), TOO_SLOW, TIMEOUT
//   FINE_KP and VERIFY: HIT, UNDER, OVER, TOO_SLOW, TIMEOUT
typedef enum {
    POWDER_ANALYSIS_EVAL_UNDER = 0,     // weight at or below the coarse stop point, or below the fine target tolerance
    POWDER_ANALYSIS_EVAL_OVER = 1,      // overthrow
    POWDER_ANALYSIS_EVAL_HIT = 2,       // within target +/- fine_stop_threshold and total time goal met
    POWDER_ANALYSIS_EVAL_TOO_SLOW = 3,  // the time limit of the phase was missed
    POWDER_ANALYSIS_EVAL_OK = 4,        // coarse Kd throw: fast enough
    POWDER_ANALYSIS_EVAL_TIMEOUT = 5,   // the throw hit the safety timeout
} powder_analysis_evaluation_t;


// What the caller has to use for the next throw. Ki is always 0.
typedef struct {
    uint8_t trial_number;                       // 1-based, for display
    powder_analysis_phase_t phase;
    powder_analysis_throw_type_t throw_type;

    float coarse_kp;
    float coarse_kd;
    float fine_kp;                              // 0 for coarse-only throws
    float fine_kd;                              // 0 for coarse-only throws
    float fine_min_flow_speed_rps;              // start value from the config, lowered by the verification
} powder_analysis_trial_params_t;

// What the caller measured during the throw
typedef struct {
    float weight;                               // final (stable) weight in gr
    float coarse_time_s;                        // duration until the coarse trickler stopped
    float total_time_s;                         // duration of the whole throw (equals coarse_time_s for coarse-only throws)
    bool timed_out;                             // throw was aborted by the safety timeout, weight and times are of that moment
    float stop_weight;                          // weight of the sample that triggered the fine stop, 0 if that never happened
} powder_analysis_trial_result_t;

// One entry of the trial protocol (shown in the web UI, exported as CSV by the browser)
typedef struct {
    uint8_t number;                             // 1-based
    powder_analysis_phase_t phase;
    float kp;                                   // the gains used by the trickler searched in this phase:
    float kd;                                   // coarse gains in the coarse phases, fine gains otherwise
    float weight;
    float coarse_time_s;
    float total_time_s;
    powder_analysis_evaluation_t evaluation;
    float fine_min_flow_speed_rps;              // the fine minimum speed used by this throw
    bool has_fall;                              // full throw that reached the fine stop and was not timed out
    float fall_gr;                              // settled weight - weight at the fine stop (what still fell afterwards)
} powder_analysis_trial_t;

// Final result: the 10 profile values plus information on what was (not) achieved
typedef struct {
    float coarse_kp;
    float coarse_ki;                            // always 0
    float coarse_kd;
    float coarse_min_flow_speed_rps;
    float coarse_max_flow_speed_rps;

    float fine_kp;
    float fine_ki;                              // always 0
    float fine_kd;
    float fine_min_flow_speed_rps;
    float fine_max_flow_speed_rps;

    bool verified;                              // a round of 10 throws hit at least 9 times
    bool verify_reached;                        // false if the analysis ended during a search phase
    uint8_t verify_hits;                        // hits of the best verification round (0..10)
    uint8_t fine_corrections;                   // verification corrections (fine Kp -10 % or fine minimum speed -25 %)

    // Why the analysis ended without verification (all false when verified)
    bool trial_limit_reached;                   // 60 trial limit
    bool verify_overthrow_limit_reached;        // the round after the last allowed correction still had 2 overthrows

    // What was not achieved with the found values ("missed" also when it was never measured)
    bool coarse_time_goal_missed;               // coarse throw with the found coarse Kd takes longer than 10 s
    bool coarse_kd_not_found;                // no coarse Kd within the coarse time goal, the smallest Kd tried is used
    bool coarse_overshoot_too_high;             // coarse throw with the found coarse Kd overshoots the stop point by > 1.0 gr
    bool total_time_goal_missed;                // throws with the found values take 15 s or longer

    float coarse_base_time_s;                   // coarse time with the found Kp and Kd 0 (0 = none)
    float coarse_time_s;                        // coarse time with the found coarse Kp and Kd (0 = none)
    float coarse_overshoot_gr;                  // overshoot over the coarse stop point with the found coarse Kp and Kd
    float total_time_s;                         // average total time of the hits of the best verification round;
                                                // before the verification: time of the found fine Kp's throw (0 = none)
    uint8_t trial_count;

    // Fall after the stop (see compute_fall_statistics in the .cpp), hint for the fine stop threshold
    uint8_t fall_count;                         // throws the statistics are based on (0 = none)
    float fall_median_gr;
    float fall_max_gr;
    float recommended_fine_threshold;           // (n + 0.5) * scale resolution, see compute_fall_statistics(); 0 when no fall was measured
    float expected_final_offset_gr;             // where throws land on average with that threshold: fall median - n * resolution
} powder_analysis_result_t;


// Generic search for the smallest value that meets a condition which is monotonic in the value
// (if a value meets it, every larger value does too), e.g. the smallest Kp that overthrows:
//   1. Double while nothing met yet, halve while everything met so far (both bounded).
//   2. Bisect between the largest "not met" and the smallest "met" value until the interval is
//      small enough or the step limit is reached.
//   3. Result = the smallest "met" value.
// Public only so the unit tests can exercise it directly.
typedef struct {
    float value;                                // value to try next (or the result once finished)
    bool has_met;
    float met_value;                            // smallest value known to meet the condition
    bool has_not_met;
    float not_met_value;                        // largest value known not to meet the condition
    uint8_t doubling_steps;
    uint8_t halving_steps;
    uint8_t bisection_steps;
    bool finished;
} powder_analysis_search_t;

// One verification round: up to 10 throws with fixed values (fine Kd never changes, it is not kept here)
typedef struct {
    uint8_t number;                             // 1-based
    float fine_kp;
    float fine_min_flow_speed_rps;
    uint8_t throw_count;
    uint8_t hit_count;
    uint8_t overthrow_count;
    float hit_time_sum_s;                       // for the average total time of the hits (tie breaker)
} powder_analysis_verify_round_t;

typedef struct {
    // Given at init, in gr
    float target_weight;
    float coarse_stop_threshold;
    float fine_stop_threshold;
    float coarse_kp_start;
    float fine_kp_start;

    powder_analysis_phase_t phase;

    // Gains found so far. The gain of the phase currently being searched is taken from `search`.
    float coarse_kp;
    float coarse_kd;
    float fine_kp;
    float fine_kd;                              // from the matrix, never searched
    float fine_min_flow_speed_rps;              // start value from the config, lowered by the verification

    powder_analysis_search_t search;            // search of the current phase (COARSE_KP, FINE_KP)

    // Coarse Kd probing (COARSE_KD): every Kd tried with what its throw measured
    uint8_t coarse_kd_count;
    uint8_t coarse_kd_retries;                      // restarts of the probing because the start value was too slow
    float coarse_kd_tried[POWDER_ANALYSIS_COARSE_KD_MAX_DOUBLINGS + 1];
    float coarse_kd_time_s[POWDER_ANALYSIS_COARSE_KD_MAX_DOUBLINGS + 1];
    float coarse_kd_overshoot_gr[POWDER_ANALYSIS_COARSE_KD_MAX_DOUBLINGS + 1];

    // Measured with the throws of the found values, 0 until measured
    float coarse_base_time_s;                   // coarse time of the found Kp with Kd 0
    float coarse_time_s;                        // coarse time of the found Kp with the found Kd
    float coarse_overshoot_gr;                  // overshoot over the stop point with the found Kd
    bool coarse_kd_found;                    // a coarse Kd within the coarse time goal was found
    float total_time_s;                         // total time of a full throw with the found values

    // Verification
    bool verify_reached;
    bool verified;
    uint8_t fine_corrections;                   // rounds given up because of overthrows, each lowered fine Kp or the fine minimum speed
    bool last_correction_lowered_kp;            // what the last correction did (for the display)
    bool verify_overthrow_limit_reached;
    powder_analysis_verify_round_t verify_round;        // the running (or last) round
    powder_analysis_verify_round_t verify_best_round;   // most hits, on a tie the faster one

    bool trial_limit_reached;

    // Trial protocol
    uint8_t trial_count;
    powder_analysis_trial_t trials[POWDER_ANALYSIS_MAX_TRIALS];
} powder_analysis_tuner_t;


// Tuner interface
void powder_analysis_tuner_init(powder_analysis_tuner_t *tuner,
                                powder_shape_t shape, grain_size_t grain_size,
                                float target_weight, float coarse_stop_threshold, float fine_stop_threshold);
void powder_analysis_tuner_get_trial(const powder_analysis_tuner_t *tuner, powder_analysis_trial_params_t *params);
powder_analysis_evaluation_t powder_analysis_tuner_report_result(powder_analysis_tuner_t *tuner,
                                                                 const powder_analysis_trial_result_t *result);
bool powder_analysis_tuner_is_done(const powder_analysis_tuner_t *tuner);
void powder_analysis_tuner_get_result(const powder_analysis_tuner_t *tuner, powder_analysis_result_t *result);
bool powder_analysis_is_exact_scale_step(float threshold);     // fragile fine stop threshold, see the .cpp

// Names for the UI
const char * powder_analysis_phase_to_string(powder_analysis_phase_t phase);
const char * powder_analysis_evaluation_to_string(powder_analysis_evaluation_t evaluation);

// Bracketing search (used internally, public for the unit tests)
void powder_analysis_search_init(powder_analysis_search_t *search, float start_value);
void powder_analysis_search_update(powder_analysis_search_t *search, bool condition_met);
float powder_analysis_search_best_value(const powder_analysis_search_t *search);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // POWDER_ANALYSIS_TUNER_H_
