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
// A tube's powder on its way, in parcels: 2.56 s at 10 ms a parcel. 128 was too few: with BM2's record a creep's
// powder took longer than 1.28 s to show, and merging the overflow stopped glides up to 0.5 s early (the PC's check,
// ADR-009, gate 2). A scale later still has its parcels merged into the last, shown no sooner than either: the
// forecast stays whole and the throw stops earlier, never later. The charge takes the controller from the heap
#define CHARGE_CONTROL_PARCELS_MAX 256

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
    CHARGE_CONTROL_RESULT_CUP,          // a reading below the base: the cup lifted (ADR-009, gate F.1)
    CHARGE_CONTROL_RESULT_NO_POWDER,    // the powder forecast did not show: an empty hopper, a jammed tube
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
    // The fine tube's grains a turn learnt over a session (ADR-009, 2026-11-22): the calibration's figure counts
    // for this many throws against the throws' own (0: none learnt), and the figure never moves further than
    // learn_fine_most of it
    charge_control_real_t learn_fine_prior;
    charge_control_real_t learn_fine_most;
    // 1: the forecast counts a fine turn made before the stretch at the stream's figure, one after it at the
    // finish's, each learnt; 0: one figure for every turn (a 5 gr throw poured 0.83 gr a fine turn, its finish 0.68)
    charge_control_real_t learn_fine_split;
    charge_control_real_t still_least_s;            // at rest: no new high for this long, or two reading intervals if longer
    uint8_t most_moves;             // tries a throw is given after its first rest
    // Limits of the firmware's own, not the research's: a throw that passes one ends with CHARGE_CONTROL_RESULT_LIMIT
    charge_control_real_t throw_most_s;
    charge_control_real_t fine_most_times;          // C19: of the fine turns the target needs at its figure
    charge_control_real_t zero_margin_gr;           // the user's zero mean margin (A9): the scale's noise is it and
                                                    // two digits (the cup test, no powder)
    charge_control_real_t over_tolerances;          // a reading this many tolerances over the target stops everything
    // The speed law (ADR-009, the owner's option 2, 2026-11-22 night; an experiment): 0 the glide's; 1 the PID law,
    // today's PID loop on the frame's forecast with the gains in charge_control_t's pid, everything else the frame's
    uint8_t speed_law;
    // The PID law's guard against gains bad to begin with (1, the default): each tube never faster than the frame's
    // top (coarse_top_rps, fine_top_rps: the fastest speeds whose lags the calibration knows) nor than the frame can
    // stop it from (the fine tube: its floor plus the room above the stop over its grains a turn and glide time; the
    // coarse tube: the glide's coarse law), and the I term counting only while its tube is not held at a limit
    uint8_t pid_envelope;
} charge_control_constants_t;

// The PID law's gains: the powder profile's, the speeds within the motors' limits. The fine tube's least speed is the
// frame's floor, not the profile's
typedef struct {
    charge_control_real_t coarse_kp;
    charge_control_real_t coarse_ki;
    charge_control_real_t coarse_kd;
    charge_control_real_t coarse_min_rps;
    charge_control_real_t coarse_max_rps;
    charge_control_real_t fine_kp;
    charge_control_real_t fine_ki;
    charge_control_real_t fine_kd;
    charge_control_real_t fine_max_rps;
} charge_control_pid_t;

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
    // The fine tube's grains a turn learnt from throw to throw: the calibration's, and the throws' sum and count;
    // the figure in use is cal.fine_gr_turn
    charge_control_real_t fine_gr_measured;
    charge_control_real_t fine_learn_sum;
    uint16_t fine_learn_n;
    charge_control_real_t fine_gr_stream;           // the split's stream figure; its finish's is cal.fine_gr_turn
    charge_control_real_t stream_learn_sum;
    uint16_t stream_learn_n;

    // Out, read after every call
    charge_control_real_t coarse_rps;
    charge_control_real_t fine_rps;
    charge_control_real_t glide_rps[2];             // coarse, fine: the glide's stream law's asked speeds, in shadow
                                                    // under the PID law (read by charge_control_glide_rps)
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
    charge_control_real_t moved_turns;              // the small moves' turns asked for: C19 adds them to its bound
    // No powder: the reading at the last rise counted, the forecast powder due since, and when the first of it was
    // due (below zero: none); and the longest lag the calibration measured
    charge_control_real_t powder_mark;
    charge_control_real_t due_since_mark_gr;
    charge_control_real_t first_due_s;
    charge_control_real_t longest_lag_s;
    bool have_first_error;
    charge_control_real_t first_error_gr;
    // The fine tube's stretch, its learning's: from its slowing, the coarse tube quiet if it ran, to the first rest
    bool coarse_ran;
    charge_control_real_t coarse_off_s;             // below zero until the coarse tube stops at its margin
    bool window_open;
    charge_control_real_t window_reading;
    charge_control_real_t window_shown_turns;       // the fine tube's turns whose powder had shown at its start
    charge_control_real_t window_turned;            // the fine tube's turns at its start, and its powder unseen then
    charge_control_real_t window_unseen_gr;
    // The PID law (k.speed_law 1): its gains, set by the caller after init, and its I and D terms, taken on each
    // reading as today's loop takes them (the integral a sum of errors, the derivative a change per millisecond)
    charge_control_pid_t pid;
    charge_control_real_t pid_integral[2];
    charge_control_real_t pid_last_error[2];
    charge_control_real_t pid_term[2];
    charge_control_real_t pid_last_s;
    bool pid_first;
    bool pid_held[2];               // its last speed was held at a limit, the envelope's among them
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

/*
    What the glide's stream law asks of a tube now (0 coarse, 1 fine), for a throw's recording: while the stream pours,
    the glide's speed whichever law pours (under the glide, the speed asked); at a rest or a move, the speed asked, the
    same under both laws
*/
charge_control_real_t charge_control_glide_rps(const charge_control_t *c, int tube);

// A result in a word, for the console, the event log and a recording: good, over, short, limit; none when not done
const char *charge_control_result_string(charge_control_result_t result);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_CONTROL_H_
