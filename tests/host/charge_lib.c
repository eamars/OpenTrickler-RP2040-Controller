/*
 * The charge controller and the tuner (src/charge_controller.c, src/charge_tuner.c) as a library for the PC, so the
 * simulation can run the firmware's own C (tests/step2_sim.py): each entry point exported under its own name, the
 * controller and the tuner held by the library.
 */
#include <stdlib.h>

#include "charge_controller.h"
#include "charge_tuner.h"

#if defined(_WIN32)
#define CHARGE_LIB_EXPORT __declspec(dllexport)
#else
#define CHARGE_LIB_EXPORT __attribute__((visibility("default")))
#endif


// The controller and, beside it, the room each throw borrows
typedef struct {
    charge_controller_t controller;
    charge_room_t room;
} lib_charge_t;


CHARGE_LIB_EXPORT charge_controller_t *lib_charge_new(void) {
    lib_charge_t *charge = calloc(1, sizeof(lib_charge_t));
    charge_controller_init(&charge->controller);
    return &charge->controller;
}


CHARGE_LIB_EXPORT void lib_charge_free(charge_controller_t *controller) {
    free(controller);
}


// The controller's own size, and the room a throw borrows
CHARGE_LIB_EXPORT int lib_charge_size(int *room) {
    *room = (int) sizeof(charge_room_t);
    return (int) sizeof(charge_controller_t);
}


CHARGE_LIB_EXPORT void lib_charge_config(charge_controller_t *controller, const charge_config_t *config) {
    controller->config = *config;
}


CHARGE_LIB_EXPORT int lib_charge_start(charge_controller_t *controller, double target, double now_ms) {
    return charge_controller_start(controller, &((lib_charge_t *) controller)->room, target, now_ms);
}


CHARGE_LIB_EXPORT void lib_charge_tick(charge_controller_t *controller, double now_ms) {
    charge_controller_tick(controller, now_ms);
}


CHARGE_LIB_EXPORT void lib_charge_reading(charge_controller_t *controller, double now_ms, double weight) {
    charge_controller_reading(controller, now_ms, weight);
}


// What it gives back: each tube's speed and whether it has stopped
CHARGE_LIB_EXPORT int lib_charge_out(const charge_controller_t *controller, double *coarse_rps, double *fine_rps) {
    *coarse_rps = controller->coarse_rps;
    *fine_rps = controller->fine_rps;
    return controller->done;
}


// What it holds, for the judge: the weight a turn it takes and the turns in the air, the room it used, and whether
// the throw was cut
CHARGE_LIB_EXPORT int lib_charge_state(const charge_controller_t *controller, double *k, double *unseen,
                                       int *parcels_most, int *rows, int *cut) {
    *k = controller->k;
    *unseen = controller->unseen;
    *parcels_most = controller->parcels_most;
    *rows = controller->row_count;
    *cut = controller->fit_cut;
    return controller->k_known;
}


// Whether the throw stopped starved (an empty hopper), for the judge
CHARGE_LIB_EXPORT int lib_charge_starved(const charge_controller_t *controller) {
    return controller->starved;
}


CHARGE_LIB_EXPORT double lib_charge_verdict_after_s(const charge_controller_t *controller) {
    return charge_controller_verdict_after_s(controller);
}


CHARGE_LIB_EXPORT void lib_charge_at_rest(charge_controller_t *controller, double reading) {
    charge_controller_at_rest(controller, reading);
}


CHARGE_LIB_EXPORT int lib_charge_carried(const charge_controller_t *controller, double *k) {
    return charge_controller_carried(controller, k);
}


CHARGE_LIB_EXPORT charge_tuner_t *lib_tuner_new(double tolerance) {
    charge_tuner_t *tuner = calloc(1, sizeof(charge_tuner_t));
    charge_tuner_init(tuner, tolerance);
    return tuner;
}


CHARGE_LIB_EXPORT void lib_tuner_free(charge_tuner_t *tuner) {
    free(tuner);
}


CHARGE_LIB_EXPORT int lib_tuner_throw(charge_tuner_t *tuner, double kp, double kd, double coarse_kp,
                                      double k, double coarse_k, double lag, double error, double time_s,
                                      double *next_kp, double *next_kd, double *next_coarse_kp) {
    return (int) charge_tuner_throw(tuner, kp, kd, coarse_kp, k, coarse_k, lag, error, time_s, next_kp, next_kd,
                                    next_coarse_kp);
}
