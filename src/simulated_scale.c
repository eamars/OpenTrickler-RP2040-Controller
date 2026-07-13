#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>
#include <math.h>

extern scale_config_t scale_config;
extern motor_config_t coarse_trickler_motor_config;
extern motor_config_t fine_trickler_motor_config;
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

static float get_current_trickler_flow_rate(void) {
    // Use absolute motor speed as proxy for flow.
    float coarse_rate = fabsf(coarse_trickler_motor_config.prev_velocity);
    float fine_rate = fabsf(fine_trickler_motor_config.prev_velocity);

    // Scale factors to convert rps to grain/s (tune as needed).
    const float coarse_factor = 5.0f;  // 5 grains/s per rps (approx)
    const float fine_factor = 1.0f;    // 1 grain/s per rps (approx)

    return coarse_rate * coarse_factor + fine_rate * fine_factor;
}

void simulated_scale_force_zero(void) {
    simulated_scale_weight = 0.0f;
    simulated_powder_weight = 0.0f;
    simulated_scale_state = SIM_SCALE_STATE_EMPTY_CUP;
}

void simulated_scale_read_loop_task(void *p) {
    const TickType_t interval = pdMS_TO_TICKS(100);
    const float flow_threshold = 0.2f; // rps-adjusted threshold
    const uint32_t target_hold_ms = 3000;
    const uint32_t cup_removed_ms = 3000;

    uint32_t target_reached_ms = 0;
    uint32_t time_in_removed_state_ms = 0;

    while (true) {
        if (scale_config.persistent_config.scale_driver == SCALE_DRIVER_SIMULATED) {
            float flow_rate = get_current_trickler_flow_rate();

            if (simulated_scale_state == SIM_SCALE_STATE_EMPTY_CUP || simulated_scale_state == SIM_SCALE_STATE_CUP_EMPTY) {
                if (flow_rate > flow_threshold) {
                    simulated_scale_state = SIM_SCALE_STATE_CHARGING;
                    simulated_powder_weight = 0.0f;
                }
            }

            if (simulated_scale_state == SIM_SCALE_STATE_CHARGING) {
                float target_weight = charge_mode_config.target_charge_weight;
                float add_amount = flow_rate * 0.1f; // flow_rate is grains/sec, interval=0.1s
                const float target_tolerance = 0.1f; // snap to target when within 0.1 grains
                bool reached_target = false;

                if (target_weight > 0.0f) {
                    if (simulated_powder_weight >= target_weight - target_tolerance) {
                        simulated_powder_weight = target_weight;
                        reached_target = true;
                    } else {
                        simulated_powder_weight += add_amount;
                        if (simulated_powder_weight > target_weight) {
                            simulated_powder_weight = target_weight;
                        }
                        reached_target = (simulated_powder_weight >= target_weight);
                    }

                    if (reached_target) {
                        target_reached_ms += 100;
                        if (target_reached_ms >= target_hold_ms) {
                            simulated_scale_state = SIM_SCALE_STATE_POST_CHARGE_WAIT;
                            target_reached_ms = 0;
                        }
                    } else {
                        target_reached_ms = 0;
                    }
                } else {
                    // No defined target: use scaled flow and never auto-drop.
                    simulated_powder_weight += add_amount;
                    reached_target = false;
                    target_reached_ms = 0;
                }

                simulated_scale_weight = simulated_powder_weight;

                // No noise once target is reached.
                if (target_weight > 0.0f && reached_target) {
                    // keep exact value
                } else if (target_weight <= 0.0f) {
                    float noise = ((float)(rand() % 101) - 50.0f) / 1000.0f;
                    simulated_scale_weight += noise;
                } else {
                    float noise = ((float)(rand() % 101) - 50.0f) / 1000.0f;
                    simulated_scale_weight += noise;
                }
            }
            else if (simulated_scale_state == SIM_SCALE_STATE_POST_CHARGE_WAIT) {
                simulated_scale_weight = simulated_powder_weight;
                target_reached_ms += 100;

                if (target_reached_ms >= cup_removed_ms) {
                    simulated_scale_state = SIM_SCALE_STATE_CUP_REMOVED;
                    time_in_removed_state_ms = 0;
                    simulated_scale_weight = -simulated_holder_weight;
                    target_reached_ms = 0;
                }
            }
            else if (simulated_scale_state == SIM_SCALE_STATE_CUP_REMOVED) {
                simulated_scale_weight = -simulated_holder_weight;
                time_in_removed_state_ms += 100;
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

            // Publish measurement
            scale_config.current_scale_measurement = simulated_scale_weight;
            if (scale_config.scale_measurement_ready) {
                xSemaphoreGive(scale_config.scale_measurement_ready);
            }
        }

        vTaskDelay(interval);
    }
}

scale_handle_t simulated_scale_handle = {
    .read_loop_task = simulated_scale_read_loop_task,
    .force_zero = simulated_scale_force_zero,
};
