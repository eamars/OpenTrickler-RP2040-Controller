// The PC's handle on src/charge_pid_learnt.c (ADR-009, 2026-11-22 evening): plain arrays in, plain arrays out, so
// ctypes needs no copy of the structs (firmware_pid.py)
#include <math.h>
#include <stdlib.h>

#include "charge_pid_learnt.h"

typedef struct {
    charge_pid_t c;
    charge_pid_settings_t s;
    charge_pid_constants_t k;
    charge_pid_learning_t learning;
} shim_pid_t;


// settings: the 16 floats of charge_pid_settings_t in order; constants: the 10 floats then most_tops
void *shim_pid_new(const double *settings, const double *constants) {
    shim_pid_t *p = calloc(1, sizeof(*p));
    float *s = (float *) &p->s;
    for (int i = 0; i < 16; i++) {
        s[i] = (float) settings[i];
    }
    float *k = (float *) &p->k;
    for (int i = 0; i < 10; i++) {
        k[i] = (float) constants[i];
    }
    p->k.most_tops = (uint8_t) constants[10];
    return p;
}


void shim_pid_free(void *p) {
    free(p);
}


void shim_pid_start(void *handle, double target_gr, uint32_t now_ms) {
    shim_pid_t *p = handle;
    charge_pid_start(&p->c, &p->s, &p->k, &p->learning, (float) target_gr, now_ms);
}


void shim_pid_step(void *handle, uint32_t now_ms, double fine_turns) {
    shim_pid_t *p = handle;
    charge_pid_count(&p->c, (float) fine_turns);
    charge_pid_tick(&p->c, now_ms);
}


void shim_pid_reading(void *handle, uint32_t now_ms, double weight) {
    shim_pid_t *p = handle;
    charge_pid_reading(&p->c, now_ms, (float) weight);
}


// coarse_rps, fine_rps, done, result, first error (NaN if none), tops, gr_turn, rest_weight
void shim_pid_outputs(void *handle, double *out) {
    shim_pid_t *p = handle;
    out[0] = p->c.coarse_rps;
    out[1] = p->c.fine_rps;
    out[2] = p->c.done;
    out[3] = p->c.result;
    out[4] = p->c.have_first_error ? p->c.first_error_gr : (double) NAN;
    out[5] = p->c.tops;
    out[6] = p->learning.gr_turn;
    out[7] = p->c.rest_weight;
}
