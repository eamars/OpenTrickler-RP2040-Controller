/*
 * The display's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/settings/display). In C beside
 * mini_12864_module.cpp: the table is written with C's designated initializers.
 */
#include <math.h>
#include <stddef.h>

#include "common.h"
#include "eeprom.h"
#include "mini_12864_module.h"
#include "settings.h"

extern mini_12864_module_config_t mini_12864_module_config;


static const setting_choice_t display_charge_view_choices[] = {
    {.held = DISPLAY_CHARGE_VIEW_WEIGHT, .text = "weight", .label = "Weight"},
    {.held = DISPLAY_CHARGE_VIEW_CHART, .text = "chart", .label = "Chart"},
};


// 90 and 270 turn a 128 by 64 screen on its side: not offered
static const setting_choice_t display_rotation_choices[] = {
    {.held = DISPLAY_ROTATION_0, .number = 0, .label = "As mounted"},
    {.held = DISPLAY_ROTATION_180, .number = 180, .label = "Upside down"},
};


#define DISPLAY_FIELD(member) SETTING_AT(mini_12864_module_config_t, member)

static const setting_field_t display_fields[] = {
    {.name = "invert_encoder", .label = "Invert the encoder", .short_label = "Invert knob",
     .about = "The encoder's knob turns the other way",
     .type = SETTING_BOOLEAN, DISPLAY_FIELD(inverted_encoder_direction), .applies = "now"},
    {.name = "rotation", .label = "Rotation",
     .about = "The screen's turn, in degrees",
     .type = SETTING_CHOICE, DISPLAY_FIELD(display_rotation), SETTING_CHOICES(display_rotation_choices),
     .unit = "degrees", .applies = "at_restart"},
    {.name = "invert_display", .label = "Invert the screen", .short_label = "Invert screen",
     .about = "The screen shown the other way: dark on a lit glass",
     .type = SETTING_BOOLEAN, DISPLAY_FIELD(inverted_display), .applies = "now"},
    {.name = "charge_view", .label = "Charge view",
     .about = "What the charge's screen leads with: the weight, or a chart of the throw",
     .type = SETTING_CHOICE, DISPLAY_FIELD(charge_view), SETTING_CHOICES(display_charge_view_choices),
     .applies = "now"},
};


static void *display_settings_live(int32_t id) {
    (void) id;
    return &mini_12864_module_config;
}


static bool display_settings_is_saved(void) {
    return config_is_saved(EEPROM_MINI_12864_CONFIG_BASE_ADDR, &mini_12864_module_config,
                           sizeof(mini_12864_module_config_t));
}


static const settings_resource_t display_settings = {
    .path = "/api/v1/settings/display",
    .save_path = "/api/v1/settings/display/save",
    .label = "Display",
    .about = "The display and its encoder",
    SETTING_FIELDS(display_fields),
    .refused_while = SETTINGS_REFUSED_NEVER,
    .size = sizeof(mini_12864_module_config_t),
    .live = display_settings_live,
    .save = mini_12864_module_config_save,
    .is_saved = display_settings_is_saved,
};


bool mini_12864_settings_register(void) {
    return settings_register(&display_settings);
}
