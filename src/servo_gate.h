#ifndef SERVO_GATE_H_
#define SERVO_GATE_H_

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

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

#ifndef FEATURE_SERVO_GATE
#error "FEATURE_SERVO_GATE is set by CMakeLists.txt: 1 when the build carries the servo gate, 0 when not"
#endif

/*
 * What the rest of the firmware asks of the gate (ADR-008, gate 2). A build without it (-DFEATURE_SERVO_GATE=OFF) has
 * none of servo_gate.c, and these answer that there is no gate: never enabled, its state disabled, nothing moved
 */
#if FEATURE_SERVO_GATE
bool servo_gate_init(void);
bool servo_gate_config_save(void);
const char * gate_state_to_string(gate_state_t);
bool servo_gate_enabled(void);          // fitted and on in its settings: charge mode uses it
gate_state_t servo_gate_state(void);
float servo_gate_ratio(void);
void servo_gate_set_ratio(float ratio, bool block_wait);
// How long a move between two ratios takes at the gate's own speeds (A17): its travel (ADR-009, gate F.3)
float servo_gate_travel_s(float from, float to);
// The moves asked to anything short of closed since the start, by any side: what the gate held may have fallen
uint32_t servo_gate_opens(void);
#else
static inline bool servo_gate_init(void) { return true; }
static inline bool servo_gate_enabled(void) { return false; }
static inline gate_state_t servo_gate_state(void) { return GATE_DISABLED; }
static inline float servo_gate_ratio(void) { return SERVO_GATE_RATIO_DISABLED; }
static inline void servo_gate_set_ratio(float ratio, bool block_wait) { (void) ratio; (void) block_wait; }
static inline float servo_gate_travel_s(float from, float to) { (void) from; (void) to; return 0.0f; }
static inline uint32_t servo_gate_opens(void) { return 0; }
#endif

#ifdef __cplusplus
}
#endif


#endif  // SERVO_GATE_H_