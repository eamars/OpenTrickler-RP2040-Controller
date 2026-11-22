#ifndef CHARGE_PID_LEARNT_H_
#define CHARGE_PID_LEARNT_H_

/*
    Today's PID loop with the glide's learning and no calibration (ADR-009, the owner's question of 2026-11-22 evening;
    an experiment, not a plan: research/pid_learning_2026-11-22.md). A port of tools/charge_sim/legacy_pid.py's
    LearntPid, kept in its order.

    The profile's PID settings run both tubes as today's loop does, once a reading. The fine PID acts on a forecast:
    the reading plus the fine tube's turns over the last lag_s at one figure, learnt at each throw's first rest from
    what landed after the fine tube stopped. The fine tube stops when the forecast is the aim short of the target; the
    verdict is one settled reading; a throw short by more than the band goes back to the stream.

    No FreeRTOS, no motor, no scale, no allocation: the PC's simulation loads it as a library. A throw:
    charge_pid_start(), then every millisecond charge_pid_count() with the fine tube's turns since the start (the
    unit's own step count) and charge_pid_tick(); each reading with charge_pid_reading(). After each call the caller
    sets the tubes' speeds from coarse_rps (negative: the back-off) and fine_rps, and stops when done is true.
*/

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The fine tube's turns in 10 ms steps: 1.28 s, past the longest lag_s the console takes
#define CHARGE_PID_WINDOW_STEPS 128
#define CHARGE_PID_WINDOW_STEP_S 0.01f

// What the profile and the charge settings give, as today's loop reads them, the speeds within the motors' limits
typedef struct {
    float coarse_kp;
    float coarse_ki;
    float coarse_kd;
    float coarse_min_rps;
    float coarse_max_rps;
    float fine_kp;
    float fine_ki;
    float fine_kd;
    float fine_min_rps;
    float fine_max_rps;
    float coarse_stop_threshold_gr;
    float backoff_turns;            // 0: no back-off
    float backoff_rps;
    float digit_gr;                 // the scale's
    float interval_s;
    float band_gr;                  // the mode's
} charge_pid_settings_t;

// The frame's numbers: the scale's make (lag_s, rest_s) and the learning's. Live, never stored (`cpid`)
typedef struct {
    float lag_s;                    // the fine tube's powder shows this long after its turn (the A&D: 0.9 s)
    float rest_s;                   // the least wait from a stop to its verdict (the glide's on the A&D: 2.5 s)
    float gr_turn_prior;            // the figure a fresh powder starts from: high, so it starts short, never over
    float learn_share;              // the newest throw weighs at least this in the learnt figure
    float coarse_quiet_s;           // the fine tube's stop waits this long after the coarse tube's
    float throw_most_s;
    float over_most_gr;
    float digit_gr;                 // the scale's digit (the A&D: 0.02 gr); no driver reports it yet
    // The fine tube's least speed from the learnt figure (the second experiment, 2026-11-22 night): the speed at
    // which the powder in the air is this share of the band, never above the profile's least speed, never below
    // floor_least_rps. 0: the profile's, as today's loop
    float floor_band_share;
    float floor_least_rps;
    // The coarse tube's powder in the forecast too (the third experiment, the owner's option 1, 2026-11-22 night): its
    // forward turns over the last coarse_lag_s at coarse_gr_turn, the calibration's. 0: left out, and the fine tube's
    // stop pauses for coarse_quiet_s instead, as before
    float coarse_gr_turn;
    float coarse_lag_s;
    // The glide's margin for that powder, whose turn-to-turn spread is the calibration's: both tubes act as if this
    // share more of it were coming (the glide's 3 spreads x the calibration's spread a turn). 0: none
    float coarse_margin_share;
    uint8_t most_tops;
} charge_pid_constants_t;

// What the throws taught: kept from charge to charge while the unit runs, not stored
typedef struct {
    float gr_turn;                  // the fine tube's grains a turn, as its window sees them; 0: not yet set
    uint16_t learnt;                // throws taken in
} charge_pid_learning_t;

typedef enum {
    CHARGE_PID_PHASE_STREAM = 0,    // the PID loops pour
    CHARGE_PID_PHASE_REST,          // both stopped: waiting for one settled reading
} charge_pid_phase_t;

typedef struct {
    charge_pid_settings_t s;
    charge_pid_constants_t k;
    charge_pid_learning_t *learning;

    // Outputs
    float coarse_rps;
    float fine_rps;
    bool done;
    uint8_t result;                 // charge_control_result_t
    uint8_t phase;                  // charge_pid_phase_t
    float rest_weight;
    bool have_first_error;
    float first_error_gr;
    uint8_t tops;
    float unseen_gr;                // the forecast's powder in the air, at the last reading, the coarse margin with it

    // The throw
    uint32_t start_ms;
    float target_gr;
    float coarse_target_gr;
    float aim_gr;
    float stop_room_gr;
    float still_s;

    // The fine tube's window
    float window[CHARGE_PID_WINDOW_STEPS];
    uint16_t window_head;           // where the next step goes
    uint16_t window_steps;          // steps the window holds
    float window_at_s;              // the current step's start
    float window_base_turns;        // the turns counted at it
    float fine_turns;
    float coarse_window[CHARGE_PID_WINDOW_STEPS];   // the coarse tube's forward turns, the same steps
    float coarse_window_base;       // its forward turns at the current step's start
    float coarse_forward;           // its forward turns since the start: a back-off pours nothing
    float coarse_counted;           // the coarse turns last counted, either way
    bool coarse_forwards;           // the coarse tube was last asked forwards

    // Today's loop
    float coarse_integral;
    float fine_integral;
    float coarse_last_error;
    float fine_last_error;
    float last_sample_s;
    bool first_sample;
    bool coarse_moves;
    float backoff_until_s;          // < 0: none
    float coarse_off_s;             // < 0: not yet

    // The readings
    float reading;
    bool have_highest;
    float highest;
    float lowest;
    float last_rise_s;
    float stop_s;
    bool have_stop;
    float stop_reading;
    float stop_window_turns;
    float stop_coarse_unseen;       // the coarse tube's powder in the air then, by the forecast, no margin
} charge_pid_t;

void charge_pid_default_constants(charge_pid_constants_t *k);
void charge_pid_start(charge_pid_t *c, const charge_pid_settings_t *s, const charge_pid_constants_t *k,
                      charge_pid_learning_t *learning, float target_gr, uint32_t now_ms);
// Each tube's turns since the start, as the unit counts its steps (the coarse tube's back-off counts too)
void charge_pid_count(charge_pid_t *c, float coarse_turns, float fine_turns);
void charge_pid_tick(charge_pid_t *c, uint32_t now_ms);
void charge_pid_reading(charge_pid_t *c, uint32_t now_ms, float weight);
// The fine tube's turns over the last lag_s
float charge_pid_window_turns(const charge_pid_t *c);
// The fine tube's least speed as the next pass of the loop takes it
float charge_pid_least_rps(const charge_pid_t *c);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_PID_LEARNT_H_
