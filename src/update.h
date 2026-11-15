#ifndef UPDATE_H_
#define UPDATE_H_

/*
 * Firmware updates (ADR-004), the part that is the same on every chip: which slot runs, a trial's watchdog, and the
 * confirmation once the unit is on its network. The slots themselves are update_slots.h's.
 */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the slots at start-up, before the scheduler. On a trial, the watchdog is armed and fed until the image
 *        confirms itself or the trial's time runs out
 */
void update_init(void);

/**
 * @brief The web server listens (wireless.c). An image on trial confirms itself when the unit is on the network it
 *        is set up for; otherwise it is left, and the unit goes back at the trial's end
 * @param on_its_network station mode when Wi-Fi is set up, its own access point when it is not
 */
void update_network_ready(bool on_its_network);

/**
 * @brief The console's `update`: `update` gives the slots, `update try` restarts into a trial of the other slot
 * @return true for an @ok reply
 */
bool update_console(const char *args, char *reply, size_t reply_size);

#ifdef __cplusplus
}
#endif

#endif  // UPDATE_H_
