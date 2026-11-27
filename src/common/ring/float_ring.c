#include <math.h>
#include <string.h>

#include "float_ring.h"


void float_ring_init(float_ring_t *ring, float *data, size_t size) {
    ring->data = data;
    ring->size = size;
    ring->write_at = 0;
    ring->count = 0;
    memset(data, 0, size * sizeof(float));
}


void float_ring_add(float_ring_t *ring, float value) {
    ring->data[ring->write_at++] = value;
    ring->write_at %= ring->size;

    if (ring->count < ring->size) {
        ring->count++;
    }
}


size_t float_ring_count(const float_ring_t *ring) {
    return ring->count;
}


// The sum in double, as the mean and the deviation are worked: a float sum of ten readings would lose their last digit
static double float_ring_sum(const float_ring_t *ring) {
    double sum = 0.0;

    for (size_t idx = 0; idx < ring->size; idx++) {
        sum += ring->data[idx];
    }

    return sum;
}


float float_ring_mean(const float_ring_t *ring) {
    return float_ring_sum(ring) / ring->size;
}


float float_ring_sd(const float_ring_t *ring) {
    float mean = float_ring_mean(ring);
    double sum_of_squares = 0.0;

    for (size_t idx = 0; idx < ring->size; idx++) {
        sum_of_squares += pow(ring->data[idx] - mean, 2);
    }

    return sqrt(sum_of_squares / ring->size);
}
