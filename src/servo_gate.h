#ifndef SERVO_GATE_H_
#define SERVO_GATE_H_

#include <stdint.h>
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
    float gate_close_duty_cycle;
    float gate_open_duty_cycle;
} eeprom_servo_gate_config_t;



typedef struct {
    eeprom_servo_gate_config_t eeprom_servo_gate_config;
    gate_state_t gate_state;

    // RTOS queue
} servo_gate_t;


#ifdef __cplusplus
extern "C" {
#endif

bool servo_gate_init(void);
bool servo_gate_config_save(void);
bool http_rest_servo_gate_state(struct fs_file *file, int num_params, char *params[], char *values[]);


#ifdef __cplusplus
}
#endif


#endif  // SERVO_GATE_H_