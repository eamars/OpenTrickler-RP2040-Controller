#ifndef SETUP_H_
#define SETUP_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "activity.h"

/*
    The set-up's stage, kept by the unit (ADR-008, gate 4: "The set-up, designed again"). The page and the 12864 are
    two views of one set-up: the page tells the unit its step as it goes on, the menu draws the same stage, and a page
    that comes back after losing the hotspot reads where the set-up is. Kept in RAM: a restart begins it again, unless
    the network was joined and stored, which makes the unit set up.
*/

typedef enum {
    SETUP_STEP_CODE = 0,    // not begun: the code, a phone not yet joined
    SETUP_STEP_WIFI,
    SETUP_STEP_SCALE,
    SETUP_STEP_MOTORS,
    SETUP_STEP_READY,
    SETUP_STEP_DONE,
} setup_step_t;

#define SETUP_NETWORK_MAX 32    // a network's name, 1 to 31 bytes and its ending

#ifdef __cplusplus
extern "C" {
#endif

// The step, and the network the page chose (on the Wi-Fi step; "" for none)
setup_step_t setup_step(void);
void setup_network(char *out, size_t room);
// A count that changes with each change, so a view knows to draw again
uint32_t setup_changes(void);

// The page's step, by whom, with the network chosen on the Wi-Fi step (NULL or "": none yet); a step event
void setup_set_step(setup_step_t step, const char *network, activity_by_t by);

// A step's name as the API gives it, and the step for a name (false: none)
const char *setup_step_name(setup_step_t step);
bool setup_step_from_name(const char *name, setup_step_t *step);

#ifdef __cplusplus
}
#endif

#endif  // SETUP_H_
