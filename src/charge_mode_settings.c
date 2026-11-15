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
    {.held = DP_2, .number = 2, .label = "2 decimals"},
    {.held = DP_3, .number = 3, .label = "3 decimals"},
};


#define CHARGE_FIELD(member) SETTING_AT(eeprom_charge_mode_data_t, member)

static const setting_field_t charge_fields[] = {
    {.name = "coarse_stop_threshold", .label = "Coarse stop threshold", .group = "Thresholds",
     .about = "The coarse trickler stops this far below the target",
     .type = SETTING_NUMBER, CHARGE_FIELD(coarse_stop_threshold), .unit = "weight", .min = 0.0f, .max = 100.0f,
     .applies = "now"},
    {.name = "fine_stop_threshold", .label = "Fine stop threshold", .group = "Thresholds",
     .about = "The fine trickler stops this far below the target; a throw further off than it is over or under",
     .type = SETTING_NUMBER, CHARGE_FIELD(fine_stop_threshold), .unit = "weight", .min = 0.0f, .max = 10.0f,
     .applies = "now"},
    {.name = "steady_sd_margin", .label = "Steady margin (SD)", .group = "Thresholds",
     .about = "The readings are steady when their standard deviation is below it: for the zero, the cup's removal "
              "and the resume after a pause",
     .type = SETTING_NUMBER, CHARGE_FIELD(set_point_sd_margin), .unit = "weight", .min = 0.0f, .max = 1.0f,
     .applies = "now"},
    {.name = "zero_mean_margin", .label = "Zero margin (mean)", .group = "Thresholds",
     .about = "The scale reads zero when the mean of its readings is this near it",
     .type = SETTING_NUMBER, CHARGE_FIELD(set_point_mean_margin), .unit = "weight", .min = 0.0f, .max = 1.0f,
     .applies = "now"},
    {.name = "decimal_places", .label = "Decimal places", .group = "Display",
     .about = "Decimals of the weight and of the target on the screen",
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
};


static void *charge_settings_live(int32_t id) {
    (void) id;
    return &charge_mode_config.eeprom_charge_mode_data;
}


static bool charge_settings_is_saved(void) {
    return config_is_saved(EEPROM_CHARGE_MODE_BASE_ADDR, &charge_mode_config.eeprom_charge_mode_data,
                           sizeof(eeprom_charge_mode_data_t));
}


static const settings_resource_t charge_settings = {
    .path = "/api/v1/settings/charge",
    .save_path = "/api/v1/settings/charge/save",
    .label = "Charge",
    .about = "How a throw is poured and judged",
    SETTING_FIELDS(charge_fields),
    .refused_while = SETTINGS_REFUSED_IN_THROW,
    .size = sizeof(eeprom_charge_mode_data_t),
    .live = charge_settings_live,
    .save = charge_mode_config_save,
    .is_saved = charge_settings_is_saved,
};


bool charge_mode_settings_register(void) {
    return settings_register(&charge_settings);
}
