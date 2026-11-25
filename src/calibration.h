#ifndef CALIBRATION_H_
#define CALIBRATION_H_
/*
 * ADR-009 step 1: the calibration as a job on the unit (gate 3). Its three parts, each run alone:
 * - the cup: put on the scale empty and zeroed, lifted for its weight, put back, filled by the person with the tubes'
 *   speeds until it holds as much as they want, its capacity kept, emptied;
 * - the scale: its settle time and its lag (calibration_method.c);
 * - a powder, the one the charge uses: each tube's least speed (calibration_method.c).
 * Each part begins with the empty cup on the zeroed scale, and a run that would pass the cup's capacity pauses it for
 * the cup to be emptied. A part's records are kept only when the person keeps them: the cup's into the charge
 * settings, the scale's into the scale settings, a powder's into its profile, each stored. A failure says what
 * failed and retries nothing; its item keeps its value.
 *
 * It runs in the job task (jobs.c), started from the API or the console by a request the menu takes, as the purge.
 * The person answers its steps from the screen, the API or the console (calibration_answer()); a stop ends it from
 * any of them (activity_request_stop(ACTIVITY_CALIBRATION, by)).
 */
#include <stdbool.h>
#include <stdint.h>

#include "activity.h"
#include "calibration_method.h"

typedef enum {
    CALIBRATION_PART_NONE = 0,
    CALIBRATION_PART_CUP,
    CALIBRATION_PART_SCALE,
    CALIBRATION_PART_POWDER,
} calibration_part_t;

typedef enum {
    CALIBRATION_STEP_NONE = 0,
    CALIBRATION_STEP_CUP_ON,    // the empty cup on the scale: answer next
    CALIBRATION_STEP_ZEROING,   // the scale zeroed with it
    CALIBRATION_STEP_LIFT,      // the cup's part: lifted, its weight read
    CALIBRATION_STEP_BACK,      // put back
    CALIBRATION_STEP_FILL,      // filled by the person, the tubes' speeds set: answer full
    CALIBRATION_STEP_KEEP,      // its capacity shown: answer keep
    CALIBRATION_STEP_EMPTY,     // emptied and put back: answer done
    CALIBRATION_STEP_RUNNING,   // the scale's or a powder's runs
    CALIBRATION_STEP_PAUSED,    // the cup would be passed: emptied and put back, answer go_on
    CALIBRATION_STEP_RESULT,    // the records found: saved (calibration_save()), or a stop discards them
    CALIBRATION_STEP_FAILED,    // what failed: answer done
} calibration_step_t;

// The calibration as GET /api/v1/calibration, the console and the screen give it
typedef struct {
    const char *state;          // idle, starting, running
    calibration_part_t part;
    calibration_step_t step;
    const char *by;             // who started it; NULL with none
    int progress;               // 0 to 100, while running
    char line[40];              // what it does, in words: "Fine tube 0.12 rev/s", "It holds 148 of 150"
    float weight;               // the scale's reading now
    float fine_rps;             // the fill's speeds
    float coarse_rps;
    float capacity;             // the cup's, as filled (keep); NAN before
    float cup_weight;           // as lifted; NAN before
    calibration_scale_record_t scale;       // the scale's records found (result)
    calibration_speeds_record_t fine;       // a powder's (result)
    calibration_speeds_record_t coarse;
    float was_fine_rps;         // the powder's least speeds before
    float was_coarse_rps;
    char failure[CALIBRATION_FAILURE_LEN];  // what failed (failed); empty with none
} calibration_status_t;

typedef enum {
    CALIBRATION_START_OK = 0,
    CALIBRATION_START_BUSY,     // a job runs, a start is asked already, or a person has a screen open at the unit
    CALIBRATION_START_DRIVER_FAULT,
} calibration_start_result_t;

typedef enum {
    CALIBRATION_ANSWER_OK = 0,
    CALIBRATION_ANSWER_NO_JOB,  // no calibration runs
    CALIBRATION_ANSWER_NOT_NOW, // not an answer at this step
} calibration_answer_result_t;

#ifdef __cplusplus
extern "C" {
#endif

// A part asked from outside the menu: handed to the menu with no wait, or refused
calibration_start_result_t calibration_request_start(calibration_part_t part, activity_by_t by);

// The job task's: the part asked, run to its end
uint8_t calibration_job_run(void);

// The person's answer at a step: "next", "full", "keep", "done", "go_on"
calibration_answer_result_t calibration_answer(const char *answer);

// The fill's speeds, either or both (NAN: as it is), within each motor's limits; false when not filling
bool calibration_set_fill(float fine_rps, float coarse_rps);

// The records found kept and stored (the result step); false at another step or when a write failed
bool calibration_save(void);

void calibration_get_status(calibration_status_t *status);

// "cup", "scale", "powder"; NULL for none. "cup_on", "zeroing", ... as the API names the steps; NULL for none
const char *calibration_part_name(calibration_part_t part);
const char *calibration_step_name(calibration_step_t step);
calibration_part_t calibration_part_from_name(const char *name);

// The console's `calibrate`: alone, the state; <cup|scale|powder> starts a part; answer <word>; fill <fine> <coarse>;
// save; stop. "@ok ..." or "@err ..."
bool calibration_console(const char *args, char *reply, size_t reply_size);

#ifdef __cplusplus
}
#endif

#endif  // CALIBRATION_H_
