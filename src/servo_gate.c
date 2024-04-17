#include <stdbool.h>
#include <math.h>
#include "servo_gate.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "configuration.h"
#include "eeprom.h"

// Attributes
servo_gate_t servo_gate;

// Const settings
const float _servo_pwm_freq = 50.0;
const uint16_t _pwm_full_scale_level = 65535;


const eeprom_servo_gate_config_t default_eeprom_servo_gate_config = {
    .servo_gate_config_rev = EEPROM_SERVO_GATE_CONFIG_REV,
    .servo_gate_enable = true,
    .gate_close_duty_cycle = 0.05,
    .gate_open_duty_cycle = 0.1,
};


void set_servo_gate_state(gate_state_t state) {
    // pwm_set_chan_level(pwm_gpio_to_slice_num(gpio), pwm_gpio_to_channel(gpio), level);
    uint32_t servo0_level;
    uint32_t servo1_level;
    switch (state)
    {
    case GATE_CLOSE:
        servo0_level = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.gate_open_duty_cycle;
        servo1_level = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.gate_close_duty_cycle;
        break;
    case GATE_OPEN:
        servo0_level = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.gate_close_duty_cycle;
        servo1_level = _pwm_full_scale_level * servo_gate.eeprom_servo_gate_config.gate_open_duty_cycle;
        break;
    
    default:
        return;
    }

    uint32_t reg_level = (servo1_level) << 16 | servo0_level;

    // Write both levels to the pwm at the same time
    hw_write_masked(
        &pwm_hw->slice[SERVO_PWM_SLICE_NUM].cc,
        reg_level,
        0xffffffff
    );
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
        servo_gate.gate_state = GATE_CLOSE;
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

    set_servo_gate_state(servo_gate.gate_state);

    return true;
}

bool http_rest_servo_gate_state(struct fs_file *file, int num_params, char *params[], char *values[]) {
    // Mappings
    // g0 (int): gate_state_t
    
    static char servo_gate_json_buffer[256];

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "g0") == 0) {
            servo_gate.gate_state = (gate_state_t) atoi(values[idx]);
            set_servo_gate_state(servo_gate.gate_state);
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