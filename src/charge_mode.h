#ifndef CHARGE_MODE_H_
#define CHARGE_MODE_H_

#include <stdint.h>
#include "activity.h"
#include "common.h"
#include "events.h"
#include "neopixel_led.h"


#define EEPROM_CHARGE_MODE_DATA_REV                     8              // 16 byte 

#define WEIGHT_STRING_LEN 8

typedef enum {
    CHARGE_MODE_EXIT = 0,
    CHARGE_MODE_WAIT_FOR_ZERO = 1,
    CHARGE_MODE_WAIT_FOR_COMPLETE = 2,
    CHARGE_MODE_WAIT_FOR_CUP_REMOVAL = 3,
    CHARGE_MODE_WAIT_FOR_CUP_RETURN = 4,
} charge_mode_state_t;

typedef struct {
    uint16_t charge_mode_data_rev;

    float coarse_stop_threshold;
    float fine_stop_threshold;

    float set_point_sd_margin;
    float set_point_mean_margin;
    float coarse_stop_gate_ratio; // 0.0=open, 1.0=close, -1.0=disabled (optional)

    bool coarse_stop_backoff_enable;
    float coarse_stop_backoff_turns; // 0.125, 0.25, 0.5, 1.0
    float coarse_stop_backoff_speed_rps; // if <=0 uses min speed

    decimal_places_t decimal_places;

    // Precharge
    bool precharge_enable;
    uint32_t precharge_time_ms;
    float precharge_speed_rps;

    // LED related settings
    rgbw_u32_t neopixel_normal_charge_colour;
    rgbw_u32_t neopixel_under_charge_colour;
    rgbw_u32_t neopixel_over_charge_colour;
    rgbw_u32_t neopixel_not_ready_colour;

    // The calibration's cup (ADR-009 step 1, items 1 and 2), in the scale's unit; -1 never calibrated. Appended at the
    // end: an earlier version's record is kept by load_config (ADR-009, gate 2)
    float cup_capacity;
    float cup_weight;
    // The auto-tuner's switch (ADR-009 step 2, gate 3), on by default. Appended at the end: an earlier version's record
    // is kept by load_config, this field its default
    bool auto_tune;
} eeprom_charge_mode_data_t;

typedef struct {
    eeprom_charge_mode_data_t eeprom_charge_mode_data;
    float target_charge_weight;
    uint32_t charge_mode_event;
    charge_mode_state_t charge_mode_state;
} charge_mode_config_t;


// C Functions
#ifdef __cplusplus
extern "C" {
#endif


bool charge_mode_config_init(void);
// Its settings' table, for the API and the console (charge_mode_settings.c; ADR-003, gate 3)
bool charge_mode_settings_register(void);

uint8_t charge_mode_menu(void);
// The charge's state in its own words ("Waiting for Zero", "Target: 30.00", a fault's reason), as the screen
// shows it (ADR-010, gate 4)
const char *charge_mode_get_title(void);
bool charge_mode_config_save(void);

// The seconds of the throw under way, or of the last one (ADR-003's GET /api/v1/charge; the screen's own figure)
float charge_mode_get_elapsed_s(void);

/*
 * One target for the unit (ADR-003, gate 4): the menu's weight digits show it and set it. 0 when unset. Setting it
 * fails for a target the digits cannot show: 0.01 to 999.99 at two decimals, 0.001 to 99.999 at three, in their
 * steps. Between throws it is the next throw's.
 */
float charge_mode_get_target(void);
bool charge_mode_target_fits(float target);
bool charge_mode_set_target(float target);
// The same at other decimals: for a change of decimal places, which keeps the target's weight (review D2)
float charge_mode_target_at(decimal_places_t places);
bool charge_mode_target_fits_at(float target, decimal_places_t places);
// After the decimals changed from before: the digits written again for the same weight
void charge_mode_keep_target(decimal_places_t before);

/*
 * A charge asked from outside the menu, by the API or the console (gate 4): handed to the menu with no wait, or
 * refused. target (NAN: the one set) and powder_profile (-1: the one chosen) are written only once the start is
 * taken, so a start refused writes nothing; the caller has checked them. A stop is
 * activity_request_stop(ACTIVITY_CHARGE, by).
 */
typedef enum {
    CHARGE_START_OK = 0,
    CHARGE_START_BUSY,          // a job runs, a start is asked already, or a person has a mode or a view at the unit
    CHARGE_START_DRIVER_FAULT,
    CHARGE_START_NO_TARGET,
} charge_start_result_t;
charge_start_result_t charge_mode_request_start(activity_by_t by, float target, int powder_profile);

// What the controller costs (ADR-009, gate 3), since the unit started: its time each millisecond and each reading, the
// most and the mean, in microseconds; the heap's least free while a charge ran
typedef struct {
    uint32_t tick_most_us;
    uint32_t tick_mean_us;
    uint32_t reading_most_us;
    uint32_t reading_mean_us;
    uint32_t heap_low;
} charge_cost_t;
void charge_mode_get_cost(charge_cost_t * cost);

// The charge as GET /api/v1/charge gives it
typedef struct {
    const char *state;      // idle, starting, zeroing, charging, waiting_for_cup_removal, waiting_for_cup_return, fault
    bool in_throw;
    uint16_t throw_number;  // this throw's, or the last's: from 1 at each start of the unit
    bool paused;            // a throw waits for readings (ADR-013)
    float target;           // 0: unset
    float elapsed_s;
    uint16_t last_throw;
    event_result_t last_result;
    event_reason_t last_reason;
    float last_weight;
} charge_status_t;
void charge_mode_get_status(charge_status_t *status);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // CHARGE_MODE_H_