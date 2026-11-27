#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "events.h"
#include "rtos_port.h"
#include "setup.h"

static const char *const setup_step_names[] = {"code", "wifi", "scale", "motors", "ready", "done"};

// Read by the API's task and the menu's, written by the API's and the Wi-Fi's: in critical sections
static setup_step_t setup_now = SETUP_STEP_CODE;
static char setup_network_chosen[SETUP_NETWORK_MAX];
static volatile uint32_t setup_change_count = 0;


setup_step_t setup_step(void) {
    return setup_now;
}


void setup_network(char *out, size_t room) {
    rtos_critical_enter();
    size_t len = strnlen(setup_network_chosen, sizeof(setup_network_chosen));
    len = len < room - 1 ? len : room - 1;
    memcpy(out, setup_network_chosen, len);
    out[len] = '\0';
    rtos_critical_exit();
}


uint32_t setup_changes(void) {
    return setup_change_count;
}


void setup_set_step(setup_step_t step, const char *network, activity_by_t by) {
    size_t len = network != NULL ? strnlen(network, sizeof(setup_network_chosen) - 1) : 0;
    rtos_critical_enter();
    bool changed = step != setup_now || len != strlen(setup_network_chosen) ||
                   (len > 0 && memcmp(setup_network_chosen, network, len) != 0);
    setup_now = step;
    if (len > 0) {
        memcpy(setup_network_chosen, network, len);
    }
    setup_network_chosen[len] = '\0';
    if (changed) {
        setup_change_count += 1;
    }
    rtos_critical_exit();
    if (changed) {
        event_t event = {.source = EVENT_SOURCE_SETUP, .kind = EVENT_STEP, .by = (uint8_t) by,
                         .text = setup_step_names[step]};
        events_add(&event);
    }
}


const char *setup_step_name(setup_step_t step) {
    return step <= SETUP_STEP_DONE ? setup_step_names[step] : "code";
}


bool setup_step_from_name(const char *name, setup_step_t *step) {
    for (size_t idx = 0; idx < sizeof(setup_step_names) / sizeof(setup_step_names[0]); idx += 1) {
        if (strcmp(setup_step_names[idx], name) == 0) {
            *step = (setup_step_t) idx;
            return true;
        }
    }
    return false;
}
