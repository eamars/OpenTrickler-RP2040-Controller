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
#include "charge_pid_learnt.h"
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

/*
    Each speed law's own carried stop (ADR-009, 2026-11-22 night; a temporary form, as the carry above). The glide's
    first errors are the calibration's own (above); the PID law's are kept apart, a list a profile in one record of
    their own (EEPROM_CHARGE_LAW_STOPS_ADDR), so no stored record changes. A law whose list is empty places its stop
    from the glide's. Its carry is the glide's: the newest CHARGE_CALIBRATION_FIRST_ERRORS_MAX, at most
    CHARGE_CARRY_MOST_DIGITS at once, never for the simulated scale; but its first carry, into an empty list, is taken
    whole, since the glide's stop it was placed from is not its own.
*/
#define CHARGE_CALIBRATION_PROFILES 8
typedef struct {
    uint16_t legacy_rev;            // load_config's old revision check: 0 once stored
    uint8_t counts[CHARGE_CALIBRATION_PROFILES];
    float first_errors[CHARGE_CALIBRATION_PROFILES][CHARGE_CALIBRATION_FIRST_ERRORS_MAX];
} charge_calibration_law_stops_t;

// The calibration for a throw of a speed law (0 the glide's, 1 the PID law), its stop placed from that law's list
bool charge_calibration_get_law(uint8_t profile, float band_gr, uint8_t law, charge_control_calibration_t *out);
// A charge's first errors carried into its law's list; as charge_calibration_carry, and `own` false when the law's
// list was empty and its stop came from the glide's
bool charge_calibration_carry_law(uint8_t profile, uint8_t law, const float *errors, uint8_t count, float *before,
                                  float *after);

/*
    The auto-tuner's offer (ADR-009, 2026-11-22 night): the PID law's gains made from a profile's calibration, its rules
    set for the band, as staged.pid_offer makes them. The coarse tube: kp 1 / (grains a turn x coarse glide time),
    between the frame's coarse floor and top. The fine tube: kp 0.6 / (grains a turn x glide time), the P loop's time
    constant its longest lag; its least speed the frame's floor, its top the frame's. No I, no D. False when the
    profile holds no calibration.
*/
typedef struct {
    float coarse_kp;
    float coarse_ki;
    float coarse_kd;
    float coarse_min_rps;
    float coarse_max_rps;
    float fine_kp;
    float fine_ki;
    float fine_kd;
    float fine_min_rps;
    float fine_max_rps;
} charge_calibration_offer_t;

bool charge_calibration_offer(uint8_t profile, float band_gr, charge_calibration_offer_t *out);

// The console's `ctune`: the selected profile's PID settings and the offer; `ctune accept` writes the offer into the
// profile, live (stored by `setting powder_profiles/<n> save`), and forgets the PID law's carried stop for it
void charge_tune_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                         void *write_ctx);

// The stop room of the last calibration made for a throw before its stop was placed (the rules' own, where the
// rehearsals were thrown): a carried first error is translated to it (2026-11-22 night)
float charge_calibration_base_stop_room(void);

// Whether it holds one a throw may use, for a start's refusal
bool charge_calibration_held(uint8_t profile);

// The PID with learning's constants (an experiment, ADR-009, 2026-11-22 evening): its defaults, set by `cpid` until a
// restart
const charge_pid_constants_t *charge_pid_constants(void);
// Its learnt figure for a powder profile: kept while the unit runs, started afresh when another profile is thrown
charge_pid_learning_t *charge_pid_learning(uint8_t profile);

// The console's `ccal ...`, `ccon ...` and `cpid ...`
void charge_calibration_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx);
void charge_constants_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                              void *write_ctx);
void charge_pid_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                        void *write_ctx);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_CALIBRATION_H_
