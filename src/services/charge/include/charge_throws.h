#ifndef CHARGE_THROWS_H_
#define CHARGE_THROWS_H_
/*
 * The throw record (ADR-016, gate 1): every throw, finished or ended short, leaves one record of what it was thrown
 * on and what it did, for an agent off the unit to read (GET /api/v1/charge/throws). The newest CHARGE_THROWS_KEPT
 * are kept in RAM, taken from the heap at the first charge and kept, until the unit restarts; each is numbered as its
 * throw is (from 1 at each start). A figure the throw did not reach is NaN, null in JSON.
 *
 * The charge's task alone writes: it opens a throw's record as the throw begins, fills it as the throw goes, and
 * closes it at the verdict or at the end. Any task reads a closed record by its number.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHARGE_THROWS_KEPT 32

typedef struct {
    uint64_t at_ms;             // when it was closed: its verdict read, or its end; the unit's clock since its start
    float target;
    float weight;               // the verdict's; for a throw ended short, the reading at its end
    float elapsed_s;            // the whole time, from its pour's start
    float coarse_s;             // to the coarse tube's stop
    float fine_s;               // the fine tube's part, from the coarse tube's stop to the fine stop
    float coarse_stop_weight;   // the reading at the coarse tube's stop
    float coarse_stop_rps;      // the coarse tube's speed asked just before it
    float fine_stop_weight;     // the reading at the fine stop
    float fine_stop_rps;
    float coarse_kp;            // the gains it was thrown on
    float coarse_ki;
    float coarse_kd;
    float fine_kp;
    float fine_ki;
    float fine_kd;
    float coarse_threshold;
    float fine_threshold;
    uint16_t number;
    uint8_t charge_profile;
    uint8_t powder_profile;
    uint8_t result;             // event_result_t
    uint8_t reason;             // event_reason_t
    uint8_t pauses;             // for want of readings (ADR-013)
    uint8_t simulated : 1;      // thrown with the simulated scale
    uint8_t configured : 1;     // it reached its pour: the gains and the switches below are known
    uint8_t by_tuner : 1;       // the gains were the auto-tuner's trial, not the profile's
    uint8_t forecast : 1;
    uint8_t auto_tune : 1;
    uint8_t backoff : 1;
} charge_throw_t;

/**
 * @brief The records' room, taken from the heap the first time and kept. True when it is there.
 */
bool charge_throws_ready(void);

/**
 * @brief The record of the throw now beginning, as given: its number, profiles, target and scale; every figure NaN.
 * @return The record to fill as the throw goes, or NULL when there is no room (charge_throws_ready()).
 */
charge_throw_t *charge_throws_open(uint16_t number, uint8_t charge_profile, uint8_t powder_profile, float target,
                                   bool simulated);

/**
 * @brief The open record, or NULL when no throw's record is open.
 */
charge_throw_t *charge_throws_current(void);

/**
 * @brief The open record closed, with its end, and kept as the newest; nothing when none is open.
 */
void charge_throws_close(uint8_t result, uint8_t reason, float weight);

/**
 * @brief The newest record's number, and the oldest kept: 0 for both until a throw is recorded.
 */
uint16_t charge_throws_newest(void);
uint16_t charge_throws_first(void);

/**
 * @brief A record kept, copied by its number; false when it is not kept.
 */
bool charge_throws_get(uint16_t number, charge_throw_t *record);

/**
 * @brief A record as the API gives it (api_v1.md, "The throws"), appended to out: never past room.
 */
void charge_throws_write_json(const charge_throw_t *record, char *out, size_t room, size_t *len);

#ifdef __cplusplus
}
#endif

#endif  // CHARGE_THROWS_H_
