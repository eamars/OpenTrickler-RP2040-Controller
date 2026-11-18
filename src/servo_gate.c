#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "configuration.h"
#include "eeprom.h"
#include "common.h"
#include "servo_gate.h"
#include "settings.h"

// Attributes
servo_gate_t servo_gate;

// Const settings
const float _servo_pwm_freq = 50.0;
const uint16_t _pwm_full_scale_level = 65535;


const eeprom_servo_gate_config_t default_eeprom_servo_gate_config = {
    .servo_gate_config_rev = 0,
    .servo_gate_enable = false,
    .shutter0_close_duty_cycle = 0.09f,
    .shutter0_open_duty_cycle = 0.05f,
    .shutter1_close_duty_cycle = 0.05f,
    .shutter1_open_duty_cycle = 0.09f,
    .shutter_open_speed_pct_s = 5.0f,
    .shutter_close_speed_pct_s = 3.0f,
};


const char * _gate_state_string[] = {
    "Disabled",
    "Close",
    "Open"
};
const char * gate_state_to_string(gate_state_t state) {
    return _gate_state_string[state];
}

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static void inline _set_duty_cycle(uint16_t shutter0_duty_cycle, uint16_t shutter1_duty_cycle) {
    uint32_t reg_level = ((uint32_t) shutter0_duty_cycle) << 16 | shutter1_duty_cycle;

    // Write both levels to the pwm at the same time
    hw_write_masked(
        &pwm_hw->slice[SERVO_PWM_SLICE_NUM].cc,
        reg_level,
        0xffffffff
    );
}


static void _servo_gate_set_current_state(float open_ratio) {
    uint16_t shutter0_duty_cycle;
    uint16_t shutter1_duty_cycle;

    float shutter0_range = servo_gate.eeprom_servo_gate_config.shutter0_close_duty_cycle - servo_gate.eeprom_servo_gate_config.shutter0_open_duty_cycle;
    float shutter1_range = servo_gate.eeprom_servo_gate_config.shutter1_close_duty_cycle - servo_gate.eeprom_servo_gate_config.shutter1_open_duty_cycle;

    shutter0_duty_cycle = _pwm_full_scale_level * (servo_gate.eeprom_servo_gate_config.shutter0_open_duty_cycle + shutter0_range * open_ratio);
    shutter1_duty_cycle = _pwm_full_scale_level * (servo_gate.eeprom_servo_gate_config.shutter1_open_duty_cycle + shutter1_range * open_ratio);

    _set_duty_cycle(shutter0_duty_cycle, shutter1_duty_cycle);
}


void servo_gate_set_ratio(gate_ratio_t ratio, bool block_wait) {
    float r = (ratio == SERVO_GATE_RATIO_DISABLED) ? SERVO_GATE_RATIO_DISABLED : clamp01(ratio);
    
    xSemaphoreTake(servo_gate.move_ready_semphore, 0); 
    
    xQueueOverwrite(servo_gate.control_queue, &r);

    if (block_wait) {
        xSemaphoreTake(servo_gate.move_ready_semphore, portMAX_DELAY);
    }
}

void servo_gate_control_task(void *p) {
    (void)p;

    const float UNKNOWN_RATIO = -2.0f;
    float prev_open_ratio = UNKNOWN_RATIO;

    while (true) {
        gate_ratio_t new_ratio;
        xQueueReceive(servo_gate.control_queue, &new_ratio, portMAX_DELAY);

        // --- DISABLE ---
        if (new_ratio == SERVO_GATE_RATIO_DISABLED) {
            servo_gate.gate_state = GATE_DISABLED;

            // Do NOT modify prev_open_ratio
            xSemaphoreGive(servo_gate.move_ready_semphore);
            continue;
        }

        // Clamp to valid range
        float new_open_ratio = clamp01((float)new_ratio);

        // First valid move: set immediately
        if (prev_open_ratio == UNKNOWN_RATIO) {
            _servo_gate_set_current_state(new_open_ratio);
        } else {
            // Skip ramp if no change
            if (fabsf(new_open_ratio - prev_open_ratio) > 0.0001f) {

                float delta = new_open_ratio - prev_open_ratio;

                // 0 = open, 1 = closed
                float speed = (delta < 0.0f)
                    ? servo_gate.eeprom_servo_gate_config.shutter_open_speed_pct_s
                    : servo_gate.eeprom_servo_gate_config.shutter_close_speed_pct_s;

                if (speed < 0.0001f) speed = 0.0001f;

                uint32_t ramp_time_us = (uint32_t)(fabsf(delta / speed) * 1e6f);

                if (ramp_time_us < 1000) {
                    _servo_gate_set_current_state(new_open_ratio);
                } else {
                    // The gate moves toward its end a tick at a time, and the task sleeps between (ADR-012; audit
                    // G1, G2). The time gone is a difference of tick counts, right across their wrap
                    TickType_t start_tick = xTaskGetTickCount();
                    TickType_t ramp_ticks = pdMS_TO_TICKS(ramp_time_us / 1000);

                    while (true) {
                        TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
                        if (elapsed_ticks >= ramp_ticks) break;

                        float percentage = elapsed_ticks / (float)ramp_ticks;
                        float current_ratio = prev_open_ratio + delta * percentage;

                        _servo_gate_set_current_state(current_ratio);
                        vTaskDelay(1);
                    }

                    _servo_gate_set_current_state(new_open_ratio);
                }
            }
            else {
                // The same place again: set with the shutters' ends as they are now, so an end set at the screen
                // shows at once (ADR-010, gate 6)
                _servo_gate_set_current_state(new_open_ratio);
            }
        }

        // Update discrete state for reporting/UI
        if (new_open_ratio <= 0.0001f) {
            servo_gate.gate_state = GATE_OPEN;
        } else if (new_open_ratio >= 0.9999f) {
            servo_gate.gate_state = GATE_CLOSE;
        }

        // Save last ratio
        prev_open_ratio = new_open_ratio;
        servo_gate.gate_ratio = (gate_ratio_t)new_open_ratio;

        // Signal completion
        xSemaphoreGive(servo_gate.move_ready_semphore);
    }
}


bool servo_gate_config_save(void) {
    bool is_ok = save_config(EEPROM_SERVO_GATE_CONFIG_BASE_ADDR, &servo_gate.eeprom_servo_gate_config, sizeof(servo_gate.eeprom_servo_gate_config));
    return is_ok;
}

/*
    The servo gate's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/settings/gate)
*/
#define GATE_FIELD(member) SETTING_AT(eeprom_servo_gate_config_t, member)

static const setting_field_t gate_fields[] = {
    {.name = "enable", .label = "Fitted", .short_label = "Servo gate",
     .about = "The unit has a servo gate, and uses it",
     .type = SETTING_BOOLEAN, GATE_FIELD(servo_gate_enable), .applies = "now"},
    {.name = "shutter0_close_duty", .label = "Shutter 0 closed", .group = "Shutters",
     .about = "Shutter 0's servo pulse when closed, as a share of the PWM's period (a servo's pulse at 50 Hz)",
     .type = SETTING_NUMBER, GATE_FIELD(shutter0_close_duty_cycle), .unit = "of the PWM's period", .min = 0.02f,
     .max = 0.13f, .edit_step = 0.001f, .applies = "the next move"},
    {.name = "shutter0_open_duty", .label = "Shutter 0 open", .group = "Shutters",
     .about = "Shutter 0's servo pulse when open",
     .type = SETTING_NUMBER, GATE_FIELD(shutter0_open_duty_cycle), .unit = "of the PWM's period", .min = 0.02f,
     .max = 0.13f, .edit_step = 0.001f, .applies = "the next move"},
    {.name = "shutter1_close_duty", .label = "Shutter 1 closed", .group = "Shutters",
     .about = "Shutter 1's servo pulse when closed",
     .type = SETTING_NUMBER, GATE_FIELD(shutter1_close_duty_cycle), .unit = "of the PWM's period", .min = 0.02f,
     .max = 0.13f, .edit_step = 0.001f, .applies = "the next move"},
    {.name = "shutter1_open_duty", .label = "Shutter 1 open", .group = "Shutters",
     .about = "Shutter 1's servo pulse when open",
     .type = SETTING_NUMBER, GATE_FIELD(shutter1_open_duty_cycle), .unit = "of the PWM's period", .min = 0.02f,
     .max = 0.13f, .edit_step = 0.001f, .applies = "the next move"},
    {.name = "close_speed_per_s", .label = "Closing speed", .group = "Speed",
     .about = "How fast the gate closes: 1 is its whole travel in a second",
     .type = SETTING_NUMBER, GATE_FIELD(shutter_close_speed_pct_s), .unit = "full travels a second", .min = 0.01f,
     .max = 100.0f, .applies = "the next move"},
    {.name = "open_speed_per_s", .label = "Opening speed", .group = "Speed",
     .about = "How fast the gate opens",
     .type = SETTING_NUMBER, GATE_FIELD(shutter_open_speed_pct_s), .unit = "full travels a second", .min = 0.01f,
     .max = 100.0f, .applies = "the next move"},
};


static void *gate_settings_live(int32_t id) {
    (void) id;
    return &servo_gate.eeprom_servo_gate_config;
}


static void gate_settings_apply(const void *before, int32_t id) {
    (void) id;
    const eeprom_servo_gate_config_t *was = before;
    // Turned on or off, the gate's state is as a start leaves it: the servo is not moved
    if (servo_gate.eeprom_servo_gate_config.servo_gate_enable != was->servo_gate_enable) {
        servo_gate.gate_state = servo_gate.eeprom_servo_gate_config.servo_gate_enable ? GATE_OPEN : GATE_DISABLED;
    }
}


static bool gate_settings_is_saved(void) {
    return config_is_saved(EEPROM_SERVO_GATE_CONFIG_BASE_ADDR, &servo_gate.eeprom_servo_gate_config,
                           sizeof(servo_gate.eeprom_servo_gate_config));
}


static const settings_resource_t gate_settings = {
    .path = "/api/v1/settings/gate",
    .save_path = "/api/v1/settings/gate/save",
    .label = "Servo gate",
    .about = "The gate between the tricklers and the cup",
    SETTING_FIELDS(gate_fields),
    .refused_while = SETTINGS_REFUSED_IN_THROW,
    .size = sizeof(eeprom_servo_gate_config_t),
    .live = gate_settings_live,
    .apply = gate_settings_apply,
    .save = servo_gate_config_save,
    .is_saved = gate_settings_is_saved,
};


bool servo_gate_config_init() {
    bool is_ok = true;

    // Read charge mode config from EEPROM
    memset(&servo_gate, 0x0, sizeof(servo_gate));
    is_ok = load_config(EEPROM_SERVO_GATE_CONFIG_BASE_ADDR, &servo_gate.eeprom_servo_gate_config, &default_eeprom_servo_gate_config, sizeof(servo_gate.eeprom_servo_gate_config), EEPROM_SERVO_GATE_CONFIG_REV);
    if (!is_ok) {
        printf("Unable to read servo gate configuration\n");
        return false;
    }

    // Register to eeprom save all
    eeprom_register_handler(servo_gate_config_save);
    settings_register(&gate_settings);

    // Initialize settings
    if (servo_gate.eeprom_servo_gate_config.servo_gate_enable) {
        servo_gate.gate_state = GATE_OPEN;
    }
    else {
        servo_gate.gate_state = GATE_DISABLED;
    }

    return is_ok;
}


bool servo_gate_init() {
    bool is_ok = true;

    is_ok = servo_gate_config_init();

    // Initialize pins
    gpio_set_function(SERVO0_PWM_PIN, GPIO_FUNC_PWM);
    gpio_set_function(SERVO1_PWM_PIN, GPIO_FUNC_PWM);

    pwm_config cfg = pwm_get_default_config();

    // Set to 50hz frequency
    uint32_t sys_freq = clock_get_hz(clk_sys);
    float divider = ceil(sys_freq / (4096 * _servo_pwm_freq)) / 16.0f;
    uint16_t wrap = sys_freq / divider / _servo_pwm_freq - 1;

    pwm_config_set_clkdiv(&cfg, divider);
    pwm_config_set_wrap(&cfg, wrap);

    pwm_init(pwm_gpio_to_slice_num(SERVO0_PWM_PIN), &cfg, true);
    pwm_init(pwm_gpio_to_slice_num(SERVO1_PWM_PIN), &cfg, true);

    // Start the RTOS task and queue
    servo_gate.control_queue = xQueueCreate(1, sizeof(gate_ratio_t));
    servo_gate.move_ready_semphore = xSemaphoreCreateBinary();

    xTaskCreate(
        servo_gate_control_task,
        "servo_gate_controller",
        configMINIMAL_STACK_SIZE,
        NULL,
        8,
        &servo_gate.control_task_handler
    );

    // No, we don't set the servo gate state

    return is_ok;
}


