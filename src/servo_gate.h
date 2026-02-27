#ifndef SERVO_GATE_H_
#define SERVO_GATE_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include "http_rest.h"

#include <stdbool.h>    

#define EEPROM_SERVO_GATE_CONFIG_REV                     1              // 16 byte 

#define SERVO_GATE_RATIO_OPEN       (0.0f)
#define SERVO_GATE_RATIO_CLOSED     (1.0f)
#define SERVO_GATE_RATIO_DISABLED   (-1.0f)

typedef float gate_ratio_t;

typedef enum {
    GATE_DISABLED = 0,
    GATE_CLOSE,
    GATE_OPEN,
} gate_state_t;

/**
 * Control queue payload (ratio-only)
 *
 * Ratio convention:
 *   0.0  = OPEN
 *   1.0  = CLOSED
 *  -1.0  = DISABLED
 *
 * Any value between 0.0 and 1.0 is proportional.
 */
 
/*typedef struct {
    float ratio;
    bool block_wait;
} servo_gate_cmd_t;
 */

typedef struct {
    uint16_t servo_gate_config_rev;
    bool servo_gate_enable;
    float shutter0_close_duty_cycle;
    float shutter0_open_duty_cycle;
    float shutter1_close_duty_cycle;
    float shutter1_open_duty_cycle;
    float shutter_close_speed_pct_s;  // Per shutter speed (percentage per second)
    float shutter_open_speed_pct_s;  // Per shutter speed (percentage per second)
} eeprom_servo_gate_config_t;


typedef struct {
    eeprom_servo_gate_config_t eeprom_servo_gate_config;
    gate_state_t gate_state;
    gate_ratio_t gate_ratio;  // 0.0 = OPEN, 1.0 = CLOSED, -1.0 = DISABLED
    // RTOS control
    TaskHandle_t control_task_handler;
    QueueHandle_t control_queue;
    SemaphoreHandle_t move_ready_semphore;
} servo_gate_t;


#ifdef __cplusplus
extern "C" {
#endif

bool servo_gate_init(void);
bool servo_gate_config_save(void);
bool http_rest_servo_gate_state(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_servo_gate_config(struct fs_file *file, int num_params, char *params[], char *values[]);
const char * gate_state_to_string(gate_state_t);


// NEW:
void servo_gate_set_ratio(float ratio, bool block_wait);

#ifdef __cplusplus
}
#endif


#endif  // SERVO_GATE_H_