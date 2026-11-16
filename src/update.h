#ifndef UPDATE_H_
#define UPDATE_H_

/*
 * Firmware updates (ADR-004), the part that is the same on every chip: which slot runs, a trial's watchdog and its
 * confirmation once the unit is on its network, and the update as a job (api_v1.md, "Firmware updates"). The slots
 * themselves are update_slots.h's; the API's routes are api_v1_update.c's.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"

typedef enum {
    UPDATE_IDLE = 0,
    UPDATE_STARTING,            // asked, and the menu has not begun it yet
    UPDATE_RECEIVING,           // the job runs: the image is taken into the free slot
    UPDATE_READY,               // the image is whole and checked
    UPDATE_INSTALLING,          // the restart into its trial is under way
    UPDATE_ENDING,              // ended: the mode gives the screen back, and the job is idle once the menu is back
} update_state_t;

// How the last update ended: `last` in GET /api/v1/update
typedef enum {
    UPDATE_RESULT_NONE = 0,
    UPDATE_RESULT_CONFIRMED,    // after the restart, the image tried confirmed itself
    UPDATE_RESULT_BACK,         // after the restart, the image tried was left
    UPDATE_RESULT_REFUSED,
    UPDATE_RESULT_STOPPED,
    UPDATE_RESULT_TIME,
} update_result_t;

// A refusal: its error code (api_v1.md) and a line on why; code NULL when there is none
typedef struct {
    const char *code;
    const char *message;
} update_refusal_t;

typedef struct {
    update_result_t result;
    const char *code;           // a refusal's, or NULL
    const char *message;
    uint32_t version;           // the block version tried, or 0
} update_last_t;

typedef struct {
    update_state_t state;
    const char *version;        // the firmware running
    char block[16];             // its metadata block's version, as the firmware's version string has it
    char slot;                  // 'a', 'b'
    bool on_trial;              // running on trial, not yet confirmed
    char takes[16];             // the release manifest's name for the image this unit takes
    uint32_t free_bytes;
    uint32_t received;
    uint32_t expected;          // the image's length, once its upload says it; 0 before (the screen's progress)
    update_last_t last;
} update_status_t;

typedef enum {
    UPDATE_START_OK = 0,
    UPDATE_START_BUSY,          // a job runs, a start is asked, or a person is in a mode or a view at the unit
    UPDATE_START_NO_SLOTS,      // the slots are not known: no update can be written
    UPDATE_START_ON_TRIAL,      // the image running is on trial: the free slot is the way back until it is confirmed
} update_start_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the slots at start-up, before the scheduler, and the note a trial left. On a trial, the watchdog is
 *        armed and fed until the image confirms itself or the trial's time runs out
 */
void update_init(void);

/**
 * @brief The web server listens (wireless.c). An image on trial confirms itself when the unit is on the network it
 *        is set up for; otherwise it is left, and the unit goes back at the trial's end
 * @param on_its_network station mode when Wi-Fi is set up, its own access point when it is not
 */
void update_network_ready(bool on_its_network);

// The job (api_v1_update.c)
void update_status(update_status_t *status);
update_start_result_t update_request_start(activity_by_t by);
// false when it is not ready: 409 not_ready
bool update_install(void);

// The image in pieces (api_v1_commands.c's upload): begin's status is 200 when it is taken, and its token names the
// upload in its pieces, its end or its abort. One upload at a time: another's token changes nothing
update_refusal_t update_image_begin(uint32_t bytes, int *status, uint32_t *token);
bool update_image_piece(uint32_t token, const uint8_t *data, size_t len);
update_refusal_t update_image_end(uint32_t token, int *status);
void update_image_abort(uint32_t token);

// A block version as the firmware's version string has it: (512, 256) is 2.0.1, (276, 218) is 1.20.0-218
void update_version_text(uint32_t version, char *out, size_t size);

/**
 * @brief The update's mode (menu.c): holds the screen while the job runs, the reset button stops it
 * @return the form to go back to
 */
uint8_t update_mode_menu(void);

/**
 * @brief The console's `update`: `update` gives the slots and the job, `update try` restarts into a trial of the
 *        other slot's image once it is checked
 * @return true for an @ok reply
 */
bool update_console(const char *args, char *reply, size_t reply_size);

#ifdef __cplusplus
}
#endif

#endif  // UPDATE_H_
