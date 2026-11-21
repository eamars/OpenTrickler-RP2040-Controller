#ifndef CHARGE_CONTROL_H_
#define CHARGE_CONTROL_H_

/*
    The staged charge controller (ADR-009): the research's frame with its two finishes, ported from
    tools/charge_sim/staged.py. No FreeRTOS, no motor, no scale, no allocation: the firmware and the PC's simulation
    run the same code (the simulation loads it as a library).

    The weight acted on is a forecast: the last reading plus the powder each tube has turned out that the reading
    does not show yet, counted in turns, each small parcel with the lag of the flow it was turned out at. Each tube
    turns at a speed in proportion to the forecast room above its own margin, between a floor and a top, and stops
    for good when the room is down to its margin. The verdict is on a reading at rest; a throw that rests short is
    topped up by small moves of the fine tube.

    Every number a throw uses is the calibration's (measured on the unit, then set by charge_control_set_rules) or
    one of the frame's constants (charge_control_constants_t), which the bench tunes.

    A throw: charge_control_start(), then every millisecond charge_control_count() with each tube's turns since the
    start (the unit's own step count) and charge_control_tick(); each reading with charge_control_reading(). After
    each call the caller sets the tubes' speeds from coarse_rps and fine_rps, makes the small move asked for in
    move_turns (then charge_control_moved()), and stops when done is true.
*/

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    The controller's numbers are float on the unit. The PC's check also builds it with CHARGE_CONTROL_DOUBLE, where
    they are double as its Python reference's are, so a difference between the two is the port's and not float's.
*/
#ifdef CHARGE_CONTROL_DOUBLE
typedef double charge_control_real_t;
#else
typedef float charge_control_real_t;
#endif

#define CHARGE_CONTROL_COARSE_STOPS_MAX 4
#define CHARGE_CONTROL_FINE_STOPS_MAX 8
#define CHARGE_CONTROL_MOVES 3
#define CHARGE_CONTROL_PARCELS_MAX 256         // a tube's powder on its way, in parcels: 2.56 s at 10 ms a parcel

typedef enum {
    CHARGE_CONTROL_FINISH_GLIDE = 0,    // proposal 2: the fine tube creeps and stops a digit short (the default)
    CHARGE_CONTROL_FINISH_JOG = 1,      // proposal 1: the fine tube stops from a dribble, short, and moves add the rest
} charge_control_finish_t;

typedef enum {
    CHARGE_CONTROL_PHASE_FEED = 0,      // the tubes pour
    CHARGE_CONTROL_PHASE_REST,          // both stopped: waiting for the reading to come to rest
    CHARGE_CONTROL_PHASE_MOVE,          // a small move of the fine tube
} charge_control_phase_t;

typedef enum {
    CHARGE_CONTROL_RESULT_NONE = 0,     // not done
    CHARGE_CONTROL_RESULT_GOOD,         // at rest within the band
    CHARGE_CONTROL_RESULT_OVER,         // at rest above the band
    CHARGE_CONTROL_RESULT_SHORT,        // at rest below the band with no tries left
    CHARGE_CONTROL_RESULT_LIMIT,        // ended by a limit: too long, too many fine turns, far over
} charge_control_result_t;

// A tube's stops from one speed: what still arrived after them (mean, spread) and when the reading was still
typedef struct {
    charge_control_real_t speed_rps;
    charge_control_real_t unseen_gr;
    charge_control_real_t spread_gr;
    charge_control_real_t quiet_s;
    charge_control_real_t lag_s;            // its powder's time from turned out to shown; below zero when not measured
} charge_control_stop_t;

// Small moves of one size: the turn, what they released (mean, nine in ten at most) and the wait to see it
typedef struct {
    charge_control_real_t turns;
    charge_control_real_t mean_gr;
    charge_control_real_t most_gr;
    charge_control_real_t wait_s;
} charge_control_move_t;

// What the calibration measured, and the numbers charge_control_set_rules makes from it
typedef struct {
    charge_control_real_t digit_gr;
    charge_control_real_t interval_s;
    charge_control_real_t coarse_gr_turn;
    charge_control_real_t fine_gr_turn;
    uint8_t coarse_stop_count;
    uint8_t fine_stop_count;
    uint8_t move_count;
    uint8_t finish;                 // charge_control_finish_t
    charge_control_stop_t coarse_stops[CHARGE_CONTROL_COARSE_STOPS_MAX];   // by speed, slowest first
    charge_control_stop_t fine_stops[CHARGE_CONTROL_FINE_STOPS_MAX];
    charge_control_move_t moves[CHARGE_CONTROL_MOVES];                     // smallest first
    charge_control_real_t watch_s;                  // how long this scale's stops and moves had to be watched
    charge_control_real_t coarse_share;             // how far the coarse tube's powder a turn varied from run to run, as a share
    // By rule, from the above and the band
    charge_control_real_t band_gr;
    charge_control_real_t floor_rps;                // the fine tube's slowest: the creep, or the dribble
    charge_control_real_t stop_room_gr;             // the fine tube stops when the forecast room is down to this
    charge_control_real_t aim_gr;                   // where the first reading at rest is meant to lie, against the target
    charge_control_real_t glide_room_gr;            // the fine tube is at its floor from this room down
    charge_control_real_t glide_s;
    charge_control_real_t coarse_room_gr;
    charge_control_real_t coarse_glide_s;
    charge_control_real_t rest_s;                   // the wait after the fine tube's stop before the reading is judged
} charge_control_calibration_t;

// The frame's constants: the research's values by default (charge_control_default_constants), tuned on the bench
typedef struct {
    charge_control_real_t move_rps;                 // a small move is made at this speed
    charge_control_real_t parcel_s;                 // the powder turned out is counted in parcels of this long
    charge_control_real_t fine_top_rps;
    charge_control_real_t coarse_top_rps;
    charge_control_real_t coarse_floor_rps;         // the coarse tube pours in lumps below it
    charge_control_real_t dribble_digits_s;         // the jog finish's dribble
    charge_control_real_t coarse_spreads;           // of the coarse tube's spread a turn, kept clear of its powder still unseen
    charge_control_real_t creep_band_share;         // the creep's powder still on its way, as a share of the band
    charge_control_real_t creep_most_s;             // the creep's stretch takes no longer than this on average
    charge_control_real_t learn_share;              // of a throw's first error at rest, into where the fine tube stops
    charge_control_real_t learn_most_digits;        // no throw teaches more than this
    charge_control_real_t learn_dead_digits;        // nor less
    charge_control_real_t still_least_s;            // at rest: no new high for this long, or two reading intervals if longer
    uint8_t most_moves;             // tries a throw is given after its first rest
    // Limits of the firmware's own, not the research's: a throw that passes one ends with CHARGE_CONTROL_RESULT_LIMIT
    charge_control_real_t throw_most_s;
    charge_control_real_t fine_most_turns;
    charge_control_real_t over_most_gr;             // a reading this far over the target stops everything
} charge_control_constants_t;

typedef struct {
    charge_control_real_t due_s;            // when it will show, since the start
    charge_control_real_t grains;
} charge_control_parcel_t;

// One controller. Its learned stop lasts from one throw to the next, as a loading session's does
typedef struct {
    charge_control_calibration_t cal;
    charge_control_constants_t k;
    bool rehearsal;                 // end at the first reading at rest: the calibration's own throws
    bool moves_by_caller;           // the caller makes each small move to its count, and says when (the unit);
                                    // else a move ends by the time its turns take at move_rps (the simulation)
    charge_control_real_t stop_room_gr;             // where the fine tube stops, as learned from throw to throw

    // Out, read after every call
    charge_control_real_t coarse_rps;
    charge_control_real_t fine_rps;
    charge_control_real_t move_turns;               // a small move asked for, in turns; below zero when none is
    bool done;
    uint8_t result;                 // charge_control_result_t
    uint8_t phase;                  // charge_control_phase_t
    charge_control_real_t rest_weight;              // the reading the verdict was taken on

    // The throw
    uint32_t start_ms;
    charge_control_real_t target_gr;
    bool coarse_on;
    bool counted;                   // the caller gives the unit's own count of the turns
    charge_control_real_t turned[2];                // coarse, fine: as the unit counts its steps
    charge_control_real_t speed[2];                 // the motors' own ramped speeds, or as the count shows them
    charge_control_real_t marked[2];                // each tube's turns at the last parcel
    charge_control_real_t marked_at_s;
    uint32_t marks;                 // parcels made: the next is due at (marks + 1) parcel_s, never a sum of floats
    charge_control_real_t last_tick_s;
    charge_control_real_t acceleration_rps2;        // the motors' ramp, for the controller's own count when none is given
    charge_control_parcel_t parcels[2][CHARGE_CONTROL_PARCELS_MAX];
    uint16_t parcel_count[2];
    charge_control_real_t unseen_gr;                // all that is on its way
    charge_control_real_t coarse_unseen_gr;         // the coarse tube's part of it
    charge_control_real_t reading;
    bool have_reading;
    charge_control_real_t highest;                  // powder only adds: the reading is at rest when it has made no new high
    charge_control_real_t lowest;
    charge_control_real_t last_rise_s;
    charge_control_real_t stopped_at_s;             // below zero when the tubes are not stopped
    charge_control_real_t wait_s;
    charge_control_real_t move_until_s;
    uint8_t moves;
    uint8_t streams;                // times a rest far short sent the throw back to the stream
    bool have_first_error;
    charge_control_real_t first_error_gr;
} charge_control_t;

void charge_control_default_constants(charge_control_constants_t *k);

// The controller's numbers from the measured ones, for one finish and band. Nothing here is fitted to a powder
void charge_control_set_rules(charge_control_calibration_t *cal, charge_control_finish_t finish, charge_control_real_t band_gr,
                              const charge_control_constants_t *k);

// The rehearsals' first errors at rest place the stop: the margins above it move with it
void charge_control_place_stop(charge_control_calibration_t *cal, const charge_control_real_t *first_errors, uint16_t count);

// A controller for a calibration with its rules set. acceleration_rps2 is used only when no count is given
void charge_control_init(charge_control_t *c, const charge_control_calibration_t *cal,
                         const charge_control_constants_t *k, charge_control_real_t acceleration_rps2, bool rehearsal);

void charge_control_start(charge_control_t *c, charge_control_real_t target_gr, uint32_t now_ms);
void charge_control_count(charge_control_t *c, uint32_t now_ms, charge_control_real_t coarse_turns, charge_control_real_t fine_turns);
void charge_control_tick(charge_control_t *c, uint32_t now_ms);
void charge_control_reading(charge_control_t *c, uint32_t now_ms, charge_control_real_t weight);
// The caller made the small move asked for itself, to its step count: the move is over
void charge_control_moved(charge_control_t *c, uint32_t now_ms);

// A result in a word, for the console, the event log and a recording: good, over, short, limit; none when not done
const char *charge_control_result_string(charge_control_result_t result);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_CONTROL_H_
