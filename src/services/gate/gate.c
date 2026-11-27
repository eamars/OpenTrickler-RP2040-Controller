#include <stddef.h>

#include "gate.h"


// The gate handed over at start-up; NULL in a build that carries none
static const gate_t *gate_handed = NULL;


void gate_set(const gate_t *gate) {
    gate_handed = gate;
}


bool gate_present(void) {
    return gate_handed != NULL;
}


bool gate_enabled(void) {
    return gate_handed != NULL && gate_handed->enabled();
}


gate_state_t gate_state(void) {
    return gate_handed != NULL ? gate_handed->state() : GATE_DISABLED;
}


gate_ratio_t gate_ratio(void) {
    return gate_handed != NULL ? gate_handed->ratio() : GATE_RATIO_DISABLED;
}


void gate_set_ratio(gate_ratio_t ratio, bool block_wait) {
    if (gate_handed != NULL) {
        gate_handed->set_ratio(ratio, block_wait);
    }
}
