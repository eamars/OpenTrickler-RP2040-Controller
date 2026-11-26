/*
 * The calibration's method (src/services/calibration_method/calibration_method.c) as a library for the PC, so the simulation can run the
 * firmware's own C (tests/calibration_sim.py): each entry point exported under its own name.
 */
#include "calibration_method.h"

#if defined(_WIN32)
#define CALIBRATION_LIB_EXPORT __declspec(dllexport)
#else
#define CALIBRATION_LIB_EXPORT __attribute__((visibility("default")))
#endif


CALIBRATION_LIB_EXPORT void lib_calibration_cup(float capacity, float held_now) {
    calibration_cup(capacity, held_now);
}


CALIBRATION_LIB_EXPORT int lib_calibration_scale_part(const calibration_bench_t *bench,
                                                      calibration_scale_record_t *record) {
    return (int) calibration_scale_part(bench, record);
}


CALIBRATION_LIB_EXPORT int lib_calibration_speeds_part(const calibration_bench_t *bench, int tube, float least_rps,
                                                       float most_rps, const calibration_scale_record_t *scale,
                                                       calibration_speeds_record_t *record) {
    return (int) calibration_speeds_part(bench, tube, least_rps, most_rps, scale, record);
}
