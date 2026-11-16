#ifndef EVENTS_H_
#define EVENTS_H_
/*
 * One event log for the unit (ADR-003, gate 4; api_v1.md, "The event log"): the newest EVENTS_KEPT changes kept and
 * numbered from 1 at each start, which any number of clients read, each from where it left off. An event is a
 * change, not a state. Each is kept as a small record and written as JSON only when it is read.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVENTS_KEPT 64

typedef enum {
    EVENT_SOURCE_CHARGE = 0,
    EVENT_SOURCE_THROW,
    EVENT_SOURCE_CLEANUP,
    EVENT_SOURCE_MOTORS,
    EVENT_SOURCE_SYSTEM,
    EVENT_SOURCE_UPDATE,        // ADR-004
} event_source_t;

typedef enum {
    EVENT_STARTED = 0,
    EVENT_STOPPED,
    EVENT_PAUSED,
    EVENT_RESUMED,
    EVENT_FINISHED,
    EVENT_ENDED,
    EVENT_FAULT,
    EVENT_CLEARED,
    EVENT_RECEIVED,             // an update's image, whole and checked
    EVENT_INSTALLING,
    EVENT_CONFIRMED,
    EVENT_BACK,
} event_kind_t;

// Why a job stopped or a throw ended (api_v1.md's `reason`)
typedef enum {
    EVENT_REASON_NONE = 0,
    EVENT_REASON_STOPPED,
    EVENT_REASON_FAULT,
    EVENT_REASON_TIME,
    EVENT_REASON_FINISHED,
    EVENT_REASON_SCALE_NO_READING,
    EVENT_REASON_SCALE_UNSTEADY,
    EVENT_REASON_SCALE_UNRELIABLE,
    EVENT_REASON_DRIVER_FAULT,
    EVENT_REASON_REFUSED,       // an update's image
} event_reason_t;

// A throw's verdict (api_v1.md's `result`)
typedef enum {
    EVENT_RESULT_NONE = 0,
    EVENT_RESULT_GOOD,
    EVENT_RESULT_UNDER,
    EVENT_RESULT_OVER,
} event_result_t;

typedef struct {
    uint32_t seq;           // set by events_add
    uint64_t at_ms;         // set by events_add: the unit's clock, since its start
    uint8_t source;         // event_source_t
    uint8_t kind;           // event_kind_t
    uint8_t by;             // activity_by_t: a job's start or stop
    uint8_t reason;         // event_reason_t
    uint16_t throw_number;
    uint8_t number;         // a pause's number in its throw; a powder profile's id; a motor (motor_select_t)
    uint8_t result;         // event_result_t
    float value;            // a target, a weight
    float value2;           // a throw's seconds
    const char *text;       // a driver's fault ("short"), the firmware's version: a string that lives on
} event_t;

#ifdef __cplusplus
extern "C" {
#endif

// At the start, before any task: the log, and its first event (system started)
void events_init(void);
// An event, numbered and timed here; any task
void events_add(const event_t *event);
// The newest event's number: 0 before any
uint32_t events_latest_seq(void);
// The oldest kept
uint32_t events_first_seq(void);
// The event of that number, if it is still kept
bool events_get(uint32_t seq, event_t *event);
// An event as the API gives it
void events_write_json(const event_t *event, char *out, size_t room, size_t *len);

// The names, for the charge's `last` and others
const char *events_reason_name(event_reason_t reason);
const char *events_result_name(event_result_t result);

#ifdef __cplusplus
}
#endif

#endif  // EVENTS_H_
