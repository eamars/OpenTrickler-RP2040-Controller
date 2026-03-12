/**
 * @file app_state.h
 * @brief Application state variable shared between display types
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include "app.h"  // For AppState_t

#ifdef __cplusplus
extern "C" {
#endif

// Global exit state - used for mode transitions
// Defined in app_state.c
extern AppState_t exit_state;

#ifdef __cplusplus
}
#endif

#endif // APP_STATE_H
