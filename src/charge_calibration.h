#ifndef CHARGE_CALIBRATION_H_
#define CHARGE_CALIBRATION_H_

/*
    Each powder profile's calibration for the staged controller (ADR-009), in an EEPROM slot of its own
    (EEPROM_CHARGE_CALIBRATION_BASE_ADDR, 1 KB a profile), and the frame's constants the bench tunes, live, never
    stored. A slot holds what the calibration measured, the rehearsals' first errors, and the finish and band asked
    for; the rules are made from them on the unit (charge_control_set_rules, then charge_control_place_stop), so
    another finish or band needs no new calibration.

    Until the calibration runs on the unit (gate 3), a slot is written from the PC through the console's `ccal`
    (tools/charge_sim/bench/push_calibration.py), and the constants are read and set with `ccon`.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "charge_control.h"
#include "remote_control.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHARGE_CALIBRATION_FIRST_ERRORS_MAX 16

typedef struct {
    uint16_t legacy_rev;            // load_config's old revision check: 0 once stored
    uint8_t valid;                  // a calibration is held
    uint8_t first_error_count;
    charge_control_calibration_t measured;  // the measured part; finish and band_gr as asked; the rules unset
    float first_errors[CHARGE_CALIBRATION_FIRST_ERRORS_MAX];  // the rehearsals' first errors at rest
} charge_calibration_record_t;

// The constants a throw starts with: the research's by default, set by `ccon` until a restart
const charge_control_constants_t *charge_calibration_constants(void);

// The calibration of a powder profile with its rules made, for a throw. False when the profile has none
bool charge_calibration_get(uint8_t profile, charge_control_calibration_t *out);

// The console's `ccal ...` and `ccon ...`
void charge_calibration_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx);
void charge_constants_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                              void *write_ctx);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_CALIBRATION_H_
