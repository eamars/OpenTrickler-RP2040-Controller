#ifndef GATE_H_
#define GATE_H_
/*
 * The powder's gate, as charge mode, the cleanup, the menu and the API ask of one (ADR-006, gate 9). A build may carry
 * no gate: then none is handed over at start-up, and each call below answers as a unit with no gate (never enabled,
 * its state disabled, nothing moved). The servo gate is handed over by the start-up's wiring when the build carries
 * it (gate_set()); no module below it names the servo gate.
 *
 * Handed over once, before any task starts, and never changed.
 */
#include <stdbool.h>

// A gate's ratio: 0.0 open, 1.0 closed, any value between proportional; -1.0 disabled
typedef float gate_ratio_t;
#define GATE_RATIO_OPEN         (0.0f)
#define GATE_RATIO_CLOSED       (1.0f)
#define GATE_RATIO_DISABLED     (-1.0f)

typedef enum {
    GATE_DISABLED = 0,
    GATE_CLOSE,
    GATE_OPEN,
} gate_state_t;

// What a gate gives: each call as the one below of the same name
typedef struct {
    bool (*enabled)(void);
    gate_state_t (*state)(void);
    gate_ratio_t (*ratio)(void);
    void (*set_ratio)(gate_ratio_t ratio, bool block_wait);
} gate_t;

#ifdef __cplusplus
extern "C" {
#endif

void gate_set(const gate_t *gate);              // the build's gate, kept by its address
bool gate_present(void);                        // a gate was handed over: the build carries one
bool gate_enabled(void);                        // fitted and on in its settings: charge mode uses it
gate_state_t gate_state(void);
gate_ratio_t gate_ratio(void);
void gate_set_ratio(gate_ratio_t ratio, bool block_wait);

#ifdef __cplusplus
}
#endif

#endif  // GATE_H_
