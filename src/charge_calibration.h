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
// With band_gr above 0, its rules are made for that band and the glide, whatever band the calibration was written for
bool charge_calibration_get(uint8_t profile, float band_gr, charge_control_calibration_t *out);
/*
    The throws' first errors carried into a profile's calibration at a charge's end (ADR-009, 2026-11-22; a temporary
    form, until the profiles' stored and shared form is settled: README, "Open, to be discussed"). Each is a throw's
    first error at rest translated to the calibration's own stop room; they are added to its first errors, the newest
    CHARGE_CALIBRATION_FIRST_ERRORS_MAX kept, and the stop is placed from them at the next charge's start. Nothing is
    written when the stop they place would move more than CHARGE_CARRY_MOST_DIGITS at once, nor for the simulated scale.
    True when written; `before` and `after` take the median first error the stop was and will be placed from.
*/
#define CHARGE_CARRY_MOST_DIGITS 3.0f
bool charge_calibration_carry(uint8_t profile, const float *errors, uint8_t count, float *before, float *after);

// Whether it holds one a throw may use, for a start's refusal
bool charge_calibration_held(uint8_t profile);

// The console's `ccal ...` and `ccon ...`
void charge_calibration_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx);
void charge_constants_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                              void *write_ctx);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_CALIBRATION_H_
