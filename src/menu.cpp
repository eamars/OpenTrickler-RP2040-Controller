#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include "app.h"
#include "u8g2.h"
#include "configuration.h"
#include "rotary_button.h"

#include "mui.h"
#include "mui_u8g2.h"


// External variables
extern u8g2_t display_handler;
extern QueueHandle_t encoder_event_queue;
extern muif_t muif_list[];
extern fds_t fds_data[];
extern const size_t muif_cnt;

// Local variables
uint8_t charge_weight_digits[] = {0, 0, 0, 0};
AppState_t exit_state = APP_STATE_DEFAULT;


void menu_task(void *p){
    // Create UI element
    mui_t mui;

    mui_Init(&mui, &display_handler, fds_data, muif_list, muif_cnt);
    mui_GotoForm(&mui, 0, 0);

    u8g2_FirstPage(&display_handler);
    do {
        mui_Draw(&mui);
    } while ( u8g2_NextPage(&display_handler) );

    while (true) {
        if (mui_IsFormActive(&mui)) {
            ButtonEncoderEvent_t button_encoder_event;
            while (xQueueReceive(encoder_event_queue, &button_encoder_event, pdMS_TO_TICKS(20))){
                if (button_encoder_event == BUTTON_ENCODER_ROTATE_CW) {
                    printf("Next Selected\n");
                    mui_NextField(&mui);
                }
                else if (button_encoder_event == BUTTON_ENCODER_ROTATE_CCW) {
                    printf("Prev Selected\n");
                    mui_PrevField(&mui);
                }
                else if (button_encoder_event == BUTTON_ENCODER_PRESSED) {
                    printf("Item Selected\n");
                    mui_SendSelect(&mui);
                }
            }

            // If active then draw the menu
            u8g2_FirstPage(&display_handler);
            do {
                mui_Draw(&mui);
            } while ( u8g2_NextPage(&display_handler) );
        }
        else {
            // menu is not active, leave the control to the app
            printf("exit state: %d\n", exit_state);
            switch (exit_state) {
                case APP_STATE_ENTER_CHARGE_MODE:
                    exit_state = APP_STATE_DEFAULT;
                    break;
                default:
                    mui_GotoForm(&mui, 0, 0);
                    break;
            }
        }
    }
}
