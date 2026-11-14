#ifndef POWDER_ANALYSIS_CONFIG_H_
#define POWDER_ANALYSIS_CONFIG_H_

// All tunable values of the powder analysis (automatic PID tuning of a powder profile) in one place.
// Units: weights in grain (gr), times in seconds (s), motor speeds in revolutions per second (rps).
//
// See docs/powder_analysis_concept.md for the background of every value.


// ---------------------------------------------------------------------------------------------
// Powder description entered by the user. Shape and grain size select the start values below.
// ---------------------------------------------------------------------------------------------
typedef enum {
    POWDER_SHAPE_BALL = 0,
    POWDER_SHAPE_STICK = 1,
    POWDER_SHAPE_FLAKE = 2,
    POWDER_SHAPE_DISC = 3,
    POWDER_SHAPE_COUNT,
} powder_shape_t;

typedef enum {
    GRAIN_SIZE_FINE = 0,
    GRAIN_SIZE_MEDIUM = 1,
    GRAIN_SIZE_COARSE = 2,
    GRAIN_SIZE_COUNT,
} grain_size_t;

// Names for display and web UI, indexed by the enums above
static const char * const powder_shape_names[POWDER_SHAPE_COUNT] = {"Ball", "Stick", "Flake", "Disc"};
static const char * const grain_size_names[GRAIN_SIZE_COUNT] = {"Fine", "Medium", "Coarse"};


// ---------------------------------------------------------------------------------------------
// Start value matrix
//
// MEDIUM column: based on two measurement series with Vihtavuori N135 (stick / medium):
// coarse Kp 0.06-0.0675, fine Kp 3.3 verified, fine Kd 8-9.2.
// COARSE column: based on one series with Vihtavuori N560 (stick / coarse): coarse Kp 0.12,
// fine Kp 3.6 found, fine Kd 3.5-4.0.
// In both columns the other shapes are scaled from the stick values by the same factor as the
// original estimates.
// FINE column: based on one clean series with Vihtavuori N320 (stick / fine): coarse Kp 0.1125,
// best fine Kp without overthrow 4.8.
// Refine these values from the CSV protocols of real analysis runs.
//
// For reference, the built-in stick powder profiles (AR2208, AR2209, 8208XBR, Benchmark2) use
// coarse Kp 0.025-0.06 / Kd 0.3 and fine Kp 0.8-2.0 / Kd 10-15. Vihtavuori N140 needed coarse Kp 0.8.
// ---------------------------------------------------------------------------------------------

// Coarse trickler Kp start value [shape][grain size]
static const float powder_analysis_coarse_kp_start[POWDER_SHAPE_COUNT][GRAIN_SIZE_COUNT] = {
    //  Fine    Medium  Coarse
    {   0.08f,  0.04f,  0.075f },   // Ball
    {   0.10f,  0.06f,  0.12f  },   // Stick
    {   0.16f,  0.10f,  0.18f  },   // Flake
    {   0.16f,  0.10f,  0.18f  },   // Disc
};

// Fine trickler Kp start value [shape][grain size]
static const float powder_analysis_fine_kp_start[POWDER_SHAPE_COUNT][GRAIN_SIZE_COUNT] = {
    //  Fine    Medium  Coarse
    {   3.0f,   2.4f,   2.5f   },   // Ball
    {   4.0f,   3.0f,   3.0f   },   // Stick
    {   5.0f,   3.6f,   4.0f   },   // Flake
    {   5.0f,   3.6f,   4.0f   },   // Disc
};

// Fine trickler Kd [shape][grain size]: the final value, not searched. The D term works on the
// error change per millisecond and the scale changes its reading only every other sample, so it is
// tiny (< 1 % of the fine speed at Kd 4, 2-4 % at Kd 16) and noisy; measured, see concept 4.8.
static const float powder_analysis_fine_kd_start[POWDER_SHAPE_COUNT][GRAIN_SIZE_COUNT] = {
    //  Fine    Medium  Coarse
    {   4.0f,   8.0f,   4.0f   },   // Ball
    {   4.0f,   8.0f,   4.0f   },   // Stick
    {   4.0f,   8.0f,   4.0f   },   // Flake
    {   4.0f,   8.0f,   4.0f   },   // Disc
};

// Coarse Kd start value, same for all shapes and grain sizes
#define POWDER_ANALYSIS_COARSE_KD_START         0.1f


// ---------------------------------------------------------------------------------------------
// Flow speeds (not searched, chosen to avoid motor stalls). The fine minimum speed is a start
// value: the verification lowers it when throws keep overthrowing, see the correction below.
// ---------------------------------------------------------------------------------------------
#define POWDER_ANALYSIS_COARSE_MIN_FLOW_SPEED_RPS   0.1f
#define POWDER_ANALYSIS_COARSE_MAX_FLOW_SPEED_RPS   5.0f
#define POWDER_ANALYSIS_FINE_MIN_FLOW_SPEED_RPS     0.06f
#define POWDER_ANALYSIS_FINE_MAX_FLOW_SPEED_RPS     5.0f

// Ki is always 0 (as recommended by the tuning guide)
#define POWDER_ANALYSIS_KI                          0.0f


// ---------------------------------------------------------------------------------------------
// Flooding (filling the trickler tubes before the first throw), both motors run at fixed speed
// ---------------------------------------------------------------------------------------------
#define POWDER_ANALYSIS_FLOOD_COARSE_SPEED_RPS      1.0f
#define POWDER_ANALYSIS_FLOOD_FINE_SPEED_RPS        2.0f


// ---------------------------------------------------------------------------------------------
// Acceptance criteria and time goals
// ---------------------------------------------------------------------------------------------
// The coarse phase must be finished within this time
#define POWDER_ANALYSIS_COARSE_TIME_GOAL_S          10.0f

// The coarse Kp search leaves room for the brake (coarse Kd): its throws must stay within
// time goal / this factor (10 s / 1.3 = 7.7 s). Estimate, tune from the CSV protocols.
#define POWDER_ANALYSIS_COARSE_BRAKING_FACTOR       1.3f

// Coarse Kd probing (concept 4.6): start value doubled at most this often, 0.1 -> ... -> 3.2
#define POWDER_ANALYSIS_COARSE_KD_MAX_DOUBLINGS     5

// Stop doubling early when the coarse time changed less than this fraction (0.10 = 10 %)
// compared with the previous Kd: more Kd will not change it either
#define POWDER_ANALYSIS_COARSE_KD_MIN_TIME_CHANGE   0.10f

// Coarse times this close to the fastest one count as equally fast; of those the smallest Kd wins
#define POWDER_ANALYSIS_COARSE_KD_TIME_TIE_S        0.2f

// The Kd phase must end with a throw within the coarse time goal. If not even the start value
// managed that, the throw is repeated up to this many times (coarse times scatter a lot, one
// slow throw is most likely bad luck) before the brake is given up.
#define POWDER_ANALYSIS_COARSE_KD_MAX_RETRIES       2

// Control check only (does not drive the search): the throw with the chosen coarse Kd should stop
// at most this much above the coarse stop point (stop point = target - coarse_stop_threshold from
// the charge mode configuration). If not, the result is flagged.
#define POWDER_ANALYSIS_COARSE_MAX_OVERSHOOT_GR     1.0f

// A throw counts as a hit only when the whole throw (coarse + fine) is finished within this time
#define POWDER_ANALYSIS_TOTAL_TIME_GOAL_S           15.0f

// The fine Kp search looks for the smallest Kp whose full throw still finishes within this time:
// the slower the fine trickler runs at the stop, the less powder falls after it (N135 series 5:
// Kp 6.0 -> +0.156 gr, 3.0 -> 0..+0.028 gr). The 2 s margin to the total time goal absorbs the
// scatter of the coarse time (4.4-8.4 s in that series).
#define POWDER_ANALYSIS_FINE_TARGET_TIME_S          13.0f

// Safety timeout per throw. A throw taking longer is aborted by the caller and reported as timed out
// (typically caused by a Kp that is far too small).
#define POWDER_ANALYSIS_TRIAL_TIMEOUT_S             30.0f

// After a throw the weight is measured once the scale reading has settled (same criteria as charge
// mode). If it does not settle within this time the current average is used anyway.
#define POWDER_ANALYSIS_STABLE_WEIGHT_TIMEOUT_S     10.0f


// ---------------------------------------------------------------------------------------------
// Search limits
// ---------------------------------------------------------------------------------------------
// Bisection stops when the interval between "not met" and "met" is smaller than this fraction
// of the "met" value ...
#define POWDER_ANALYSIS_BISECTION_MIN_INTERVAL      0.15f

// ... or after this many bisection steps
#define POWDER_ANALYSIS_MAX_BISECTION_STEPS         3

// A search halves its value while it is still on the "wrong" side of the boundary (e.g. the start
// value already overthrows). This bounds the halving so the search cannot run forever
// (5 steps = start value / 32).
#define POWDER_ANALYSIS_MAX_HALVING_STEPS           5

// Same safety net for doubling (6 steps = start value * 64): a parameter that has no effect on the
// throw cannot double forever. The search then ends with its best value.
#define POWDER_ANALYSIS_MAX_DOUBLING_STEPS          6

// Verification: a round is this many throws with fixed gains ...
#define POWDER_ANALYSIS_VERIFY_ROUND_THROWS         10

// ... and counts as verified with this many hits: one clump after the stop must not fail a good
// profile, and five throws missed rare large falls (concept 4.9)
#define POWDER_ANALYSIS_VERIFY_HITS_FOR_VERIFIED    9

// A round is given up as soon as it has this many overthrows
#define POWDER_ANALYSIS_VERIFY_MAX_OVERTHROWS       2

// Correction after a given up round (concept 4.9). What falls after the stop depends on the speed
// at the stop = max(fine Kp * fine_stop_threshold, fine minimum flow speed); the correction lowers
// whichever of the two decides it: fine Kp by this factor (-10 %) ...
#define POWDER_ANALYSIS_FINE_KP_CORRECTION_FACTOR           0.9f

// ... or the fine minimum flow speed by this factor (-25 %, e.g. 0.06 -> 0.045 -> 0.034 -> 0.025 rps)
#define POWDER_ANALYSIS_FINE_MIN_SPEED_CORRECTION_FACTOR    0.75f

// At most this many corrections in total. Then the best round so far is the result.
#define POWDER_ANALYSIS_MAX_FINE_CORRECTIONS                3

// Upper limit of throws for the whole analysis. Also the size of the trial protocol kept in RAM.
#define POWDER_ANALYSIS_MAX_TRIALS                  60

// Plausibility limit for a target weight coming from the web (gr)
#define POWDER_ANALYSIS_MAX_TARGET_WEIGHT_GR        300.0f

// Resolution of the scale reading in gr: an A&D FX with 0.001 g shows steps of about 0.0154 gr,
// which arrive as 0.02 gr steps. Adjust for other scales. Used only for the fine stop threshold
// recommendation (hint, the charge mode setting is never changed), see compute_fall_statistics().
#define POWDER_ANALYSIS_SCALE_RESOLUTION_GR         0.02f

// Per-sample recording of every throw (analysis and test throws) for the web page: about 30 s at
// 10 Hz. 8 floats per sample, so 300 samples take about 9.6 KB of RAM. Longer throws are cut off.
#define POWDER_ANALYSIS_TRACE_MAX_SAMPLES           300


#endif  // POWDER_ANALYSIS_CONFIG_H_
