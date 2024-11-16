#include <FreeRTOS.h>
#include <task.h>
#include <u8g2.h>
#include <string.h>

#include "scale.h"
#include "app.h"
#include "display.h"
#include "mini_12864_module.h"
#include "common.h"


// Below steps are refering to Section 8-5 of the FX_FZ-i instruction manual
// typedef enum {
//     CALIBRATE_STEP_1,
// }


char title_string[32] = "";
char line1[32] = "";
char line2[32] = "";
bool show_next_key = false;

TaskHandle_t scale_calibration_render_task_handler = NULL;


extern void scale_press_cal_key();
extern void scale_press_print_key();


void scale_calibration_render_task(void *p) {
    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();
        u8g2_t * display_handler = get_display_handler();

        u8g2_ClearBuffer(display_handler);

        // Draw title
        if (strlen(title_string)) {
            u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
            u8g2_DrawStr(display_handler, 5, 10, title_string);
        }

        // Draw line
        u8g2_DrawHLine(display_handler, 0, 13, u8g2_GetDisplayWidth(display_handler));

        u8g2_SetFont(display_handler, u8g2_font_helvR08_tr);
        // Draw line 1
        if (strlen(line1)) {
            u8g2_DrawStr(display_handler, 5, 25, line1);
        }

        // Draw line 2
        if (strlen(line2)) {
            u8g2_DrawStr(display_handler, 5, 37, line2);
        }

        // Draw a button
        if (show_next_key) {
            u8g2_DrawButtonUTF8(display_handler, 64, 59, U8G2_BTN_HCENTER | U8G2_BTN_INV | U8G2_BTN_BW1, 0, 1, 1, "Next");
        }

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


uint8_t scale_calibrate_with_external_weight() {
    if (scale_calibration_render_task_handler == NULL) {
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(scale_calibration_render_task, "Scale Measurement Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &scale_calibration_render_task_handler);
    }
    else {
        vTaskResume(scale_calibration_render_task_handler);
    }

    BaseType_t scheduler_state = xTaskGetSchedulerState();

    // Step 1: Enter CAL mode by pressing CAL key
    // Displays CAL 0
    strcpy(title_string, "Step 1");
    strcpy(line1, "Wait 3s");
    memset(line2, 0x0, sizeof(line2));
    scale_press_cal_key();
    delay_ms(3000, scheduler_state);  // Wait for 3 seconds

    // Step 2a: Confirm the weight (assume it was calibrated before) and measure Zero
    strcpy(title_string, "Step 2");
    strcpy(line1, "Confirm pan is empty");
    strcpy(line2, "Press Next to continue");
    show_next_key = true;
    while (button_wait_for_input(true) != BUTTON_ENCODER_PRESSED) {
        ;
    }
    show_next_key = false;
    scale_press_print_key();

    // Step 2b update screen and prompt to wait
    strcpy(line1, "Wait 5s");
    memset(line2, 0x0, sizeof(line2));
    delay_ms(5000, scheduler_state);  // Wait for 10 seconds

    // Step 3a: Place the specified weight to the scale, wait for input
    strcpy(title_string, "Step 3");
    strcpy(line1, "Place displayed weight");
    strcpy(line2, "Press Next to continue");

    show_next_key = true;
    while (button_wait_for_input(true) != BUTTON_ENCODER_PRESSED) {
        ;
    }
    show_next_key = false;
    scale_press_print_key();

    // Step 3b update screen and prompt to wait
    strcpy(line1, "Wait 5s");
    memset(line2, 0x0, sizeof(line2));
    delay_ms(5000, scheduler_state);  // Wait for 10 seconds

    strcpy(title_string, "Step 4");
    strcpy(line1, "Remove the weight");
    strcpy(line2, "Press Next to continue");

    show_next_key = true;
    while (button_wait_for_input(true) != BUTTON_ENCODER_PRESSED) {
        ;
    }
    show_next_key = false;

    // Step 5 calibration done
    strcpy(title_string, "Calibration done");
    strcpy(line1, "Return to main menu in");
    strcpy(line2, "3 seconds");

    delay_ms(3000, scheduler_state);  // Wait for 3 seconds
    

    vTaskSuspend(scale_calibration_render_task_handler);

    return 31;  // Returns to scale page
}



