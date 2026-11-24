#ifndef CHARGE_MODE_H_
#define CHARGE_MODE_H_

#include <stdint.h>
#include "activity.h"
#include "common.h"
#include "events.h"
#include "charge_rules.h"
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

// What pours the next charge (ADR-009, gate F.5): the controller, or for the console's comparisons alone, never
// stored, the frame's glide law or today's PID loop
typedef enum {
    CHARGE_POUR_CONTROLLER = 0,
    CHARGE_POUR_GLIDE = 1,
    CHARGE_POUR_LEGACY = 2,
} charge_pour_t;

typedef struct {
    uint16_t charge_mode_data_rev;

    float coarse_stop_threshold;
    float fine_stop_threshold;

    float set_point_sd_margin;
    float set_point_mean_margin;
    float coarse_stop_gate_ratio; // 0.0=open, 1.0=close, -1.0=disabled (optional)

    bool coarse_stop_backoff_enable;
    // The filler after the bool above (ADR-009, A18): its first two bytes held the loop and the pace until gate F.5,
    // read as nothing and written as zero since; the third is the auto-tuner's switch, 0 on (the register's row 1).
    // The record's size and CRC stay: every unit has held 0 in the third, a pad never written
    uint8_t a18_unused[2];
    uint8_t auto_tune_off;
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
    CHARGE_START_CALIBRATION_DAMAGED,   // the same, its record stored but failing its check: refused as none, said so
    CHARGE_START_TOLERANCE_UNDER_DIGIT, // the fine stop threshold under the scale's digit (ADR-009, gate F.2)
    CHARGE_START_NO_SPEED,      // a top speed of 0 for a tube the throw needs: the profile's or the motor's (F.2)
} charge_start_result_t;
charge_start_result_t charge_mode_request_start(activity_by_t by, float target, int powder_profile);

/*
 * For the bench (ADR-009): the next charge is a series of `throws` into the same cup, with no cup lifted between
 * them. Each throw starts from the weight the cup rests at and is judged from it, as the PC's trial did; the series
 * ends after its last throw, or when the cup holds CHARGE_SERIES_CUP_MOST. 0: the next charge as any other. Taken by
 * the next start and never stored.
 */
void charge_mode_set_series(uint16_t throws);

// For the console's comparisons (ADR-009, gate F.5): what pours the next charge, taken by its start and never stored;
// a charge so poured carries nothing into a record and learns nothing
void charge_mode_set_pour(charge_pour_t pour);


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
    uint16_t last_recording;        // the last throw's recording, 0 when it has none (ADR-009)
} charge_status_t;
void charge_mode_get_status(charge_status_t *status);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // CHARGE_MODE_H_