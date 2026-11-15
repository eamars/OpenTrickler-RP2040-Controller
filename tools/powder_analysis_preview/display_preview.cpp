// Renders every display screen of the powder analysis into a PBM file (128x64) using the real
// drawing code of src/powder_analysis.cpp (cut out by extract_sources.py) and the u8g2 bitmap
// backend. Writes <out dir>/NN_name.pbm and <out dir>/screens.json.
//
//   display_preview <out dir>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "u8g2.h"
#include "powder_analysis.h"

// ---------------------------------------------------------------------------------------------
// Stubs for what the drawing code takes from the firmware
// ---------------------------------------------------------------------------------------------
#define WEIGHT_STRING_LEN 8
#define WEIGHT_DIGIT_COUNT 5

typedef enum { DP_2 = 0, DP_3 = 1 } decimal_places_t;

static struct {
    struct {
        float coarse_stop_threshold;
        float fine_stop_threshold;
        decimal_places_t decimal_places;
    } eeprom_charge_mode_data;
} charge_mode_config = {{3.0f, 0.02f, DP_2}};

eeprom_profile_data_t profile_data;
static powder_analysis_status_t powder_analysis_status;
static uint8_t cursor = 0;
static uint8_t weight_digits[WEIGHT_DIGIT_COUNT];

static int float_to_string(char *out, float value, decimal_places_t decimal_places) {
    return sprintf(out, decimal_places == DP_3 ? "%.3f" : "%.2f", value);
}

// The real result texts, drawing functions and state names
#include "draw_extract.cpp"


// ---------------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------------
static u8g2_t u8g2;

// Mirrors the switch of render_task() in src/powder_analysis.cpp
static void render(void) {
    u8g2_ClearBuffer(&u8g2);
    switch (powder_analysis_status.state) {
        case POWDER_ANALYSIS_STATE_SETUP_WEIGHT:
            draw_setup_weight(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_SETUP_SHAPE:
            draw_title(&u8g2, "Powder Shape");
            draw_list(&u8g2, powder_shape_names, POWDER_SHAPE_COUNT, 0, 4);
            break;
        case POWDER_ANALYSIS_STATE_SETUP_GRAIN:
            draw_title(&u8g2, "Grain Size");
            draw_list(&u8g2, grain_size_names, GRAIN_SIZE_COUNT, 0, 3);
            break;
        case POWDER_ANALYSIS_STATE_PREPARE:
            draw_prepare(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_FLOODING:
            draw_flooding(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL:
        case POWDER_ANALYSIS_STATE_WAIT_PAN_RETURN:
        case POWDER_ANALYSIS_STATE_WAIT_ZERO:
            draw_empty_pan(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_TRIAL:
        case POWDER_ANALYSIS_STATE_TEST_THROW:
            draw_trial(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_TEST_DONE:
            draw_test_done(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_PAUSED:
            draw_paused(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_ABORT_CONFIRM:
            draw_abort_confirm(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_RESULT_COARSE:
            draw_result_coarse(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_RESULT_FINE:
            draw_result_fine(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_RESULT_THRESHOLD:
            draw_result_threshold(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_SAVE_SLOT:
            draw_save_slot(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_SAVE_CONFIRM:
            draw_save_confirm(&u8g2);
            break;
        case POWDER_ANALYSIS_STATE_ACTIVATE:
            draw_activate(&u8g2);
            break;
        default:
            draw_title(&u8g2, "Powder Analysis");
            break;
    }
    u8g2_SendBuffer(&u8g2);
}


static FILE *screens_json = NULL;
static int screen_count = 0;

// Render the current status as one screen: PBM file plus an entry in screens.json
static void screen(const char *out_dir, const char *name, const char *description, const char *values) {
    render();

    char path[512];
    snprintf(path, sizeof(path), "%s/%02d_%s.pbm", out_dir, screen_count, name);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fprintf(f, "P1\n128 64\n");
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            fputc(u8x8_GetBitmapPixel(u8g2_GetU8x8(&u8g2), x, y) ? '1' : '0', f);
        }
        fputc('\n', f);
    }
    fclose(f);

    fprintf(screens_json, "%s{\"file\":\"%02d_%s.png\",\"name\":\"%s\",\"description\":\"%s\",\"values\":\"%s\"}\n",
            screen_count > 0 ? "," : "", screen_count, name, name, description, values);
    screen_count += 1;
}


// ---------------------------------------------------------------------------------------------
// Dummy data (N135 stick / medium like numbers, target 23.1 gr)
// ---------------------------------------------------------------------------------------------
static void reset_status(void) {
    memset(&powder_analysis_status, 0x0, sizeof(powder_analysis_status));
    powder_analysis_status.target_weight = 23.1f;
    powder_analysis_status.shape = POWDER_SHAPE_STICK;
    powder_analysis_status.grain_size = GRAIN_SIZE_MEDIUM;
    powder_analysis_status.zero_available = true;
    powder_analysis_status.current_weight = 0.0f;
    cursor = 0;
}

static void set_trial(uint8_t number, powder_analysis_phase_t phase, powder_analysis_throw_type_t type,
                      float ckp, float ckd, float fkp, float fkd) {
    powder_analysis_trial_params_t *t = &powder_analysis_status.trial;
    t->trial_number = number;
    t->phase = phase;
    t->throw_type = type;
    t->coarse_kp = ckp;
    t->coarse_kd = ckd;
    t->fine_kp = fkp;
    t->fine_kd = fkd;
    t->fine_min_flow_speed_rps = 0.06f;
    powder_analysis_status.phase = phase;
}

static void set_last_trial(uint8_t number, powder_analysis_phase_t phase, float kp, float kd, float weight,
                           float coarse_time, float total_time, powder_analysis_evaluation_t evaluation) {
    powder_analysis_trial_t *t = &powder_analysis_status.last_trial;
    t->number = number;
    t->phase = phase;
    t->kp = kp;
    t->kd = kd;
    t->weight = weight;
    t->coarse_time_s = coarse_time;
    t->total_time_s = total_time;
    t->evaluation = evaluation;
    t->fine_min_flow_speed_rps = 0.06f;
    powder_analysis_status.has_last_trial = true;
}

static void set_result(bool verified, uint8_t hits, uint8_t corrections, bool overthrow_limit) {
    powder_analysis_result_t *r = &powder_analysis_status.result;
    memset(r, 0x0, sizeof(*r));
    r->coarse_kp = 0.0675f;
    r->coarse_kd = 0.1f;
    r->coarse_min_flow_speed_rps = 0.1f;
    r->coarse_max_flow_speed_rps = 5.0f;
    r->fine_kp = corrections > 0 ? 3.3f * 0.9f : 3.3f;
    r->fine_kd = 8.0f;
    r->fine_min_flow_speed_rps = 0.06f;
    r->fine_max_flow_speed_rps = 5.0f;
    r->verified = verified;
    r->verify_reached = true;
    r->verify_hits = hits;
    r->fine_corrections = corrections;
    r->verify_overthrow_limit_reached = overthrow_limit;
    r->coarse_base_time_s = 3.6f;
    r->coarse_time_s = 4.1f;
    r->coarse_overshoot_gr = 0.55f;
    r->total_time_s = 8.3f;
    r->trial_count = 32;
    r->fall_count = 5;
    r->fall_median_gr = 0.016f;
    r->fall_max_gr = 0.056f;
    r->recommended_fine_threshold = 0.03f;
    r->expected_final_offset_gr = -0.004f;
    powder_analysis_status.has_result = true;
    powder_analysis_status.profile_idx = 4;
    snprintf(powder_analysis_status.profile_name, sizeof(powder_analysis_status.profile_name), "analysis_1");
}

static void weight_to_digits(float weight) {
    int hundredths = (int) roundf(weight * 100.0f);
    for (int i = 0; i < WEIGHT_DIGIT_COUNT; i++) {
        weight_digits[i] = hundredths % 10;
        hundredths /= 10;
    }
}


int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: display_preview <out dir>\n");
        return 1;
    }
    const char *out_dir = argv[1];

    u8g2_SetupBitmap(&u8g2, &u8g2_cb_r0, 128, 64);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    const char *names[MAX_PROFILE_CNT] = {"AR2208,gr", "AR2209,gr", "8208XBR,gr", "Benchmark2,gr", "N135 stick", "analysis_1", "Profile6", "Profile7"};
    for (int i = 0; i < MAX_PROFILE_CNT; i++) {
        snprintf(profile_data.profiles[i].name, PROFILE_NAME_MAX_LEN, "%s", names[i]);
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/screens.json", out_dir);
    screens_json = fopen(path, "w");
    if (screens_json == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(screens_json, "[");

    // D1 target weight
    reset_status();
    weight_to_digits(23.1f);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_SETUP_WEIGHT;
    cursor = 2;
    screen(out_dir, "d1_target_weight", "D1 Target weight, cursor on the ones digit", "23.10 gr, cursor 2");
    cursor = WEIGHT_DIGIT_COUNT;
    screen(out_dir, "d1_target_weight_next", "D1 Target weight, cursor on Next", "23.10 gr, cursor Next");

    // D2 / D3 lists
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_SETUP_SHAPE;
    cursor = 1;
    screen(out_dir, "d2_shape", "D2 Powder shape, Stick selected", "cursor 1 (Stick)");
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_SETUP_GRAIN;
    cursor = 1;
    screen(out_dir, "d3_grain", "D3 Grain size, Medium selected", "cursor 1 (Medium)");

    // D4 prepare
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_PREPARE;
    powder_analysis_status.current_weight = 0.02f;
    charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold = 0.03f;
    screen(out_dir, "d4_prepare", "D4 Prepare: thresholds and pan hint", "coarse stop 3.0, fine stop 0.03");
    charge_mode_config.eeprom_charge_mode_data.fine_stop_threshold = 0.02f;
    screen(out_dir, "d4_prepare_exact_step", "D4 Prepare: fine stop threshold on an exact scale step, with the tip", "coarse stop 3.0, fine stop 0.02 (exact step)");
    charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold = 10.0f;
    screen(out_dir, "d4_prepare_exact_step_long", "D4 Prepare: exact step with a two-digit coarse threshold (longest text)", "coarse stop 10.0, fine stop 0.02");
    charge_mode_config.eeprom_charge_mode_data.coarse_stop_threshold = 3.0f;

    // D5 flooding
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_FLOODING;
    powder_analysis_status.current_weight = 0.0f;
    screen(out_dir, "d5_flooding_stopped", "D5 Flooding stopped, cursor on Start", "flooding_running false");
    powder_analysis_status.flooding_running = true;
    powder_analysis_status.current_weight = 12.46f;
    cursor = 1;
    screen(out_dir, "d5_flooding_running", "D5 Flooding running, cursor on Done", "flooding_running true, weight 12.46");

    // D6 empty pan after flooding (no throw yet)
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL;
    powder_analysis_status.current_weight = 12.46f;
    screen(out_dir, "d6_empty_pan", "D6 Empty pan after flooding", "no last trial, weight 12.46");

    // D7 trial running
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_TRIAL;
    set_trial(3, POWDER_ANALYSIS_PHASE_COARSE_KP, POWDER_ANALYSIS_THROW_COARSE_ONLY, 0.12f, 0.0f, 0.0f, 0.0f);
    powder_analysis_status.current_weight = 14.82f;
    powder_analysis_status.elapsed_s = 2.4f;
    screen(out_dir, "d7_trial_coarse_kp", "D7 Throw #3 running, coarse Kp search", "coarse kp 0.12 kd 0, weight 14.82 of 23.10, 2.4 s");
    powder_analysis_status.target_weight = 300.0f;
    powder_analysis_status.current_weight = 123.456f;
    screen(out_dir, "d7_trial_long_weight", "D7 Throw running with the longest weight text (300 gr target)", "weight 123.456 of 300.00");
    powder_analysis_status.target_weight = 23.1f;
    set_trial(17, POWDER_ANALYSIS_PHASE_FINE_KP, POWDER_ANALYSIS_THROW_FULL, 0.0675f, 0.1f, 3.375f, 8.0f);
    powder_analysis_status.current_weight = 22.64f;
    powder_analysis_status.elapsed_s = 6.8f;
    screen(out_dir, "d7_trial_fine_kp", "D7 Throw #17 running, fine Kp search", "fine kp 3.375 kd 8.0, weight 22.64, 6.8 s");

    // D8 trial result (empty pan prompt with the last throw)
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_WAIT_PAN_REMOVAL;
    set_last_trial(17, POWDER_ANALYSIS_PHASE_FINE_KP, 3.375f, 8.0f, 23.14f, 4.1f, 8.3f, POWDER_ANALYSIS_EVAL_OVER);
    powder_analysis_status.current_weight = 23.14f;
    screen(out_dir, "d8_trial_result_over", "D8 Last throw #17: 23.14 gr, 8.3 s, OVER", "last trial OVER, weight 23.14");
    set_last_trial(21, POWDER_ANALYSIS_PHASE_VERIFY, 3.375f, 8.0f, 23.16f, 4.0f, 8.1f, POWDER_ANALYSIS_EVAL_OVER);
    powder_analysis_status.verify_round = 2;
    powder_analysis_status.fine_corrections = 1;
    powder_analysis_status.correction_just_applied = true;
    powder_analysis_status.correction_lowered_kp = true;
    screen(out_dir, "d8_trial_result_kp_correction", "D8 Verification round given up: Kp -10% (1/3)", "last trial OVER, correction_just_applied, lowered_kp");
    powder_analysis_status.correction_lowered_kp = false;
    screen(out_dir, "d8_trial_result_minspd_correction", "D8 Verification round given up: MinSpd -25% (1/3)", "last trial OVER, correction_just_applied, lowered min speed");

    // D9 paused
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_PAUSED;
    powder_analysis_status.current_weight = -0.01f;
    screen(out_dir, "d9_paused_zero", "D9 Paused with Zero button (scale driver can zero)", "zero_available true, cursor Resume");
    cursor = 1;
    screen(out_dir, "d9_paused_zero_selected", "D9 Paused, cursor on Zero", "zero_available true, cursor Zero");
    powder_analysis_status.zero_available = false;
    cursor = 0;
    screen(out_dir, "d9_paused_no_zero", "D9 Paused without Zero (generic scale driver)", "zero_available false");

    // D10 abort
    reset_status();
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_ABORT_CONFIRM;
    screen(out_dir, "d10_abort_no", "D10 Abort question, cursor on No", "cursor 0");
    cursor = 1;
    screen(out_dir, "d10_abort_yes", "D10 Abort question, cursor on Yes", "cursor 1");

    // D11 / D12 results
    reset_status();
    set_result(true, 9, 1, false);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_RESULT_COARSE;
    screen(out_dir, "d11_result_coarse", "D11 Result 1/3: coarse values", "kp 0.0675 kd 0.1, time 4.1 s, overshoot 0.55 gr, hint OK");
    powder_analysis_status.result.coarse_overshoot_too_high = true;
    powder_analysis_status.result.coarse_overshoot_gr = 1.4f;
    screen(out_dir, "d11_result_coarse_overshoot", "D11 Result 1/3 with the overshoot warning", "coarse_overshoot_too_high, overshoot 1.4 gr");
    powder_analysis_status.result.coarse_time_s = 12.3f;
    powder_analysis_status.result.coarse_overshoot_gr = -0.55f;
    screen(out_dir, "d11_result_coarse_long", "D11 Result 1/3 with the longest time line", "time 12.3 s, overshoot -0.55 gr");
    set_result(true, 9, 1, false);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_RESULT_FINE;
    screen(out_dir, "d12_result_fine_verified_corr", "D12 Result 2/3: verified 9/10 after one correction", "verified, hits 9, corrections 1");
    set_result(true, 10, 0, false);
    screen(out_dir, "d12_result_fine_verified", "D12 Result 2/3: verified 10/10", "verified, hits 10");
    set_result(false, 6, 3, true);
    screen(out_dir, "d12_result_fine_not_verified", "D12 Result 2/3: not verified, overthrows after 3 corrections", "not verified, hits 6, overthrow limit");

    // D13 threshold recommendation
    set_result(true, 9, 1, false);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_RESULT_THRESHOLD;
    screen(out_dir, "d13_result_threshold", "D13 Result 3/3: recommended fine stop threshold, cursor on Save", "rec. 0.03, lands -0.004, current 0.02");
    powder_analysis_status.result.recommended_fine_threshold = 0.07f;
    powder_analysis_status.result.expected_final_offset_gr = 0.012f;
    cursor = 1;
    screen(out_dir, "d13_result_threshold_discard", "D13 Result 3/3 with a larger recommendation, cursor on Discard", "rec. 0.07, lands +0.012, cursor 1");
    powder_analysis_status.result.fall_count = 0;
    cursor = 0;
    screen(out_dir, "d13_result_threshold_no_data", "D13 Result 3/3 without fall data (no full throw measured)", "fall_count 0");

    // D14 - D16 saving
    reset_status();
    set_result(true, 9, 1, false);
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_SAVE_SLOT;
    cursor = 4;
    screen(out_dir, "d14_save_slot", "D14 Save slot list, cursor on slot 4", "cursor 4 of 8 slots");
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_SAVE_CONFIRM;
    cursor = 1;
    screen(out_dir, "d15_save_confirm", "D15 Overwrite slot 4 as analysis_1, cursor on Save", "profile_idx 4, name analysis_1");
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_ACTIVATE;
    cursor = 0;
    screen(out_dir, "d16_activate", "D16 Use this profile now?", "cursor No");

    // Test throw
    reset_status();
    powder_analysis_status.test_mode = true;
    powder_analysis_status.target_weight = 8.2f;
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_TEST_THROW;
    set_trial(0, POWDER_ANALYSIS_PHASE_DONE, POWDER_ANALYSIS_THROW_FULL, 0.11f, 0.1f, 4.8f, 16.0f);
    powder_analysis_status.current_weight = 6.3f;
    powder_analysis_status.elapsed_s = 3.1f;
    screen(out_dir, "test_throw_running", "Test throw running (web started)", "fine kp 4.8 kd 16, weight 6.30, 3.1 s");
    powder_analysis_status.state = POWDER_ANALYSIS_STATE_TEST_DONE;
    set_last_trial(0, POWDER_ANALYSIS_PHASE_DONE, 4.8f, 16.0f, 8.22f, 2.1f, 7.4f, POWDER_ANALYSIS_EVAL_HIT);
    screen(out_dir, "test_throw_done", "Test throw done", "8.22 gr, 7.4 s, HIT, kp 4.8 kd 16, min 0.060");

    fprintf(screens_json, "]\n");
    fclose(screens_json);
    printf("%d display screens written to %s\n", screen_count, out_dir);
    return 0;
}
