#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include <math.h>

extern scale_config_t scale_config;
extern charge_mode_config_t charge_mode_config;

static float simulated_scale_weight = 0.0f;

// Scale simulation state machine
typedef enum {
    SIM_SCALE_STATE_EMPTY_CUP = 0,
    SIM_SCALE_STATE_CHARGING,
    SIM_SCALE_STATE_POST_CHARGE_WAIT,
    SIM_SCALE_STATE_CUP_REMOVED,
    SIM_SCALE_STATE_CUP_EMPTY
} simulated_scale_state_t;

static simulated_scale_state_t simulated_scale_state = SIM_SCALE_STATE_EMPTY_CUP;
static float simulated_powder_weight = 0.0f;
static const float simulated_holder_weight = 150.0f; // grains

// Faults the remote control puts in (ADR-013); written by the console's task, read by this one; never stored
static volatile uint32_t simulated_scale_interval_ms = 100;     // sim rate: the time between readings
static volatile bool simulated_scale_bad_pending = false;       // sim bad: the next reading is a frame that did not parse
static volatile TickType_t simulated_scale_quiet_until = 0;     // sim quiet: nothing published before this tick
static volatile bool simulated_scale_quiet = false;
static volatile bool simulated_scale_weight_forced = false;     // sim weight: this is published, whatever the powder does
static volatile float simulated_scale_forced_weight = 0.0f;
static volatile float simulated_scale_flow_share = 1.0f;         // sim flow: the share of the powder that lands
static volatile float simulated_scale_offset = 0.0f;             // sim offset: added to every reading published


bool simulated_scale_set_rate(float per_second) {
    if (!(per_second >= 1.0f && per_second <= 50.0f)) {
        return false;
    }
    simulated_scale_interval_ms = (uint32_t) (1000.0f / per_second + 0.5f);
    return true;
}


float simulated_scale_get_rate(void) {
    return 1000.0f / (float) simulated_scale_interval_ms;
}


void simulated_scale_force_weight(bool forced, float weight) {
    simulated_scale_forced_weight = weight;
    simulated_scale_weight_forced = forced;
}


bool simulated_scale_set_flow(float share) {
    if (!(share >= 0.0f && share <= 1.0f)) {
        return false;
    }
    simulated_scale_flow_share = share;
    return true;
}


void simulated_scale_set_offset(float weight) {
    simulated_scale_offset = weight;
}


void simulated_scale_send_bad(void) {
    simulated_scale_bad_pending = true;
}


void simulated_scale_go_quiet(uint32_t ms) {
    simulated_scale_quiet_until = xTaskGetTickCount() + pdMS_TO_TICKS(ms);
    simulated_scale_quiet = true;
}

static float get_current_trickler_flow_rate(void) {
    // Grains a second: each trickler's forward speed in its own turns, ramps included, by its made-up grains a turn.
    // A tube turned back (the PID loop's back-off) pours nothing
    return fmaxf(0.0f, motor_get_velocity(SELECT_COARSE_TRICKLER_MOTOR)) * SIMULATED_SCALE_COARSE_GR_TURN +
           fmaxf(0.0f, motor_get_velocity(SELECT_FINE_TRICKLER_MOTOR)) * SIMULATED_SCALE_FINE_GR_TURN;
}

void simulated_scale_force_zero(void) {
    simulated_scale_weight = 0.0f;
    simulated_powder_weight = 0.0f;
    simulated_scale_state = SIM_SCALE_STATE_EMPTY_CUP;
}

void simulated_scale_read_loop_task(void *p) {
    const float flow_threshold = 0.02f;     // grains a second: below it nothing is falling
    const uint32_t cup_removed_ms = 3000;

    uint32_t target_reached_ms = 0;
    uint32_t time_in_removed_state_ms = 0;
    const uint32_t step_ms = 2;
    uint32_t since_reading_ms = 0;
    float poured = 0.0f;        // since the last reading: a small move lasts less than a reading's interval

    while (true) {
        // The powder falls every few milliseconds, the scale reads at its interval (sim rate, which may change)
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        const uint32_t interval_ms = simulated_scale_interval_ms;
        float flow_now = get_current_trickler_flow_rate();
        poured += flow_now * simulated_scale_flow_share * (float) step_ms / 1000.0f;
        since_reading_ms += step_ms;
        if (since_reading_ms < interval_ms) {
            continue;
        }
        since_reading_ms = 0;

        if (scale_get_running_driver() == SCALE_DRIVER_SIMULATED) {
            float flow_rate = flow_now;

            if (simulated_scale_state == SIM_SCALE_STATE_EMPTY_CUP || simulated_scale_state == SIM_SCALE_STATE_CUP_EMPTY) {
                if (flow_rate > flow_threshold) {
                    simulated_scale_state = SIM_SCALE_STATE_CHARGING;
                    simulated_powder_weight = 0.0f;
                }
            }

            if (simulated_scale_state == SIM_SCALE_STATE_CHARGING) {
                // The powder is what the tubes turned out, whatever the target: an over shows as one (ADR-009).
                // The cup is lifted as a person lifts it, when the charge asks for it
                simulated_powder_weight += poured;
                simulated_scale_weight = simulated_powder_weight;
                if (charge_mode_config.charge_mode_state == CHARGE_MODE_WAIT_FOR_CUP_REMOVAL) {
                    simulated_scale_state = SIM_SCALE_STATE_POST_CHARGE_WAIT;
                    target_reached_ms = 0;
                }
                // Powder landing jiggles the pan; a pan with nothing landing on it reads steady (ADR-013's pause
                // waits for that)
                else if (flow_rate * simulated_scale_flow_share > flow_threshold) {     // only what lands
                    simulated_scale_weight += ((float) (rand() % 101) - 50.0f) / 1000.0f;
                }
            }
            else if (simulated_scale_state == SIM_SCALE_STATE_POST_CHARGE_WAIT) {
                simulated_scale_weight = simulated_powder_weight;
                target_reached_ms += interval_ms;

                if (target_reached_ms >= cup_removed_ms) {
                    simulated_scale_state = SIM_SCALE_STATE_CUP_REMOVED;
                    time_in_removed_state_ms = 0;
                    simulated_scale_weight = -simulated_holder_weight;
                    target_reached_ms = 0;
                }
            }
            else if (simulated_scale_state == SIM_SCALE_STATE_CUP_REMOVED) {
                simulated_scale_weight = -simulated_holder_weight;
                time_in_removed_state_ms += interval_ms;
                if (time_in_removed_state_ms >= cup_removed_ms) {
                    simulated_scale_state = SIM_SCALE_STATE_CUP_EMPTY;
                    simulated_scale_weight = 0.0f;
                }
            }
            else if (simulated_scale_state == SIM_SCALE_STATE_EMPTY_CUP) {
                simulated_scale_weight = 0.0f;
            }

            if (simulated_scale_state == SIM_SCALE_STATE_CHARGING && simulated_scale_weight < 0.0f) {
                simulated_scale_weight = 0.0f;
            }

            if (simulated_scale_state == SIM_SCALE_STATE_EMPTY_CUP || simulated_scale_state == SIM_SCALE_STATE_CUP_EMPTY) {
                simulated_scale_weight = 0.0f;
            }

            // sim quiet: the powder keeps falling, but nothing is published until the time is up
            if (simulated_scale_quiet && (int32_t) (xTaskGetTickCount() - simulated_scale_quiet_until) >= 0) {
                simulated_scale_quiet = false;
            }

            if (!simulated_scale_quiet) {
                // Publish measurement. sim bad: a frame that did not parse, published as the drivers do today
                float published = simulated_scale_weight_forced ? simulated_scale_forced_weight :
                                  roundf((simulated_scale_weight + simulated_scale_offset) / SIMULATED_SCALE_DIGIT_GR) *
                                  SIMULATED_SCALE_DIGIT_GR;
                if (simulated_scale_bad_pending) {
                    simulated_scale_bad_pending = false;
                    published = nanf("");
                }
                scale_publish_measurement(published);
            }
        }
        poured = 0.0f;
    }
}

scale_handle_t simulated_scale_handle = {
    .read_loop_task = simulated_scale_read_loop_task,
    .force_zero = simulated_scale_force_zero,
};
