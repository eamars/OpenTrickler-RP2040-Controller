#include <stdbool.h>
#include <math.h>
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
    .servo_gate_config_rev = EEPROM_SERVO_GATE_CONFIG_REV,
    .servo_gate_enable = false,
    .shutter0_close_duty_cycle = 0.09,
    .shutter0_open_duty_cycle = 0.05,
    .shutter1_close_duty_cycle = 0.05,
    .shutter1_open_duty_cycle = 0.09,
    .servo_gate_dwell_time_ms = 200ul,
};


void _servo_gate_set_current_state() {
    uint32_t shutter0_duty_cycle;
    uint32_t shutter1_duty_cycle;

    switch (servo_gate.gate_state)
    {
    case GATE_CLOSE:
        shutter0_duty_cycle = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.shutter0_close_duty_cycle;
        shutter1_duty_cycle = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.shutter1_close_duty_cycle;
        break;
    case GATE_OPEN:
        shutter0_duty_cycle = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.shutter0_open_duty_cycle;
        shutter1_duty_cycle = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.shutter1_open_duty_cycle;
        break;
    
    default:
        return;
    }

    uint32_t reg_level = (shutter0_duty_cycle) << 16 | shutter1_duty_cycle;

    // Write both levels to the pwm at the same time
    hw_write_masked(
        &pwm_hw->slice[SERVO_PWM_SLICE_NUM].cc,
        reg_level,
        0xffffffff
    );
}


void servo_gate_set_state(gate_state_t state, bool block_for_dwell) {
    if (servo_gate.servo_gate_control_mux) {
        xSemaphoreTake(servo_gate.servo_gate_control_mux, portMAX_DELAY);

        servo_gate.gate_state = state;

        // Apply the state
        _servo_gate_set_current_state();

        if (block_for_dwell) {
            BaseType_t scheduler_state = xTaskGetSchedulerState();
            delay_ms(servo_gate.eeprom_servo_gate_config.servo_gate_dwell_time_ms, scheduler_state);
        }

        xSemaphoreGive(servo_gate.servo_gate_control_mux);
    }
}


bool servo_gate_config_save(void) {
    bool is_ok = eeprom_write(EEPROM_SERVO_GATE_CONFIG_BASE_ADDR, (uint8_t *) &servo_gate.eeprom_servo_gate_config, sizeof(eeprom_servo_gate_config_t));
    return is_ok;
}

bool servo_gate_config_init() {
    bool is_ok = true;

    // Read charge mode config from EEPROM
    memset(&servo_gate, 0x0, sizeof(servo_gate));
    is_ok = eeprom_read(EEPROM_SERVO_GATE_CONFIG_BASE_ADDR, (uint8_t *)&servo_gate.eeprom_servo_gate_config, sizeof(eeprom_servo_gate_config_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_SERVO_GATE_CONFIG_BASE_ADDR);
        return false;
    }

    if (servo_gate.eeprom_servo_gate_config.servo_gate_config_rev != EEPROM_SERVO_GATE_CONFIG_REV) {
        memcpy(&servo_gate.eeprom_servo_gate_config, &default_eeprom_servo_gate_config, sizeof(eeprom_servo_gate_config_t));

        // Write back
        is_ok = servo_gate_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_SERVO_GATE_CONFIG_BASE_ADDR);
            return false;
        }
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

    // Stop early if servo config failed to initialise, or disabled
    if (!is_ok || servo_gate.gate_state == GATE_DISABLED) {
        return false;
    }

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
    servo_gate.servo_gate_control_mux = xSemaphoreCreateMutex();

    servo_gate_set_state(servo_gate.gate_state, false);

    return true;
}


bool http_rest_servo_gate_state(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // g0 (int): gate_state_t

    static char servo_gate_json_buffer[64];

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "g0") == 0) {
            gate_state_t state = (gate_state_t) atoi(values[idx]);
            servo_gate_set_state(state, false);
        }
    }
    
    // Response
    snprintf(servo_gate_json_buffer, 
             sizeof(servo_gate_json_buffer),
             "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
             "{\"g0\":%d}",
             (int) servo_gate.gate_state);

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
    // c5 (int | uint32_t): servo_gate_dwell_time_ms
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
            servo_gate.eeprom_servo_gate_config.servo_gate_dwell_time_ms = strtol(values[idx], NULL, 10);;
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
             "{\"c0\":%s,\"c1\":%0.3f,\"c2\":%0.3f,\"c3\":%0.3f,\"c4\":%0.3f,\"c5\":%ld}",
             http_json_header,
             boolean_to_string(servo_gate.eeprom_servo_gate_config.servo_gate_enable),
             servo_gate.eeprom_servo_gate_config.shutter0_close_duty_cycle,
             servo_gate.eeprom_servo_gate_config.shutter0_open_duty_cycle,
             servo_gate.eeprom_servo_gate_config.shutter1_close_duty_cycle,
             servo_gate.eeprom_servo_gate_config.shutter1_open_duty_cycle,
             servo_gate.eeprom_servo_gate_config.servo_gate_dwell_time_ms);

    size_t data_length = strlen(servo_gate_json_buffer);
    file->data = servo_gate_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
