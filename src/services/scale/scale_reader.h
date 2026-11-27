#ifndef SCALE_READER_H_
#define SCALE_READER_H_

#include "scale_make.h"

/*
    The scale's one reader (ADR-017, gate 2): a task on the scale's serial port that waits for each byte, finds the
    running make's frames and publishes what they read. Its argument is the make (const scale_make_t *).
*/
void scale_reader_task(void *make);

// The port was drained while the reader stood (the simulated scale's hand-over): its next frame is found afresh
void scale_reader_restart(void);

#endif  // SCALE_READER_H_
