/**
 * @file app_state.c
 * @brief Application state variable shared between display types
 */

#include "app_state.h"

// Global exit state - used for mode transitions
AppState_t exit_state = APP_STATE_DEFAULT;
