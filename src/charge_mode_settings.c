/*
 * Charge mode's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/settings/charge). In C beside
 * charge_mode.cpp: the table is written with C's designated initializers.
 */
#include <math.h>
#include <stddef.h>

#include "charge_mode.h"
#include "common.h"
#include "eeprom.h"
#include "settings.h"

extern charge_mode_config_t charge_mode_config;


static const setting_choice_t decimal_places_choices[] = {
    {.held = DP_2, .number = 2, .label = "2 decimals", .short_label = "2"},
    {.held = DP_3, .number = 3, .label = "3 decimals", .short_label = "3"},
};


#define CHARGE_FIELD(member) SETTING_AT(eeprom_charge_mode_data_t, member)

static const setting_field_t charge_fields[] = {
    {.name = "coarse_stop_threshold", .label = "Coarse stop threshold", .group = "Thresholds",
     .short_label = "Coarse stop", .edit_step = 0.1f,
     .about = "The coarse trickler stops this far below the target",
     .type = SETTING_NUMBER, CHARGE_FIELD(coarse_stop_threshold), .unit = "weight", .min = 0.0f, .max = 100.0f,
     .applies = "now"},
    {.name = "fine_stop_threshold", .label = "Fine stop threshold", .group = "Thresholds",
     .short_label = "Fine stop", .edit_step = 0.01f,
     .about = "The fine trickler stops this far below the target; a throw further off than it is over or under",
     .type = SETTING_NUMBER, CHARGE_FIELD(fine_stop_threshold), .unit = "weight", .min = 0.0f, .max = 10.0f,
     .applies = "now"},
    {.name = "steady_sd_margin", .label = "Steady margin (SD)", .group = "Thresholds",
     .about = "The readings are steady when their standard deviation is below it: for the zero, the cup's removal "
              "and the resume after a pause. The scale's calibration sets it from the scale's own step",
     .type = SETTING_NUMBER, CHARGE_FIELD(set_point_sd_margin), .unit = "weight", .min = 0.0f, .max = 1.0f,
     .applies = "now"},
    {.name = "zero_mean_margin", .label = "Zero margin (mean)", .group = "Thresholds",
     .about = "The scale reads zero when the mean of its readings is this near it. The scale's calibration sets it "
              "from the scale's own step",
     .type = SETTING_NUMBER, CHARGE_FIELD(set_point_mean_margin), .unit = "weight", .min = 0.0f, .max = 1.0f,
     .applies = "now"},
    {.name = "decimal_places", .label = "Decimal places", .group = "Display", .short_label = "Decimals",
     .about = "Decimals of the weight and of the target on the screen. A change keeps the target as a weight, and "
              "is refused when the target cannot be shown at the new decimals",
     .type = SETTING_CHOICE, CHARGE_FIELD(decimal_places), SETTING_CHOICES(decimal_places_choices),
     .applies = "now"},
    {.name = "coarse_stop_gate_ratio", .label = "Gate at the coarse stop", .group = "Coarse stop",
     .about = "Where the gate goes when the coarse trickler stops: 0 open, 1 closed; null, it is not moved",
     .type = SETTING_NUMBER, CHARGE_FIELD(coarse_stop_gate_ratio), .min = 0.0f, .max = 1.0f,
     .nullable = true, .null_held = -1.0f, .applies = "now"},
    {.name = "coarse_stop_backoff_enable", .label = "Back off at the coarse stop", .group = "Coarse stop",
     .about = "The coarse trickler turns back a little when it stops",
     .type = SETTING_BOOLEAN, CHARGE_FIELD(coarse_stop_backoff_enable), .applies = "now"},
    {.name = "coarse_stop_backoff_turns", .label = "Back-off turns", .group = "Coarse stop",
     .about = "How far it turns back",
     .type = SETTING_NUMBER, CHARGE_FIELD(coarse_stop_backoff_turns), .unit = "turns", .min = 0.125f, .max = 2.0f,
     .step = 0.125f, .applies = "now"},
    {.name = "coarse_stop_backoff_speed_rps", .label = "Back-off speed", .group = "Coarse stop",
     .about = "How fast it turns back; 0 is the coarse motor's minimum speed, and it is used up to its maximum",
     .type = SETTING_NUMBER, CHARGE_FIELD(coarse_stop_backoff_speed_rps), .unit = "rev/s", .min = 0.0f,
     .max = 100.0f, .applies = "now"},
    {.name = "precharge_enable", .label = "Precharge", .group = "Precharge",
     .about = "At a throw's end, with the gate closed, the coarse trickler runs a while so powder waits behind the "
              "gate for the next; only with the servo gate enabled",
     .type = SETTING_BOOLEAN, CHARGE_FIELD(precharge_enable), .applies = "now"},
    {.name = "precharge_time_ms", .label = "Precharge time", .group = "Precharge",
     .about = "How long the precharge runs; the RST button is not read meanwhile",
     .type = SETTING_INTEGER, CHARGE_FIELD(precharge_time_ms), .unit = "ms", .min = 0.0f, .max = 5000.0f,
     .applies = "now"},
    {.name = "precharge_speed_rps", .label = "Precharge speed", .group = "Precharge",
     .about = "The coarse trickler's speed in the precharge, used up to its maximum",
     .type = SETTING_NUMBER, CHARGE_FIELD(precharge_speed_rps), .unit = "rev/s", .min = 0.0f, .max = 100.0f,
     .applies = "now"},
    {.name = "colour_good", .label = "Good throw", .group = "Colours",
     .about = "The LEDs' colour after a throw within the fine stop threshold",
     .type = SETTING_COLOUR, CHARGE_FIELD(neopixel_normal_charge_colour), .applies = "now"},
    {.name = "colour_under", .label = "Under", .group = "Colours",
     .about = "The LEDs' colour after a throw under the target",
     .type = SETTING_COLOUR, CHARGE_FIELD(neopixel_under_charge_colour), .applies = "now"},
    {.name = "colour_over", .label = "Over", .group = "Colours",
     .about = "The LEDs' colour after a throw over the target",
     .type = SETTING_COLOUR, CHARGE_FIELD(neopixel_over_charge_colour), .applies = "now"},
    {.name = "colour_not_ready", .label = "Not ready", .group = "Colours",
     .about = "The LEDs' colour while the unit waits for the cup",
     .type = SETTING_COLOUR, CHARGE_FIELD(neopixel_not_ready_colour), .applies = "now"},
    // ADR-009 step 1: the calibration's cup, items 1 and 2
    {.name = "cup_capacity", .label = "Cup capacity", .group = "Calibration",
     .about = "How much the cup holds, filled and kept by the calibration: no pour of a calibration passes it; null, "
              "never calibrated",
     .type = SETTING_NUMBER, CHARGE_FIELD(cup_capacity), .unit = "weight", .min = 0.0f, .max = 10000.0f,
     .nullable = true, .null_held = -1.0f, .applies = "now"},
    {.name = "cup_weight", .label = "Cup weight", .group = "Calibration",
     .about = "The empty cup's weight, as the calibration found it; null, never calibrated",
     .type = SETTING_NUMBER, CHARGE_FIELD(cup_weight), .unit = "weight", .min = 0.0f, .max = 10000.0f,
     .nullable = true, .null_held = -1.0f, .applies = "now"},
    // ADR-009 step 2, gate 3: the auto-tuner's switch
    {.name = "auto_tune", .label = "Auto-tune", .group = "Tuning",
     .about = "After each window of 25 throws the fine Kp and Kd move toward more throws within the tolerance, then "
              "quicker ones, better ones kept into the powder's profile; a coarse Kp so high its powder overshoots is "
              "brought down once; off, the profile is never changed",
     .type = SETTING_BOOLEAN, CHARGE_FIELD(auto_tune), .applies = "the next throw"},
};


static void *charge_settings_live(int32_t id) {
    (void) id;
    return &charge_mode_config.eeprom_charge_mode_data;
}


static bool charge_settings_is_saved(void) {
    return config_is_saved(EEPROM_CHARGE_MODE_BASE_ADDR, &charge_mode_config.eeprom_charge_mode_data,
                           sizeof(eeprom_charge_mode_data_t));
}


/*
    The decimals are the target's view (charge_mode.cpp): a change of them keeps the target's weight, and is refused
    when that weight cannot be shown at the new decimals (review D2, 2026-11-16). Before, 9.875 at three decimals
    read 98.75 at two, and a start threw that
*/
static const char *charge_settings_check(const void *proposed, int32_t id, const char **field) {
    (void) id;
    const eeprom_charge_mode_data_t *next = proposed;
    decimal_places_t now = charge_mode_config.eeprom_charge_mode_data.decimal_places;
    if (next->decimal_places == now) {
        return NULL;
    }
    float target = charge_mode_target_at(now);
    if (target > 0.0f && !charge_mode_target_fits_at(target, next->decimal_places)) {
        *field = "decimal_places";
        return "the charge's target cannot be shown at those decimals: set one that can be first";
    }
    return NULL;
}


static void charge_settings_apply(const void *before, int32_t id) {
    (void) id;
    const eeprom_charge_mode_data_t *was = before;
    if (was->decimal_places != charge_mode_config.eeprom_charge_mode_data.decimal_places) {
        charge_mode_keep_target(was->decimal_places);
    }
}


static const settings_resource_t charge_settings = {
    .path = "/api/v1/settings/charge",
    .save_path = "/api/v1/settings/charge/save",
    .revert_path = "/api/v1/settings/charge/revert",
    .addr = EEPROM_CHARGE_MODE_BASE_ADDR,
    .label = "Charge",
    .about = "How a throw is poured and judged",
    SETTING_FIELDS(charge_fields),
    .refused_while = SETTINGS_REFUSED_IN_THROW,
    .size = sizeof(eeprom_charge_mode_data_t),
    .live = charge_settings_live,
    .check = charge_settings_check,
    .apply = charge_settings_apply,
    .save = charge_mode_config_save,
    .is_saved = charge_settings_is_saved,
};


bool charge_mode_settings_register(void) {
    return settings_register(&charge_settings);
}
