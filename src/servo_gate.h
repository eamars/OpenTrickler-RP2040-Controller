#ifndef SERVO_GATE_H_
#define SERVO_GATE_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include "http_rest.h"

#define EEPROM_SERVO_GATE_CONFIG_REV                     1              // 16 byte 

typedef enum {
    GATE_DISABLED = 0,
    GATE_CLOSE,
    GATE_OPEN,
} gate_state_t;


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

#ifdef __cplusplus
}
#endif


#endif  // SERVO_GATE_H_