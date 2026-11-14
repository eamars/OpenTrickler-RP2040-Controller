#ifndef MENU_H_
#define MENU_H_

// C++ function
void menu_task(void *p);

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The form on screen, as menu_task last drew it; -1 while a mode owns the screen or before the menu starts.
 */
int menu_get_current_form_id(void);

/**
 * @brief Ask menu_task to show a form, and wait until it has drawn (ADR-001). Call from another task.
 * @param form_id the MUI form.
 * @param found set to whether the form exists; when it does not, the screen stays as it was.
 * @return false when the menu does not have the screen (a mode has it) or did not answer within a second.
 */
bool menu_goto_form(uint8_t form_id, bool *found);

/**
 * @brief The main menu's title: "OpenTrickler", or the motor driver that is not OK (ADR-012, step 6).
 */
const char * menu_main_title(void);

/**
 * @brief The screen for charge or cleanup asked for with a motor driver not OK (ADR-012, step 6): the driver named,
 *        and that nothing starts. Waits for a press. Call from the menu task, which owns the screen then.
 * @return the form to go back to: the main menu.
 */
uint8_t menu_motor_fault_view(void);

#ifdef __cplusplus
}
#endif

#endif  // MENU_H_