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

#ifdef __cplusplus
}
#endif

#endif  // MENU_H_