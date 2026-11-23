#ifndef RECORDING_H_
#define RECORDING_H_

/*
    Every throw of the charge's controller recorded (ADR-009, gate 2's rest; api_v1.md, "Recordings"): a summary, the
    settings in force, the marks and the samples, read through /api/v1/recordings and the console's `ccrec`.

    The unit is the source, not the store: it keeps the newest throws' summaries and the samples of the throw being
    made and of the last complete one, so a client reading each throw as it finishes loses nothing. On the Pico 2 W
    they are kept across charges. On the Pico W they come from the heap for a charge and go at its end: its RAM has
    no room to keep them between charges, when an update's poll needs nearly all the heap. A charge never waits on its
    recording: what the heap cannot give is not recorded, and the summary says so (samples_kept false).

    Only the staged frame's throws are recorded; a throw of today's PID loop or of pid_learnt has none.

    Written by the job task; read by the HTTP server's task and the console's, under the recording's own lock.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "events.h"

typedef enum {
    RECORDING_MARK_COARSE_STOP = 0,     // the coarse tube stopped at its room: value, the reading
    RECORDING_MARK_FINE_STOP,           // the fine tube stopped to rest: value, the reading
    RECORDING_MARK_MOVE,                // a top-up: value, its turns
    RECORDING_MARK_PAUSED,              // no fresh reading (ADR-013): value, the pause's number
    RECORDING_MARK_RESUMED,             // value, the settled weight it went on from
    RECORDING_MARK_VERDICT,             // value, the weight judged
} recording_mark_kind_t;

// The controller's own figures of a throw, as its console line gives them
typedef struct {
    float first;            // the first error at rest; NaN when there was none
    uint8_t moves;
    uint8_t streams;
    float coarse_turns;
    float fine_turns;
    float stop_room;
} recording_controller_t;

#ifdef __cplusplus
extern "C" {
#endif

// At the start, before any task: its lock, and on the Pico 2 W its store
void recording_init(void);
// Its routes (api_v1.c)
bool recording_api_init(void);

// A staged charge begins and ends: on the Pico W, its store from the heap for the charge, and given back
void recording_charge_begin(void);
void recording_charge_end(void);

// A throw begins, at its zero: its entry and the settings in force. Its id, the throw's own number; 0 when none is had
uint16_t recording_throw_begin(uint16_t throw_number, float target, uint8_t law, bool simulated);
// Its pour begins, at now_ms on the unit's clock: its rows' t_ms count from here
void recording_pour_begin(uint32_t now_ms);
// A row, one a reading, at t_ms since the pour began: the weight, the speeds asked, the forecast, the glide's in shadow
void recording_row(uint32_t t_ms, float weight, float coarse_rps, float fine_rps, float forecast, float glide_coarse_rps,
                   float glide_fine_rps);
void recording_mark(recording_mark_kind_t kind, float value);
void recording_throw_controller(const recording_controller_t *figures);
// The throw's end: its verdict (result, EVENT_REASON_FINISHED), or why it was cut short (EVENT_RESULT_NONE, reason)
void recording_throw_end(event_result_t result, event_reason_t reason, float weight, float elapsed_s);

// The console: `ccrec [<recording>]` (a recording's rows, by default the newest kept), `ccrec nosamples on|off` (never
// stored: the next charges' samples taken as not had, as when the heap cannot give them)
void recording_console(const char *args, char *reply, size_t reply_size, void (*write)(const char *line, void *ctx),
                       void *write_ctx);

#ifdef __cplusplus
}
#endif

#endif  // RECORDING_H_
