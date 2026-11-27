#ifndef UPDATE_POLL_H_
#define UPDATE_POLL_H_

/*
 * The update's poll (ADR-010, gate 5; menu_design.md, "The update at the screen"). The unit asks its source for the
 * newest release on its channel, once each start after it joins its network as a station and again when asked, and
 * offers it when its image is newer than the one running. Nothing is downloaded until a person says Install: then
 * the update's job fetches the image itself (update.c).
 *
 * The source is a repository's address, its releases laid out under it as GitHub lays them out; the bench points it
 * at the local OTA server (tools/ota_server.py), never at GitHub's releases (the owner). Stable reads
 * releases/latest/download/manifest.json, which GitHub never resolves to a pre-release. Beta reads the newest
 * release of all, its tag from GitHub's list of releases (api.github.com for github.com, /api/v3 on any other host,
 * as GitHub Enterprise has it), then that release's manifest.
 *
 * The channel and the source are one stored struct in a slot of their own, and /api/v1/settings/update (ADR-008's
 * page sets them).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "update.h"

#define UPDATE_SOURCE_MAX 128
#define UPDATE_NOTE_MAX 96

typedef enum : uint8_t {
    UPDATE_CHANNEL_STABLE = 0,
    UPDATE_CHANNEL_BETA,
} update_channel_t;

// The stored format: its layout changes nothing else's (EEPROM_UPDATE_CONFIG_BASE_ADDR, a slot of its own)
typedef struct {
    uint16_t update_config_rev;                     // 0: validated by its CRC
    update_channel_t channel;
    char source[UPDATE_SOURCE_MAX];                 // a repository's address, GitHub's layout under it
} eeprom_update_config_t;

typedef enum {
    UPDATE_POLL_IDLE = 0,                           // not asked since the start, or the source changed since
    UPDATE_POLL_CHECKING,
    UPDATE_POLL_UP_TO_DATE,                         // the channel's newest is not newer, or the channel has none
    UPDATE_POLL_FOUND,
    UPDATE_POLL_FAILED,
} update_poll_state_t;

typedef struct {
    update_poll_state_t state;
    char error[64];                                 // FAILED: why
    // FOUND: the release, as its manifest gives it
    char version[24];
    uint32_t block;                                 // its image's block version: newer than the image running
    char note[UPDATE_NOTE_MAX];                     // its note's start; the whole is on the web
    bool critical;                                  // "Important update"
    update_channel_t channel;                       // the channel it was found on
    char bench[16];                                 // the bench host's address it was found on; "" for the source
} update_poll_status_t;

typedef enum {
    UPDATE_POLL_ASKED = 0,
    UPDATE_POLL_BUSY,                               // a poll runs, or the update's job does
    UPDATE_POLL_NO_NETWORK,                         // not joined to a network as a station: no internet
} update_poll_ask_t;

// The image the poll found: where it is, its length and its SHA-256, as the manifest gives them (update.h)
typedef update_found_image_t update_poll_image_t;

#ifdef __cplusplus
extern "C" {
#endif

// At start-up, after the EEPROM: the stored channel and source, the settings' table, the HTTPS client; and the
// update is handed its fetch (update_set_fetcher())
bool update_poll_init(void);
bool update_poll_config_save(void);

// The station joined its network: the start's poll, once
void update_poll_station_joined(void);

// A poll now: the menu's "Check for update", POST /api/v1/update/check, the console's `update check`
update_poll_ask_t update_poll_ask(void);

void update_poll_status(update_poll_status_t *status);

// The image found, for the job's download; false when none is found
bool update_poll_found_image(update_poll_image_t *image);

// The console: `update poll`, how the last poll and fetch went
bool update_poll_console(char *reply, size_t reply_size);

#ifdef __cplusplus
}
#endif

#endif  // UPDATE_POLL_H_
