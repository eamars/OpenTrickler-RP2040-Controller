/*
    What the simulation needs of the firmware's charge controller beyond its header (ADR-009, gate 1): the sizes of
    its structs, so the Python side can check its copies of them, and the controller's outputs in one call.
    Built with src/charge_control.c into a library by tools/charge_sim/firmware_controller.py. Not in the firmware.
*/

#include <stddef.h>

#include "charge_control.h"

size_t shim_sizeof_controller(void) {
    return sizeof(charge_control_t);
}

size_t shim_sizeof_calibration(void) {
    return sizeof(charge_control_calibration_t);
}

size_t shim_sizeof_constants(void) {
    return sizeof(charge_control_constants_t);
}

// The controller's outputs and state, as doubles whatever its precision, in this order: coarse_rps, fine_rps,
// move_turns, done, result, phase, rest_weight, stop_room_gr, have_first_error, first_error_gr, moves, streams;
// then for a test: unseen_gr, coarse_unseen_gr, reading, the turns and the speeds (coarse, fine)
void shim_outputs(const charge_control_t *c, double *out) {
    out[0] = c->coarse_rps;
    out[1] = c->fine_rps;
    out[2] = c->move_turns;
    out[3] = c->done ? 1.0 : 0.0;
    out[4] = (double) c->result;
    out[5] = (double) c->phase;
    out[6] = c->rest_weight;
    out[7] = c->stop_room_gr;
    out[8] = c->have_first_error ? 1.0 : 0.0;
    out[9] = c->first_error_gr;
    out[10] = (double) c->moves;
    out[11] = (double) c->streams;
    out[12] = c->unseen_gr;
    out[13] = c->coarse_unseen_gr;
    out[14] = c->reading;
    out[15] = c->turned[0];
    out[16] = c->turned[1];
    out[17] = c->speed[0];
    out[18] = c->speed[1];
}

// The fine tube's learning carried from another controller (the check's lockstep: a throw starts from the
// reference's learnt state, as it starts from its learnt stop)
void shim_set_fine_learning(charge_control_t *c, double n, double sum, double current) {
    c->fine_learn_n = (uint16_t) n;
    c->fine_learn_sum = (charge_control_real_t) sum;
    c->cal.fine_gr_turn = (charge_control_real_t) current;
}

// A tube's parcels on their way (0 coarse, 1 fine), for a test: their count, and each one's due time and grains
uint16_t shim_parcels(const charge_control_t *c, int tube, double *due_s, double *grains) {
    for (uint16_t i = 0; i < c->parcel_count[tube]; i++) {
        due_s[i] = c->parcels[tube][i].due_s;
        grains[i] = c->parcels[tube][i].grains;
    }
    return c->parcel_count[tube];
}
