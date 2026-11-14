// Produces the canned JSON of every web preview scenario with the module's real REST serializers
// (cut out of src/powder_analysis.cpp by extract_sources.py). Prints one JSON object per line:
//   {"name":..., "description":..., "live":..., "state":<status body>, "trials":<body>, "traces":[[chunk,...],...]}
//
// The analysis protocol of the "result" scenarios is produced by running the real tuner against
// the simulated powder of the unit tests, so phases, evaluations and values are consistent.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#define isnanf(x) isnan(x)
#define isinff(x) isinf(x)
#include "powder_analysis.h"
#include "simulated_powder.h"

// ---------------------------------------------------------------------------------------------
// Stubs for the firmware environment the REST handlers touch
// ---------------------------------------------------------------------------------------------
const char *http_json_header = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
const char *boolean_to_string(bool v) { return v ? "true" : "false"; }
static float fake_weight = 0.0f;
float scale_get_current_measurement() { return fake_weight; }
static struct { struct { float coarse_stop_threshold; float fine_stop_threshold; } eeprom_charge_mode_data; } charge_mode_config = {{3.0f, 0.02f}};
typedef enum { OVERRIDE_FROM_REST = 5 } ButtonEncoderEvent_t;
#define APP_STATE_ENTER_POWDER_ANALYSIS 11
#define portMAX_DELAY 0
#define xQueueSend(q, e, t) ((void) (q), (void) (e))
#define TEST_THROW_MAX_GAIN 1000.0f
static bool entered_from_web = false;
static int encoder_event_queue = 1;
static int exit_state = 0;
static powder_analysis_status_t powder_analysis_status;
static powder_analysis_tuner_t tuner;
static powder_analysis_trace_t trace;
bool powder_analysis_post_command(const powder_analysis_command_t *) { return true; }
void powder_analysis_get_status(powder_analysis_status_t *status) { *status = powder_analysis_status; }

// The real result texts, state names and REST handlers
#include "rest_extract.cpp"


// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------
// Body of a handler response (without the HTTP header)
static const char * body_of(bool (*handler)(struct fs_file *, int, char **, char **), int argc, char **params, char **values) {
    struct fs_file file;
    handler(&file, argc, params, values);
    const char *body = strstr(file.data, "\r\n\r\n");
    return body ? body + 4 : file.data;
}

static void print_json_string(const char *text) {
    putchar('"');
    for (const char *c = text; *c; c++) {
        if (*c == '"' || *c == '\\') {
            putchar('\\');
        }
        putchar(*c);
    }
    putchar('"');
}

// One scenario line. The trace sets (recorded throws) are serialized in chunks like the device does.
static void emit(const char *name, const char *description, const char *live,
                 const powder_analysis_trace_t *trace_sets, int trace_set_count) {
    printf("{\"name\":\"%s\",\"description\":", name);
    print_json_string(description);
    printf(",\"live\":\"%s\",\"state\":%s", live, body_of(http_rest_powder_analysis_state, 0, NULL, NULL));
    printf(",\"trials\":%s,\"traces\":[", body_of(http_rest_powder_analysis_trials, 0, NULL, NULL));
    for (int set = 0; set < trace_set_count; set++) {
        trace = trace_sets[set];
        printf("%s[", set > 0 ? "," : "");
        for (int from = 0; ; from += 40) {
            char from_text[16];
            snprintf(from_text, sizeof(from_text), "%d", from);
            const char *params[] = {"from"};
            const char *values[] = {from_text};
            printf("%s%s", from > 0 ? "," : "", body_of(http_rest_powder_analysis_trace, 1, (char **) params, (char **) values));
            if (from + 40 >= trace.count) {
                break;
            }
        }
        printf("]");
    }
    printf("]}\n");
}

static void reset(void) {
    memset(&powder_analysis_status, 0x0, sizeof(powder_analysis_status));
    memset(&tuner, 0x0, sizeof(tuner));
    memset(&trace, 0x0, sizeof(trace));
    powder_analysis_status.target_weight = 23.1f;
    powder_analysis_status.shape = POWDER_SHAPE_STICK;
    powder_analysis_status.grain_size = GRAIN_SIZE_MEDIUM;
    powder_analysis_status.zero_available = true;
    fake_weight = 0.0f;
}

static void prompt(const char *text) {
    snprintf(powder_analysis_status.prompt, sizeof(powder_analysis_status.prompt), "%s", text);
}

static void copy_last_trial(void) {
    powder_analysis_status.last_trial = tuner.trials[tuner.trial_count - 1];
    powder_analysis_status.has_last_trial = true;
    powder_analysis_status.phase = tuner.phase;
}

// A hand-made full throw reported to the tuner (stop weight for the fall statistics)
static void report(float weight, float coarse_time, float total_time) {
    powder_analysis_trial_result_t r;
    memset(&r, 0x0, sizeof(r));
    r.weight = weight;
    r.coarse_time_s = coarse_time;
    r.total_time_s = total_time;
    r.stop_weight = powder_analysis_status.target_weight - charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold;
    powder_analysis_tuner_report_result(&tuner, &r);
    copy_last_trial();
    powder_analysis_status.trial.trial_number = tuner.trial_count + 1;
}

// An N135 like simulated powder: coarse Kp edge 0.064, fine Kp edge 3.4, throws of about 8 s
static void n135_powder(simulated_powder_t *powder) {
    simulated_powder_init_default(powder, 23.1f, 3.0f, 0.02f);
    powder->coarse_kp_threshold = 0.064f;
    powder->coarse_time_base = 0.22f;
    powder->fine_kp_threshold = 3.4f;
    powder->fine_time_base = 14.0f;
    powder->fine_kd_slowdown = 0.0f;
}

// Run the real tuner against the simulated powder until the given phase
static void run_until(powder_analysis_phase_t phase, simulated_powder_t *powder) {
    powder_analysis_tuner_init(&tuner, POWDER_SHAPE_STICK, GRAIN_SIZE_MEDIUM, 23.1f, 3.0f, 0.02f);
    n135_powder(powder);
    while (tuner.phase != phase && !powder_analysis_tuner_is_done(&tuner)) {
        powder_analysis_trial_params_t params;
        powder_analysis_trial_result_t result;
        powder_analysis_tuner_get_trial(&tuner, &params);
        simulated_powder_throw(powder, &params, &result);
        if (params.throw_type == POWDER_ANALYSIS_THROW_FULL && !result.timed_out) {
            result.stop_weight = 23.1f - 0.02f;
        }
        powder_analysis_tuner_report_result(&tuner, &result);
    }
    copy_last_trial();
    powder_analysis_tuner_get_trial(&tuner, &powder_analysis_status.trial);
    powder_analysis_status.phase = tuner.phase;
    powder_analysis_status.verify_round = tuner.verify_reached ? tuner.verify_round.number : 0;
    powder_analysis_status.verify_round_hits = tuner.verify_round.hit_count;
    powder_analysis_status.fine_corrections = tuner.fine_corrections;
}

static void finish_result(void) {
    powder_analysis_tuner_get_result(&tuner, &powder_analysis_status.result);
    powder_analysis_status.has_result = true;
    powder_analysis_status.phase = POWDER_ANALYSIS_PHASE_DONE;
    powder_analysis_status.profile_idx = 4;
    snprintf(powder_analysis_status.profile_name, sizeof(powder_analysis_status.profile_name), "analysis_1");
    prompt("Analysis finished");
}

// A recorded test throw shaped like measurement 9 (N320, fine Kp 4.8, target 8.2, threshold 0.04):
// same weight curve for every Kd, the D term tiny and alternating between 0 and a value
static void make_trace(powder_analysis_trace_t *t, uint16_t id, float fine_kd, float final_weight) {
    memset(t, 0x0, sizeof(*t));
    t->id = id;
    t->test_throw = true;
    t->params.throw_type = POWDER_ANALYSIS_THROW_FULL;
    t->params.coarse_kp = 0.11f;
    t->params.coarse_kd = 0.1f;
    t->params.fine_kp = 4.8f;
    t->params.fine_kd = fine_kd;
    t->params.fine_min_flow_speed_rps = 0.06f;
    t->final_weight = final_weight;

    const float target = 8.2f;
    const float stop = target - 0.04f;
    float weight = 0.0f;
    for (int i = 0; ; i++) {
        float time = i * 0.05f;
        bool coarse_moving = weight < target - 3.0f;
        if (coarse_moving) {
            weight += 0.22f;                    // coarse phase, about 2 s
        }
        else if (i % 2 == 0) {
            weight += 0.02f;                    // the scale changes every other sample in 0.02 gr steps
        }
        if (weight > stop) {
            weight = stop + 0.01f;
        }
        float fine_error = target - weight;
        float fine_p = 4.8f * fine_error;
        float fine_d = (i % 2 == 0 && !coarse_moving) ? -fine_kd * 0.0002f : 0.0f;
        float fine_speed = fmaxf(0.06f, fminf(fine_p + fine_d, 5.0f));
        float coarse_p = coarse_moving ? 0.11f * (target - 3.0f - weight) : 0.0f;
        float coarse_speed = coarse_moving ? fmaxf(0.1f, fminf(coarse_p, 5.0f)) : 0.0f;
        bool stop_sample = fine_error < 0.04f;

        powder_analysis_trace_sample_t *s = &t->samples[t->count++];
        s->t_s = time;
        s->weight = weight;
        s->fine_p = fine_p;
        s->fine_d = fine_d;
        s->fine_speed = stop_sample ? 0.0f : fine_speed;
        s->coarse_p = coarse_p;
        s->coarse_d = 0.0f;
        s->coarse_speed = stop_sample ? 0.0f : coarse_speed;
        if (stop_sample || t->count >= POWDER_ANALYSIS_TRACE_MAX_SAMPLES - 1) {
            break;
        }
    }
}


int main(void) {
    simulated_powder_t powder;
    powder_analysis_trace_t sets[4];

    // 1. Idle
    reset();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_IDLE;
    fake_weight = 0.02f;
    emit("idle", "Device idle (main menu), nothing running", "", NULL, 0);

    // 2. Setup on the display
    reset();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_SETUP_SHAPE;
    prompt("Enter target weight");
    emit("setup_display", "Analysis being set up on the display (shape screen)", "", NULL, 0);

    // 3. Flooding
    reset();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_FLOODING;
    powder_analysis_status.flooding_running = true;
    fake_weight = 12.46f;
    prompt("Fill the tubes");
    emit("flooding_running", "Flooding, both motors running", "flooding", NULL, 0);

    // 4. Empty pan after a throw (coarse Kp phase)
    reset();
    run_until(POWDER_ANALYSIS_PHASE_COARSE_KD, &powder);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL;
    fake_weight = powder_analysis_status.last_trial.weight;
    prompt("Empty pan into hopper");
    emit("empty_pan_after_throw", "Waiting for the pan to be emptied after a coarse Kp throw", "", NULL, 0);

    // 5. Throw running (coarse Kp)
    reset();
    run_until(POWDER_ANALYSIS_PHASE_COARSE_KD, &powder);
    tuner.trial_count -= 3;     // pretend we are still in the Kp search, third throw
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_TRIAL;
    powder_analysis_status.trial.trial_number = tuner.trial_count + 1;
    powder_analysis_status.trial.phase = POWDER_ANALYSIS_PHASE_COARSE_KP;
    powder_analysis_status.trial.throw_type = POWDER_ANALYSIS_THROW_COARSE_ONLY;
    powder_analysis_status.trial.coarse_kp = 0.12f;
    powder_analysis_status.trial.coarse_kd = 0.0f;
    powder_analysis_status.trial.fine_kp = 0.0f;
    powder_analysis_status.trial.fine_kd = 0.0f;
    powder_analysis_status.phase = POWDER_ANALYSIS_PHASE_COARSE_KP;
    powder_analysis_status.elapsed_s = 0.4f;
    fake_weight = 1.2f;
    prompt("Throw running");
    emit("throw_running_coarse_kp", "Throw running in the coarse Kp search (weight and time advance while polling)", "trial", NULL, 0);

    // 6. Verification round 2 after a Kp correction
    reset();
    run_until(POWDER_ANALYSIS_PHASE_VERIFY, &powder);
    report(23.14f, 4.1f, 8.3f);                 // overthrow
    report(23.10f, 4.0f, 8.1f);                 // hit
    report(23.15f, 4.2f, 8.4f);                 // overthrow -> round given up, Kp -10 %
    powder_analysis_status.correction_just_applied = true;
    powder_analysis_status.correction_lowered_kp = tuner.last_correction_lowered_kp;
    powder_analysis_status.fine_corrections = tuner.fine_corrections;
    powder_analysis_status.verify_round = tuner.verify_round.number;
    powder_analysis_status.verify_round_hits = tuner.verify_round.hit_count;
    powder_analysis_tuner_get_trial(&tuner, &powder_analysis_status.trial);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL;
    fake_weight = 23.15f;
    prompt("Empty pan into hopper");
    emit("verify_round2_correction", "Verification: round 1 given up after 2 overthrows, fine Kp -10 % (1/3), round 2 starts", "", NULL, 0);

    // 7. Paused
    reset();
    run_until(POWDER_ANALYSIS_PHASE_FINE_KP, &powder);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_PAUSED;
    fake_weight = -0.01f;
    prompt("Paused, refill hopper");
    emit("paused", "Paused between throws, scale can be zeroed", "", NULL, 0);

    // 8. Abort confirmation
    reset();
    run_until(POWDER_ANALYSIS_PHASE_FINE_KP, &powder);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_ABORT_CONFIRM;
    fake_weight = 5.3f;
    prompt("Abort analysis?");
    emit("abort_confirm", "Abort question after Abort was pressed during a throw", "", NULL, 0);

    // 9. Result verified 9/10 with the fall hint (full protocol from the simulated run)
    reset();
    run_until(POWDER_ANALYSIS_PHASE_VERIFY, &powder);
    const float verify_weights[10] = {23.11f, 23.09f, 23.16f, 23.10f, 23.12f, 23.10f, 23.08f, 23.11f, 23.09f, 23.10f};
    for (int i = 0; i < 10; i++) {
        report(verify_weights[i], 4.1f, 8.3f);  // one overthrow (23.16)
    }
    finish_result();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_RESULT_FINE;
    fake_weight = 23.10f;
    emit("result_verified", "Analysis finished: verified 9/10, fall hint, full protocol, profile list for saving", "", NULL, 0);

    // 10. Result not verified: overthrows after 3 corrections
    reset();
    run_until(POWDER_ANALYSIS_PHASE_VERIFY, &powder);
    for (int round = 0; round < 4; round++) {
        report(23.10f, 4.1f, 8.3f);
        report(23.15f, 4.2f, 8.4f);
        report(23.09f, 4.0f, 8.1f);
        report(23.17f, 4.1f, 8.5f);
    }
    finish_result();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_RESULT_THRESHOLD;     // device on the threshold page
    fake_weight = 23.17f;
    emit("result_not_verified", "Analysis finished without verification: overthrows persisted through 3 corrections", "", NULL, 0);

    // 11. Use now question
    reset();
    run_until(POWDER_ANALYSIS_PHASE_VERIFY, &powder);
    for (int i = 0; i < 10; i++) {
        report(23.10f, 4.1f, 8.2f);
    }
    finish_result();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_ACTIVATE;
    prompt("Analysis finished");
    emit("use_now", "Profile saved, device asks whether to use it now", "", NULL, 0);

    // 12. Test throw done with four recorded throws (fine Kd 0 / 4 / 16 / 64)
    reset();
    powder_analysis_status.target_weight = 8.2f;
    charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold = 0.04f;
    powder_analysis_status.test_mode = true;
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_TEST_DONE;
    powder_analysis_status.trial.throw_type = POWDER_ANALYSIS_THROW_FULL;
    powder_analysis_status.trial.phase = POWDER_ANALYSIS_PHASE_DONE;
    powder_analysis_status.trial.coarse_kp = 0.11f;
    powder_analysis_status.trial.coarse_kd = 0.1f;
    powder_analysis_status.trial.fine_kp = 4.8f;
    powder_analysis_status.trial.fine_kd = 64.0f;
    powder_analysis_status.trial.fine_min_flow_speed_rps = 0.06f;
    powder_analysis_status.last_trial.phase = POWDER_ANALYSIS_PHASE_DONE;
    powder_analysis_status.last_trial.kp = 4.8f;
    powder_analysis_status.last_trial.kd = 64.0f;
    powder_analysis_status.last_trial.weight = 8.23f;
    powder_analysis_status.last_trial.coarse_time_s = 2.1f;
    powder_analysis_status.last_trial.total_time_s = 7.6f;
    powder_analysis_status.last_trial.evaluation = POWDER_ANALYSIS_EVAL_HIT;
    powder_analysis_status.last_trial.fine_min_flow_speed_rps = 0.06f;
    powder_analysis_status.has_last_trial = true;
    powder_analysis_status.elapsed_s = 7.6f;
    fake_weight = 8.23f;
    prompt("Test throw done");
    make_trace(&sets[0], 1, 0.0f, 8.22f);
    make_trace(&sets[1], 2, 4.0f, 8.24f);
    make_trace(&sets[2], 3, 16.0f, 8.21f);
    make_trace(&sets[3], 4, 64.0f, 8.23f);
    trace = sets[3];
    emit("test_throw_done", "Test throw done, four recorded throws with fine Kd 0 / 4 / 16 / 64 (same weights, tiny D term)", "", sets, 4);

    return 0;
}
