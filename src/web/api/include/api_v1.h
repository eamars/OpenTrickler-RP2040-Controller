#ifndef API_V1_H_
#define API_V1_H_
/*
 * The routes of API v1 (ADR-003; docs/developments_plans/ADR-003-http_rest_server/api_v1.md): the index, the
 * screen, the powder profiles' list and /api/v1/debug here; the settings' from their tables (settings.c); the
 * commands, the event log and the unit's reads in api_v1_commands.c.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool api_v1_init(void);
bool api_v1_commands_init(void);

// The commands' share of GET /api/v1/schema (gate 4): JSON texts the schema's stream writes one at a time. Actions
// and events are objects; states are members ("charge":[...]). NULL past the last
typedef enum {
    API_V1_SCHEMA_ACTIONS,
    API_V1_SCHEMA_STATES,
    API_V1_SCHEMA_EVENTS,
} api_v1_schema_part_t;
const char *api_v1_schema_item(api_v1_schema_part_t part, size_t idx);

// The state of the job that runs (the charge's or the cleanup's), "idle" when none: what a 409 busy carries
const char *api_v1_job_state(void);

// A random number new at each start, so a client sees a restart (the index's and the event log's `boot`)
uint32_t api_v1_boot_id(void);
// "pico_w", "pico2_w"
const char *api_v1_board_name(void);

/*
 * The unit's screen as the API gives it (ADR-006, gate 6): its frame, for the page's mirror, and the hello an agent
 * says. The UI hands these over at its start, so the API names no screen. With none handed over the display's two
 * routes answer 404 and a hello 409.
 */
#define API_V1_HELLO_NAME_MAX 16        // api_v1.md: a hello's name, 1 to 16 characters
typedef struct {
    uint16_t width;                     // the frame, in pixels
    uint16_t height;
    size_t frame_bytes;                 // and in bytes
    const char *format;                 // its layout by name: "mono_rows" (api_v1.md, /api/v1/display)
    void (*frame_copy)(uint8_t *out);   // the frame as a person sees it now, whole
    // The hello shown: name is printable ASCII, API_V1_HELLO_NAME_MAX characters at most; *flash says whether the
    // panel flashed first. False while a charge has the screen
    bool (*hello)(const char *name, bool *flash);
} api_v1_screen_t;

// The UI, once, before the server starts. The struct is kept by its address
void api_v1_set_screen(const api_v1_screen_t *screen);
// The screen handed over, NULL when there is none (the API's own files)
const api_v1_screen_t *api_v1_screen(void);

#ifdef __cplusplus
}
#endif

#endif  // API_V1_H_
