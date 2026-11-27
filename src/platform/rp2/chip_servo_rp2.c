/*
 * Two servos on the RP chips (chip_servo.h): one PWM slice, its channels A and B, its period near 65,536 counts at
 * 50 Hz. Both widths are one register, written at once.
 */
#include "chip_servo.h"

#include <math.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

static const float chip_servo_hz = 50.0f;
static uint chip_servo_slice = 0;
static bool chip_servo_pin0_is_a = true;


bool chip_servo_open(unsigned int pin0, unsigned int pin1) {
    uint slice = pwm_gpio_to_slice_num(pin0);
    if (pwm_gpio_to_slice_num(pin1) != slice || pwm_gpio_to_channel(pin0) == pwm_gpio_to_channel(pin1)) {
        return false;
    }
    gpio_set_function(pin0, GPIO_FUNC_PWM);
    gpio_set_function(pin1, GPIO_FUNC_PWM);

    pwm_config cfg = pwm_get_default_config();
    // 50 Hz, the divider the smallest in sixteenths that keeps the period within 16 bits
    uint32_t sys_freq = clock_get_hz(clk_sys);
    float divider = ceil(sys_freq / (4096 * chip_servo_hz)) / 16.0f;
    uint16_t wrap = sys_freq / divider / chip_servo_hz - 1;
    pwm_config_set_clkdiv(&cfg, divider);
    pwm_config_set_wrap(&cfg, wrap);
    pwm_init(slice, &cfg, true);

    chip_servo_slice = slice;
    chip_servo_pin0_is_a = pwm_gpio_to_channel(pin0) == PWM_CHAN_A;
    return true;
}


void chip_servo_set(uint16_t width0, uint16_t width1) {
    uint16_t a = chip_servo_pin0_is_a ? width0 : width1;
    uint16_t b = chip_servo_pin0_is_a ? width1 : width0;
    hw_write_masked(&pwm_hw->slice[chip_servo_slice].cc, ((uint32_t) b << 16) | a, 0xffffffff);
}
