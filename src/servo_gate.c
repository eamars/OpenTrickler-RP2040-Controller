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
                    uint32_t start_time = time_us_32();
                    uint32_t stop_time  = start_time + ramp_time_us;

                    while (true) {
                        uint32_t current_time = time_us_32();
                        if (current_time > stop_time) break;

                        float percentage = (current_time - start_time) / (float)ramp_time_us;
                        float current_ratio = prev_open_ratio + delta * percentage;

                        _servo_gate_set_current_state(current_ratio);
                    }

                    _servo_gate_set_current_state(new_open_ratio);
                }
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


bool http_rest_servo_gate_state(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char servo_gate_json_buffer[96];

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "g0") == 0) {
            gate_state_t state = (gate_state_t)atoi(values[idx]);

            float ratio = SERVO_GATE_RATIO_DISABLED;
            switch (state) {
                case GATE_OPEN:   ratio = SERVO_GATE_RATIO_OPEN; break;
                case GATE_CLOSE:  ratio = SERVO_GATE_RATIO_CLOSED; break;
                case GATE_DISABLED:
                default:          ratio = SERVO_GATE_RATIO_DISABLED; break;
            }

            servo_gate_set_ratio(ratio, false);
        }
        else if (strcmp(params[idx], "r0") == 0) {
            float ratio = strtof(values[idx], NULL);
            servo_gate_set_ratio(ratio, false);
        }
    }

    snprintf(servo_gate_json_buffer,
             sizeof(servo_gate_json_buffer),
             "%s"
             "{\"g0\":%d}",
             http_json_header,
             (int)servo_gate.gate_state);

    size_t data_length = strlen(servo_gate_json_buffer);
    file->data = servo_gate_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_servo_gate_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // c0 (bool): servo_gate_enable
    // c1 (float): shutter0_close_duty_cycle
    // c2 (float): shutter0_open_duty_cycle
    // c3 (float): shutter1_close_duty_cycle
    // c4 (float): shutter1_open_duty_cycle
    // c5 (float): shutter_close_speed_pct_s
    // c6 (float): shutter_open_speed_pct_s
    // ee (bool): save_to_eeprom

    static char servo_gate_json_buffer[256];
    bool save_to_eeprom = false;


    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "c0") == 0) {
            bool enable = string_to_boolean(values[idx]);
            servo_gate.eeprom_servo_gate_config.servo_gate_enable = enable;
        }
        else if (strcmp(params[idx], "c1") == 0) {
            servo_gate.eeprom_servo_gate_config.shutter0_close_duty_cycle = strtof(values[idx], NULL);;
        }
        else if (strcmp(params[idx], "c2") == 0) {
            servo_gate.eeprom_servo_gate_config.shutter0_open_duty_cycle = strtof(values[idx], NULL);;
        }
        else if (strcmp(params[idx], "c3") == 0) {
            servo_gate.eeprom_servo_gate_config.shutter1_close_duty_cycle = strtof(values[idx], NULL);;
        }
        else if (strcmp(params[idx], "c4") == 0) {
            servo_gate.eeprom_servo_gate_config.shutter1_open_duty_cycle = strtof(values[idx], NULL);;
        }
        else if (strcmp(params[idx], "c5") == 0) {
            servo_gate.eeprom_servo_gate_config.shutter_close_speed_pct_s = strtof(values[idx], NULL);;
        }
        else if (strcmp(params[idx], "c6") == 0) {
            servo_gate.eeprom_servo_gate_config.shutter_open_speed_pct_s = strtof(values[idx], NULL);;
        }
        else if (strcmp(params[idx], "ee") == 0) {
            save_to_eeprom = string_to_boolean(values[idx]);
        }
    }

    // Perform action
    if (save_to_eeprom) {
        servo_gate_config_save();  // Note: this will save settings for both
    }
    
    // Response
    snprintf(servo_gate_json_buffer, 
             sizeof(servo_gate_json_buffer),
             "%s"
             "{\"c0\":%s,\"c1\":%0.3f,\"c2\":%0.3f,\"c3\":%0.3f,\"c4\":%0.3f,\"c5\":%0.3f,\"c6\":%0.3f}",
             http_json_header,
             boolean_to_string(servo_gate.eeprom_servo_gate_config.servo_gate_enable),
             servo_gate.eeprom_servo_gate_config.shutter0_close_duty_cycle,
             servo_gate.eeprom_servo_gate_config.shutter0_open_duty_cycle,
             servo_gate.eeprom_servo_gate_config.shutter1_close_duty_cycle,
             servo_gate.eeprom_servo_gate_config.shutter1_open_duty_cycle,
             servo_gate.eeprom_servo_gate_config.shutter_close_speed_pct_s,
             servo_gate.eeprom_servo_gate_config.shutter_open_speed_pct_s);

    size_t data_length = strlen(servo_gate_json_buffer);
    file->data = servo_gate_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
