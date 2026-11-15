#ifndef API_V1_H_
#define API_V1_H_
/*
 * The routes of API v1 (ADR-003; docs/developments_plans/ADR-003-http_rest_server/api_v1.md): the index, the
 * screen, the powder profiles' list and /api/v1/debug here; the settings' from their tables (settings.c); the
 * commands, the event log and the unit's reads in api_v1_commands.c.
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool api_v1_init(void);
bool api_v1_commands_init(void);

// A random number new at each start, so a client sees a restart (the index's and the event log's `boot`)
uint32_t api_v1_boot_id(void);
// "pico_w", "pico2_w"
const char *api_v1_board_name(void);

#ifdef __cplusplus
}
#endif

#endif  // API_V1_H_
