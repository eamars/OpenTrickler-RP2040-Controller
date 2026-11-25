#ifndef CHARGE_CONTROLLER_H_
#define CHARGE_CONTROLLER_H_
/*
 * ADR-009 step 2: the charge controller, as tools/charge_sim/step2.py runs it on the twin (gate 1), in C with nothing
 * of the RTOS, the motors or the scale in it. The charge mode drives it on the unit (gate 3); tests/step2_sim.py runs
 * the same C on the simulation beside the Python, call for call (gate 2).
 *
 * Two channels, each a tube:
 * - the coarse is today's: its PID on the reading's error to the target less its threshold, stopped for good once the
 *   reading reaches it, then its back-off;
 * - the fine one's PID works on the forecast's error, within its envelope, and it stops the moment the forecast
 *   reaches the target (each millisecond, not only on a reading). No top-up.
 * The forecast is the reading plus the fine tube's turns turned out and not yet shown (10 ms parcels of its speed at
 * the motor's ramp, each due after step 1's lag at its flow) times its weight a turn: the median of the last throws'
 * own, carried in RAM from throw to throw; the first throw after a start finds its own once a few tenths of a turn are
 * shown, and before that takes the reading alone.
 *
 * Times are given in milliseconds from any start, as doubles: the controller keeps the simulation's own arithmetic.
 */
#include <stdbool.h>
#include <stdint.h>

#define CHARGE_LAG_POINTS 5             // as the scale's settings keep step 1's lag (SCALE_LAG_POINTS)
#define CHARGE_CARRY_THROWS 5           // the throws whose own weights a turn are carried (their median)
#define CHARGE_PARCELS_MAX 2048         // the fine tube's 10 ms parcels kept from the coarse tube's quiet on, for the
                                        // throw's own weight a turn: 20 s; past it the own is held as last found
#define CHARGE_ROWS_MAX 512             // the readings kept from the quiet on, for the same: 25 s of the A&D's 20 a s
#define CHARGE_AIR_MAX 256              // parcels turned out and not yet shown: the slowest lag's, 1.2 s, and more
#define CHARGE_EDGE_MAX 256             // parcels turned out within the lags before the quiet: their rest depends on
                                        // the weight a turn

// A channel: its PID's gains and the speeds it is held between, rev/s
typedef struct {
    float kp;
    float ki;
    float kd;
    float least_rps;
    float most_rps;
} charge_channel_t;

// What a throw is given besides its target: the profile, the charge settings and step 1's records
typedef struct {
    charge_channel_t coarse;
    charge_channel_t fine;              // its least: step 1's, the fine tube's least speed for this powder
    float coarse_threshold;             // the coarse tube stops for good at the target less this
    bool backoff_enable;                // the coarse tube turned back once stopped, as today
    float backoff_turns;
    float backoff_rps;                  // 0: at the coarse tube's least
    float settle_s;                     // step 1's: the scale's settle time
    int lag_count;                      // step 1's lag by flow, at least one point, by rising flow
    float lag_flow_per_s[CHARGE_LAG_POINTS];
    float lag_s[CHARGE_LAG_POINTS];
    float acceleration_rps2;            // the motors' ramp
    float fine_weight_per_turn;         // step 1's, the powder's calibration: with nothing carried the forecast
                                        // starts from it; 0, none (ADR-009, 2026-11-26: without it a fast profile
                                        // never teaches one)
} charge_config_t;

// A parcel of the fine tube's turns: when it began, from the throw's start, and how long it lasted
typedef struct {
    float turns;
    uint32_t at_ms : 24;
    uint32_t ms : 8;
} charge_parcel_t;

typedef struct {
    float t_s;                          // from the throw's start
    float weight;
} charge_row_t;

typedef struct {
    double due_s;                       // when it shows on the scale, from the throw's start
    float turns;
} charge_air_t;

/*
 * A throw's room: the fine tube's parcels and the readings it keeps, some 28 KB. The charge borrows it while it runs
 * and gives it back after (charge_controller_start); the controller itself stays small and is kept for good
 */
typedef struct {
    charge_air_t air[CHARGE_AIR_MAX];               // a ring, in the order turned out
    charge_parcel_t parcels[CHARGE_PARCELS_MAX];    // from parcels[parcel_first], parcel_count of them
    charge_row_t rows[CHARGE_ROWS_MAX];
    double shown_at_row[CHARGE_ROWS_MAX];           // the fit's room: turns first shown at each reading
    charge_parcel_t edge[CHARGE_EDGE_MAX];          // the rest's edge (below)
} charge_room_t;

/*
 * The controller: its config, set before each throw (the tuner changes the fine kp between throws); what it gives
 * back (each tube's speed asked, rev/s, negative to turn back; done once it has stopped); the weights a turn it
 * carries from throw to throw; and the throw's own state, its room lent for the throw
 */
typedef struct {
    charge_config_t config;
    double coarse_rps;
    double fine_rps;
    bool done;

    double carry[CHARGE_CARRY_THROWS];  // the last throws' own weights a turn, oldest first
    int carry_count;

    double start_ms;
    double target;
    double coarse_target;
    double coarse_integral;
    double fine_integral;
    double coarse_last_error;
    double fine_last_error;             // of the forecast; none before the first reading
    bool fine_last_known;
    double last_sample_ms;
    bool first_sample;
    bool coarse_moves;
    bool backoff_on;
    double backoff_until_ms;
    double fine_speed;                  // the fine motor's speed at its ramp
    double parcel;                      // the parcel being turned out, its turns
    double parcel_start_s;
    double last_tick_s;
    double reading;
    bool quiet_known;                   // the coarse tube's powder quiet from quiet_at_s on
    double quiet_at_s;
    bool k_known;                       // the weight a turn the forecast takes now
    double k;
    bool own_known;                     // the throw's own weight a turn
    double own;
    bool held;                          // the fine PID's speed held at a limit: its integral held
    bool fit_cut;                       // parcels or readings past their room: the throw's own held as last found

    charge_room_t *room;                // lent for the throw
    double unseen;                      // the air's turns
    int air_first;
    int air_count;

    int parcel_first;
    int parcel_count;
    int parcels_most;                   // the most kept at once in this throw

    int row_count;

    // The throw's rest, from the first reading after the quiet: the fine turns not yet shown then are the parcels due
    // after it, those turned out since its quickest lag before it for certain, those of the edge by their own lag
    bool rest_known;
    double rest_quiet_s;
    double rest_at_quiet;
    double rest_certain;
    bool rest_cut;                      // the edge past its room: no carry renewed
    int edge_count;
} charge_controller_t;

#ifdef __cplusplus
extern "C" {
#endif

// A controller with nothing carried: as after a start of the unit
void charge_controller_init(charge_controller_t *controller);

// A throw begins: its target, at now_ms, in the room lent for it (kept until the throw is at rest). False when its
// config cannot be used (no lag table) or no room is lent
bool charge_controller_start(charge_controller_t *controller, charge_room_t *room, double target, double now_ms);

// Each millisecond: the fine tube's turns counted, the air's due parcels shown, the stop the moment the forecast
// reaches the target
void charge_controller_tick(charge_controller_t *controller, double now_ms);

// Each reading of the scale, after the millisecond's tick: both tubes' speeds
void charge_controller_reading(charge_controller_t *controller, double now_ms, double weight);

// A pause's end (ADR-013: both motors were stopped for want of readings): the fine tube counted from standing, the
// parcel being turned out closed where the pause began, the D term begun again as on a first reading
void charge_controller_resume(charge_controller_t *controller, double now_ms);

// How long after the stop the verdict is read: the slowest lag and the settle time
double charge_controller_verdict_after_s(const charge_controller_t *controller);

// The throw at rest, its verdict's reading: its own weight a turn carried to the next throws, when the fine tube
// turned enough alone after the coarse tube's quiet
void charge_controller_at_rest(charge_controller_t *controller, double reading);

// The weight a turn carried, the median of the last throws' own; with none, step 1's where it is kept; false when
// neither is
bool charge_controller_carried(const charge_controller_t *controller, double *k);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_CONTROLLER_H_
