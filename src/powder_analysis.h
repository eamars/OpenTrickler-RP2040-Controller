#ifndef POWDER_ANALYSIS_H_
#define POWDER_ANALYSIS_H_

// Powder analysis: runs the automatic PID tuning (see powder_analysis_tuner.h) on the real
// hardware and drives the display screens D1..D16 of docs/powder_analysis_concept.md.
//
// Entered from the menu task like the other modes: powder_analysis_menu() takes over the display
// until the analysis is finished or aborted and returns the menu form to go back to.
//
// The web interface controls the same run in parallel through the REST handlers at the end of
// this file: a command does exactly what the corresponding display button does.
//
// Display buttons: screens with two choices show both as footer buttons, the rotary knob moves the
// selection and the encoder button activates it. On the other screens the footer only labels the
// keys: left = encoder button, right = reset button.

#include <stdint.h>
#include <stdbool.h>
#include "http_rest.h"
#include "profile.h"
#include "powder_analysis_config.h"
#include "powder_analysis_tuner.h"


#ifdef __cplusplus
extern "C" {
#endif


// States = screens of the concept (D1..D16)
typedef enum {
    POWDER_ANALYSIS_STATE_IDLE = 0,                 // module not running
    POWDER_ANALYSIS_STATE_SETUP_WEIGHT = 1,         // D1  target weight
    POWDER_ANALYSIS_STATE_SETUP_SHAPE = 2,          // D2  powder shape
    POWDER_ANALYSIS_STATE_SETUP_GRAIN = 3,          // D3  grain size
    POWDER_ANALYSIS_STATE_PREPARE = 4,              // D4  thresholds, put pan on scale
    POWDER_ANALYSIS_STATE_FLOODING = 5,             // D5  fill the trickler tubes
    POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL = 6,     // D6/D8 empty pan: wait until it is taken away
    POWDER_ANALYSIS_STATE_WAIT_PAN_RETURN = 7,      // D6/D8 empty pan: wait until it is back
    POWDER_ANALYSIS_STATE_WAIT_ZERO = 8,            // D6/D8 empty pan: wait for a stable zero
    POWDER_ANALYSIS_STATE_TRIAL = 9,                // D7  throw running
    POWDER_ANALYSIS_STATE_PAUSED = 10,              // D9
    POWDER_ANALYSIS_STATE_ABORT_CONFIRM = 11,       // D10
    POWDER_ANALYSIS_STATE_RESULT_COARSE = 12,       // D11
    POWDER_ANALYSIS_STATE_RESULT_FINE = 13,         // D12
    POWDER_ANALYSIS_STATE_RESULT_THRESHOLD = 14,    // D13 fine stop threshold recommendation, Save / Discard
    POWDER_ANALYSIS_STATE_SAVE_SLOT = 15,           // D14
    POWDER_ANALYSIS_STATE_SAVE_CONFIRM = 16,        // D15
    POWDER_ANALYSIS_STATE_ACTIVATE = 17,            // D16
    POWDER_ANALYSIS_STATE_EXIT = 18,                // leaving the module

    // Test throw (web only): a single full throw with given values, recorded per sample.
    // The pan handling before the throw reuses the WAIT_* states with status.test_mode set.
    POWDER_ANALYSIS_STATE_TEST_THROW = 19,
    POWDER_ANALYSIS_STATE_TEST_DONE = 20,
} powder_analysis_state_t;


// Commands from the web interface (and what the display buttons are translated to)
typedef enum {
    POWDER_ANALYSIS_CMD_NONE = 0,
    POWDER_ANALYSIS_CMD_START = 1,          // start with the setup values of the command, skips the setup screens
    POWDER_ANALYSIS_CMD_NEXT = 2,           // Next / Yes / select
    POWDER_ANALYSIS_CMD_BACK = 3,           // Back / No
    POWDER_ANALYSIS_CMD_PAUSE_RESUME = 4,   // flooding start/pause, pause/resume between throws
    POWDER_ANALYSIS_CMD_FLOOD_DONE = 5,     // tubes are full
    POWDER_ANALYSIS_CMD_ABORT = 6,          // asks "Abort analysis?"
    POWDER_ANALYSIS_CMD_ABORT_CONFIRM = 7,
    POWDER_ANALYSIS_CMD_ABORT_CANCEL = 8,
    POWDER_ANALYSIS_CMD_SAVE = 9,           // from the result screens: saves to profile_idx under profile_name right away
    POWDER_ANALYSIS_CMD_DISCARD = 10,
    POWDER_ANALYSIS_CMD_USE_NOW_YES = 11,
    POWDER_ANALYSIS_CMD_USE_NOW_NO = 12,
    POWDER_ANALYSIS_CMD_ZERO_SCALE = 13,    // only while paused, only if the scale driver supports it (zero_available)
    POWDER_ANALYSIS_CMD_TEST_THROW = 14,    // single throw with the values of the command, only when idle or after a test throw
} powder_analysis_command_type_t;

typedef struct {
    powder_analysis_command_type_t type;

    // For START
    float target_weight;
    powder_shape_t shape;
    grain_size_t grain_size;

    // For SAVE
    uint8_t profile_idx;
    char profile_name[PROFILE_NAME_MAX_LEN];

    // For TEST_THROW (target_weight as well)
    float coarse_kp;
    float coarse_kd;
    float fine_kp;
    float fine_kd;
    float fine_min_flow_speed_rps;
} powder_analysis_command_t;


// Everything the display and the web interface show
typedef struct {
    powder_analysis_state_t state;

    // Setup
    float target_weight;
    powder_shape_t shape;
    grain_size_t grain_size;

    // Progress
    powder_analysis_phase_t phase;
    powder_analysis_trial_params_t trial;   // values of the running throw, afterwards of the last one
    float current_weight;
    float elapsed_s;                        // of the running throw, afterwards the duration of the last one
    bool flooding_running;
    bool test_mode;                         // a test throw is being prepared / running / done, not an analysis
    bool zero_available;                    // the scale driver can zero the scale (Zero button while paused)
    bool has_last_trial;
    powder_analysis_trial_t last_trial;     // result of the last throw (D8)
    uint8_t verify_round;                   // running (or last) verification round, 0 before verification
    uint8_t verify_round_hits;              // hits in that round
    uint8_t fine_corrections;               // verification corrections so far (fine Kp or fine minimum speed)
    bool correction_just_applied;           // the last throw gave its round up and a correction was applied
    bool correction_lowered_kp;             // what that correction did: fine Kp -10 % (true) or min speed -25 % (false)
    char prompt[32];                        // what the user should do now

    // Result (valid once has_result is set)
    bool has_result;
    powder_analysis_result_t result;
    uint8_t profile_idx;                    // save target
    char profile_name[PROFILE_NAME_MAX_LEN];
} powder_analysis_status_t;


// Per-sample recording of the last throw (analysis or test throw), kept until the next throw starts
typedef struct {
    float t_s;                              // seconds since the throw started
    float weight;
    float fine_p;                           // P and D terms of the fine trickler in rps ...
    float fine_d;
    float fine_speed;                       // ... and the commanded speed after the min/max clamp
    float coarse_p;
    float coarse_d;
    float coarse_speed;                     // 0 once stopped, negative during the backoff
} powder_analysis_trace_sample_t;

typedef struct {
    uint16_t id;                            // incremented once a throw is complete incl. its settled weight (web page fetches then)
    uint16_t count;
    bool truncated;                         // the throw was longer than the buffer
    float final_weight;                     // settled weight after the throw, NaN while running or when aborted (id unchanged then)
    float target_weight;                    // target of that throw (the live status may already have another one)
    bool test_throw;                        // recorded during a test throw, otherwise an analysis throw of params.phase
    powder_analysis_trial_params_t params;  // the values the throw was made with
    powder_analysis_trace_sample_t samples[POWDER_ANALYSIS_TRACE_MAX_SAMPLES];
} powder_analysis_trace_t;


// Menu entry, returns the menu form id to go back to
uint8_t powder_analysis_menu(void);

// Web interface
bool powder_analysis_post_command(const powder_analysis_command_t *command);
void powder_analysis_get_status(powder_analysis_status_t *status);   // consistent copy, safe from any task
const char * powder_analysis_state_to_string(powder_analysis_state_t state);

// REST interface
bool http_rest_powder_analysis_state(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_powder_analysis_trials(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_powder_analysis_trace(struct fs_file *file, int num_params, char *params[], char *values[]);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // POWDER_ANALYSIS_H_
