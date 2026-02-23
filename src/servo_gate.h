#ifndef SERVO_GATE_H_
#define SERVO_GATE_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include "http_rest.h"

#include <stdbool.h>    

#define EEPROM_SERVO_GATE_CONFIG_REV                     1              // 16 byte 

typedef enum {
    GATE_DISABLED = 0,
    GATE_CLOSE,
    GATE_OPEN,
} gate_state_t;
/**
 * Control queue payload:
 * - either a discrete gate_state (existing behavior)
 * - or a direct open ratio command (new behavior)
 *
 * Ratio convention matches existing code:
 *   0.0 = OPEN
 *   1.0 = CLOSED
 */
typedef struct {
    bool is_ratio;       // true = ratio command, false = state command
    gate_state_t state;  // used when is_ratio == false
    float ratio;         // used when is_ratio == true (0.0..1.0)
} servo_gate_cmd_t;

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

void servo_gate_set_state(gate_state_t, bool);

// NEW:
void servo_gate_set_ratio(float ratio, bool block_wait);

#ifdef __cplusplus
}
#endif


#endif  // SERVO_GATE_H_