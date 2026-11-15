/*
 * Firmware updates (ADR-004): the part that is the same on every chip. At start-up the slots are read; an image
 * booted on trial arms the watchdog and feeds it while it waits for the network, and confirms itself once the unit
 * is on the network it is set up for and its web server listens. An image that does not confirm itself in time is
 * left: the watchdog restarts the unit, and the other slot's image boots.
 */
#include "update.h"

#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "hardware/watchdog.h"

#include "activity.h"
#include "update_slots.h"
#include "version.h"

#define UPDATE_TRIAL_WATCHDOG_MS 8000       // the watchdog while on trial: the RP2040's longest is 8.3 s
#define UPDATE_TRIAL_FEED_MS 1000
#define UPDATE_TRIAL_LIMIT_MS 60000         // fed no longer than this: a trial that is not on its network by then ends

#ifndef PICO_CRT0_VERSION_MAJOR
#define PICO_CRT0_VERSION_MAJOR 0
#endif
#ifndef PICO_CRT0_VERSION_MINOR
#define PICO_CRT0_VERSION_MINOR 0
#endif

static update_slots_t update_slots;
static bool update_slots_known = false;
static volatile bool update_confirmed = false;
static volatile int update_confirm_result = 0;
static TickType_t update_trial_started = 0;
static TimerHandle_t update_feed_timer = NULL;


static char update_slot_name(int8_t slot) {
    return slot == 0 ? 'a' : (slot == 1 ? 'b' : '-');
}


// While on trial: the watchdog is fed until the image confirms itself or the trial's time runs out
static void update_feed(TimerHandle_t timer) {
    if (update_confirmed) {
        xTimerStop(timer, 0);
        return;
    }
    if ((xTaskGetTickCount() - update_trial_started) >= pdMS_TO_TICKS(UPDATE_TRIAL_LIMIT_MS)) {
        printf("Update: not confirmed within %d s; the watchdog restarts into the other slot\n",
               UPDATE_TRIAL_LIMIT_MS / 1000);
        xTimerStop(timer, 0);
        return;
    }
    watchdog_update();
}


void update_init(void) {
    update_slots_known = update_slots_read(&update_slots);
    printf("Update: running slot %c, version %s (block %d.%d)%s\n", update_slot_name(update_slots.running),
           version_string, PICO_CRT0_VERSION_MAJOR, PICO_CRT0_VERSION_MINOR,
           update_slots.on_trial ? ", on trial: it confirms itself once the unit is on its network" : "");
    if (!update_slots.on_trial) {
        return;
    }
    watchdog_enable(UPDATE_TRIAL_WATCHDOG_MS, true);
    update_trial_started = xTaskGetTickCount();
    update_feed_timer = xTimerCreate("Update feed", pdMS_TO_TICKS(UPDATE_TRIAL_FEED_MS), pdTRUE, NULL, update_feed);
    if (update_feed_timer == NULL || xTimerStart(update_feed_timer, 0) != pdPASS) {
        printf("Update: no timer to feed the trial's watchdog; the unit goes back\n");
    }
}


void update_network_ready(bool on_its_network) {
    if (!update_slots.on_trial || update_confirmed) {
        return;
    }
    if (!on_its_network) {
        printf("Update: not on its network; not confirmed, the unit goes back at the trial's end\n");
        return;
    }
#if RC_TEST_NO_CONFIRM
    // A test build that never confirms itself (rc_run.py slot_trial --expect back)
    printf("Update: a test build: not confirmed\n");
    return;
#endif
    update_confirm_result = update_slots_confirm();
    if (update_confirm_result != 0) {
        printf("Update: the confirmation failed (%d); the unit goes back\n", update_confirm_result);
        return;
    }
    update_confirmed = true;
    watchdog_disable();
    printf("Update: slot %c confirmed\n", update_slot_name(update_slots.running));
}


bool update_console(const char *args, char *reply, size_t reply_size) {
    while (*args == ' ') {
        args += 1;
    }
    if (*args == '\0') {
        snprintf(reply, reply_size,
                 "@ok update slot=%c known=%d trial=%d confirmed=%d confirm_result=%d ver=%s block=%d.%d "
                 "free=0x%lx+0x%lx",
                 update_slot_name(update_slots.running), update_slots_known, update_slots.on_trial,
                 update_confirmed, update_confirm_result, version_string, PICO_CRT0_VERSION_MAJOR,
                 PICO_CRT0_VERSION_MINOR, (unsigned long) update_slots.free_offset,
                 (unsigned long) update_slots.free_size);
        return true;
    }
    if (strcmp(args, "try") == 0) {
        if (activity_get() != ACTIVITY_NONE) {
            snprintf(reply, reply_size, "@err busy update try: %s runs", activity_name(activity_get()));
            return false;
        }
        if (!update_slots_known) {
            snprintf(reply, reply_size, "@err not_found update try: the slots are not known");
            return false;
        }
        printf("@ok update try: restarting into a trial of slot %c\n", update_slot_name(1 - update_slots.running));
        int result = update_slots_try_other();
        snprintf(reply, reply_size, "@err failed update try: the restart was refused (%d)", result);
        return false;
    }
    snprintf(reply, reply_size, "@err usage update [try]");
    return false;
}
