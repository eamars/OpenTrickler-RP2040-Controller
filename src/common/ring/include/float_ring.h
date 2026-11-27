#ifndef FLOAT_RING_H_
#define FLOAT_RING_H_

#include <stddef.h>

/*
    The newest readings, a fixed number of them, and what the charge asks of them: their mean and their standard
    deviation, to tell a steady scale.

    The ring keeps no array of its own: its user gives one, as long-lived as the ring (on its stack, for a ring made
    inside a function). Nothing is taken from the heap.
*/
typedef struct {
    float *data;            // the user's array
    size_t size;            // its length
    size_t write_at;        // where the next reading goes
    size_t count;           // the readings added, up to size
} float_ring_t;


#ifdef __cplusplus
extern "C" {
#endif

// Start an empty ring over the user's array of that many floats, which it zeroes
void float_ring_init(float_ring_t *ring, float *data, size_t size);

// Add a reading, over the oldest once the ring is full
void float_ring_add(float_ring_t *ring, float value);

// The readings added, up to the ring's size: the ring is full when it is the size
size_t float_ring_count(const float_ring_t *ring);

// The mean and the standard deviation of the whole array. Ask once the ring is full: before, the readings not yet
// added count as zeros.
float float_ring_mean(const float_ring_t *ring);
float float_ring_sd(const float_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif  // FLOAT_RING_H_
