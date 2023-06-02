#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>
#include <u8g2.h>
#include <math.h>

#include "app.h"
#include "FloatRingBuffer.h"
#include "rotary_button.h"
#include "display.h"
#include "scale.h"
#include "motors.h"
#include "charge_mode.h"
#include "eeprom.h"


uint8_t charge_weight_digits[] = {0, 0, 0, 0};

// PID related
charge_mode_config_t charge_mode_config;

const eeprom_charge_mode_data_t default_charge_mode_data = {
    .charge_mode_data_rev = EEPROM_CHARGE_MODE_DATA_REV,
    .coarse_kp = 0.2f,
    .coarse_ki = 0.0f,
    .coarse_kd = 1.0f,

    .fine_kp = 5.0f,
    .fine_ki = 0.0f,
    .fine_kd = 20.0f,

    .error_margin_grain = 0.03,
    .zero_sd_margin_grain = 0.02,
    .zero_mean_stability_grain = 0.04,
};

// Configures
TaskHandle_t scale_measurement_render_task_handler = NULL;
static char title_string[30];



typedef enum {
    CHARGE_MODE_WAIT_FOR_ZERO,
    CHARGE_MODE_WAIT_FOR_COMPLETE,
    CHARGE_MODE_WAIT_FOR_CUP_REMOVAL,
    CHARGE_MODE_WAIT_FOR_CUP_RETURN,
    CHARGE_MODE_EXIT,

} ChargeModeState_t;


void scale_measurement_render_task(void *p) {
    char current_weight_string[5];
    
    u8g2_t * display_handler = get_display_handler();

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        u8g2_ClearBuffer(display_handler);
        // Draw title
        if (strlen(title_string)) {
            u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
            u8g2_DrawStr(display_handler, 5, 10, title_string);
        }

        // Draw line
        u8g2_DrawHLine(display_handler, 0, 13, u8g2_GetDisplayWidth(display_handler));

        // current weight (only show values > -10)
        memset(current_weight_string, 0x0, sizeof(current_weight_string));
        float scale_measurement = scale_get_current_measurement();
        if (scale_measurement > -10) {
            sprintf(current_weight_string, "%0.02f", scale_measurement);
        }
        else {
            strcpy(current_weight_string, "---");
        }
        

        u8g2_SetFont(display_handler, u8g2_font_profont22_tf);
        u8g2_DrawStr(display_handler, 26, 35, current_weight_string);

        // print unit (short)
        const char * scale_unit_string = get_scale_unit_string(true);
        u8g2_SetFont(display_handler, u8g2_font_helvR08_tr);
        u8g2_DrawStr(display_handler, 96, 35, scale_unit_string);

        u8g2_SendBuffer(display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


ChargeModeState_t charge_mode_wait_for_zero(ChargeModeState_t prev_state) {
    // Wait for 5 measurements and wait for stable
    FloatRingBuffer data_buffer(10);

    // Update current status
    snprintf(title_string, sizeof(title_string), "Waiting for Zero");

    // Stop condition: 10 stable measurements in 200ms apart (2 seconds minimum)
    while (true) {
        TickType_t last_measurement_tick = xTaskGetTickCount();

        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            return CHARGE_MODE_EXIT;
        }
        else if (button_encoder_event == BUTTON_ENCODER_PRESSED) {
            scale_press_re_zero_key();
        }

        // Perform measurement
        float current_measurement = scale_block_wait_for_next_measurement();

        data_buffer.enqueue(current_measurement);

        // Generate stop condition
        if (data_buffer.getCounter() >= 10){
            if (data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.zero_sd_margin_grain && 
                data_buffer.getMean() < charge_mode_config.eeprom_charge_mode_data.zero_mean_stability_grain) {
                break;
            }
        }

        // Wait for 200 for next measurement
        vTaskDelayUntil(&last_measurement_tick, pdMS_TO_TICKS(300));
    }

    return CHARGE_MODE_WAIT_FOR_COMPLETE;
}

ChargeModeState_t charge_mode_wait_for_complete(ChargeModeState_t prev_state) {
    // Update current status
    snprintf(title_string, sizeof(title_string), "Target: %.02f gr", charge_mode_config.target_charge_weight);

    uint16_t coarse_trickler_max_speed = get_motor_max_speed(SELECT_COARSE_TRICKLER_MOTOR);
    uint16_t fine_trickler_max_speed = get_motor_max_speed(SELECT_FINE_TRICKLER_MOTOR);

    float integral = 0.0f;
    float last_error = 0.0f;

    TickType_t last_sample_tick = xTaskGetTickCount();
    TickType_t current_sample_tick = last_sample_tick;
    bool should_coarse_trickler_move = true;

    while (true) {
        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            return CHARGE_MODE_EXIT;
        }

        // Run the PID controlled loop to start charging
        // Perform the measurement
        float current_weight = scale_block_wait_for_next_measurement();
        current_sample_tick = xTaskGetTickCount();

        float error = charge_mode_config.target_charge_weight - current_weight;

        // Stop condition
        if (error < 0 || abs(error) < charge_mode_config.eeprom_charge_mode_data.error_margin_grain) {
            // Stop all motors
            motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, 0);
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

            break;
        }

        // Coarse trickler move condition
        if (abs(error) < 5.0f && should_coarse_trickler_move) {
            should_coarse_trickler_move = false;
            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, 0);

            // TODO: When tuning off the coarse trickler, also move reverse to back off some powder
        }

        // Update PID variables
        float elapse_time_ms = (current_sample_tick - last_sample_tick) / portTICK_RATE_MS;
        integral += error;
        float derivative = (error - last_error) / elapse_time_ms;

        // Update fine trickler speed
        float new_p = charge_mode_config.eeprom_charge_mode_data.fine_kp * error;
        float new_i = charge_mode_config.eeprom_charge_mode_data.fine_ki * integral;
        float new_d = charge_mode_config.eeprom_charge_mode_data.fine_kd * derivative;
        float new_speed = fmax(0.1, fmin(round(new_p + new_i + new_d), fine_trickler_max_speed));

        motor_set_speed(SELECT_FINE_TRICKLER_MOTOR, new_speed);

        // Update coarse trickler speed
        if (should_coarse_trickler_move) {
            new_p = charge_mode_config.eeprom_charge_mode_data.coarse_kp * error;
            new_i = charge_mode_config.eeprom_charge_mode_data.coarse_ki * integral;
            new_d = charge_mode_config.eeprom_charge_mode_data.coarse_kd * derivative;

            new_speed = fmin(round(new_p + new_i + new_d), coarse_trickler_max_speed);

            motor_set_speed(SELECT_COARSE_TRICKLER_MOTOR, new_speed);
        }

        // Record state
        last_sample_tick = current_sample_tick;
        last_error = error;
    }


    return CHARGE_MODE_WAIT_FOR_CUP_REMOVAL;
}

ChargeModeState_t charge_mode_wait_for_cup_removal(ChargeModeState_t prev_state) {
    // Update current status
    snprintf(title_string, sizeof(title_string), "Remove Cup", charge_mode_config.target_charge_weight);

    FloatRingBuffer data_buffer(5);

    // Stop condition: 5 stable measurements in 300ms apart (1.5 seconds minimum)
    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            return CHARGE_MODE_EXIT;
        }

        // Perform measurement
        float current_weight = scale_block_wait_for_next_measurement();
        data_buffer.enqueue(current_weight);

        // Generate stop condition
        if (data_buffer.getCounter() >= 5) {
            if (data_buffer.getSd() < charge_mode_config.eeprom_charge_mode_data.error_margin_grain && 
                data_buffer.getMean() + 10 < charge_mode_config.eeprom_charge_mode_data.zero_mean_stability_grain){
                break;
            }
        }

        // Wait for 600 for next measurement
        vTaskDelayUntil(&last_sample_tick, pdMS_TO_TICKS(300));
    }

    return CHARGE_MODE_WAIT_FOR_CUP_RETURN;
}

ChargeModeState_t charge_mode_wait_for_cup_return(ChargeModeState_t prev_state) { 
    snprintf(title_string, sizeof(title_string), "Return Cup", charge_mode_config.target_charge_weight);

    FloatRingBuffer data_buffer(5);

    while (true) {
        TickType_t last_sample_tick = xTaskGetTickCount();

        // Non block waiting for the input
        ButtonEncoderEvent_t button_encoder_event = button_wait_for_input(false);
        if (button_encoder_event == BUTTON_RST_PRESSED) {
            return CHARGE_MODE_EXIT;
        }
        else if (button_encoder_event == BUTTON_ENCODER_PRESSED) {
            scale_press_re_zero_key();
        }

        // Perform measurement
        float current_weight = scale_block_wait_for_next_measurement();
        if (current_weight >= 0) {
            break;
        }

        // Wait for 600 for next measurement
        vTaskDelayUntil(&last_sample_tick, pdMS_TO_TICKS(20));
    }

    return CHARGE_MODE_WAIT_FOR_ZERO;
}


uint8_t charge_mode_menu() {
    // Create target weight
    charge_mode_config.target_charge_weight = charge_weight_digits[3] * 10 + \
                                              charge_weight_digits[2] + \
                                              charge_weight_digits[1] * 0.1 + \
                                              charge_weight_digits[0] * 0.01;
    printf("Target Charge Weight: %f\n", charge_mode_config.target_charge_weight);

    // If the display task is never created then we shall create one, otherwise we shall resume the task
    if (scale_measurement_render_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(scale_measurement_render_task, "Scale Measurement Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &scale_measurement_render_task_handler);
    }
    else {
        vTaskResume(scale_measurement_render_task_handler);
    }

    // Enable motor on entering the charge mode
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, true);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, true);
    
    ChargeModeState_t state = CHARGE_MODE_WAIT_FOR_ZERO;

    bool quit = false;
    while (quit == false) {
        switch (state) {
            case CHARGE_MODE_WAIT_FOR_ZERO:
                state = charge_mode_wait_for_zero(state);
                break;
            case CHARGE_MODE_WAIT_FOR_COMPLETE:
                state = charge_mode_wait_for_complete(state);
                break;
            case CHARGE_MODE_WAIT_FOR_CUP_REMOVAL:
                state = charge_mode_wait_for_cup_removal(state);
                break;
            case CHARGE_MODE_WAIT_FOR_CUP_RETURN:
                state = charge_mode_wait_for_cup_return(state);
                break;
            case CHARGE_MODE_EXIT:
            default:
                quit = true;
                break;
        }

    }
    
    // // Wait for user input
    // ButtonEncoderEvent_t button_encoder_event;
    // while (true) {
    //     while (xQueueReceive(encoder_event_queue, &button_encoder_event, pdMS_TO_TICKS(20))){
    //         printf("%d\n", button_encoder_event);
    //     }
    // }
    
    // vTaskDelete(scale_measurement_render_handler);
    vTaskSuspend(scale_measurement_render_task_handler);

    // Diable motors on exiting the mode
    motor_enable(SELECT_COARSE_TRICKLER_MOTOR, false);
    motor_enable(SELECT_FINE_TRICKLER_MOTOR, false);

    return 1;  // return back to main menu
}


bool charge_mode_config_init(void) {
    bool is_ok = true;

    // Read charge mode config from EEPROM
    memset(&charge_mode_config, 0x0, sizeof(charge_mode_config));
    is_ok = eeprom_read(EEPROM_CHARGE_MODE_BASE_ADDR, (uint8_t *)&charge_mode_config.eeprom_charge_mode_data, sizeof(eeprom_charge_mode_data_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_CHARGE_MODE_BASE_ADDR);
        return false;
    }

    if (charge_mode_config.eeprom_charge_mode_data.charge_mode_data_rev != EEPROM_CHARGE_MODE_DATA_REV) {
        // charge_mode_config.eeprom_charge_mode_data.charge_mode_data_rev = EEPROM_CHARGE_MODE_DATA_REV;

        memcpy(&charge_mode_config.eeprom_charge_mode_data, &default_charge_mode_data, sizeof(eeprom_charge_mode_data_t));

        // Write back
        is_ok = eeprom_write(EEPROM_CHARGE_MODE_BASE_ADDR, (uint8_t *) &charge_mode_config.eeprom_charge_mode_data, sizeof(eeprom_charge_mode_data_t));
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_CHARGE_MODE_BASE_ADDR);
            return false;
        }
    }

    return true;
}


bool charge_mode_config_save(void) {
    bool is_ok;
    is_ok = eeprom_write(EEPROM_CHARGE_MODE_BASE_ADDR, (uint8_t *) &charge_mode_config.eeprom_charge_mode_data, sizeof(eeprom_charge_mode_data_t));
    return is_ok;
}



bool http_rest_charge_mode_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char charge_mode_json_buffer[256];

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "coarse_kp") == 0) {
            charge_mode_config.eeprom_charge_mode_data.coarse_kp = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "coarse_ki") == 0) {
            charge_mode_config.eeprom_charge_mode_data.coarse_ki = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "coarse_kd") == 0) {
            charge_mode_config.eeprom_charge_mode_data.coarse_kd = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "fine_kp") == 0) {
            charge_mode_config.eeprom_charge_mode_data.fine_kp = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "fine_ki") == 0) {
            charge_mode_config.eeprom_charge_mode_data.fine_ki = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "fine_kd") == 0) {
            charge_mode_config.eeprom_charge_mode_data.fine_kd = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "error_margin_grain") == 0) {
            charge_mode_config.eeprom_charge_mode_data.error_margin_grain = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "zero_sd_margin_grain") == 0) {
            charge_mode_config.eeprom_charge_mode_data.zero_sd_margin_grain = strtof(values[idx], NULL);
        }
        else if (strcmp(params[idx], "zero_mean_stability_grain") == 0) {
            charge_mode_config.eeprom_charge_mode_data.zero_mean_stability_grain = strtof(values[idx], NULL);
        }
    }

    // Response
    snprintf(charge_mode_json_buffer, 
             sizeof(charge_mode_json_buffer),
             "{\"coarse_kp\":%f,\"coarse_ki\":%f,\"coarse_kd\":%f,\"fine_kp\":%f,\"fine_ki\":%f,\"fine_kd\":%f,\"error_margin_grain\":%f,\"zero_sd_margin_grain\":%f,\"zero_mean_stability_grain\":%f}",
             charge_mode_config.eeprom_charge_mode_data.coarse_kp,
             charge_mode_config.eeprom_charge_mode_data.coarse_ki,
             charge_mode_config.eeprom_charge_mode_data.coarse_kd,
             charge_mode_config.eeprom_charge_mode_data.fine_kp,
             charge_mode_config.eeprom_charge_mode_data.fine_ki,
             charge_mode_config.eeprom_charge_mode_data.fine_kd,
             charge_mode_config.eeprom_charge_mode_data.error_margin_grain,
             charge_mode_config.eeprom_charge_mode_data.zero_sd_margin_grain,
             charge_mode_config.eeprom_charge_mode_data.zero_mean_stability_grain);

    size_t data_length = strlen(charge_mode_json_buffer);
    file->data = charge_mode_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_charge_mode_setpoint(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char charge_mode_json_buffer[64];

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "target_charge_weight") == 0) {
            float target_charge_weight = strtof(values[idx], NULL);
            charge_mode_config.target_charge_weight = target_charge_weight;
        }
    }

    // Response
    snprintf(charge_mode_json_buffer, 
             sizeof(charge_mode_json_buffer),
             "{\"target_charge_weight\":%0.3f}",
             charge_mode_config.target_charge_weight);

    size_t data_length = strlen(charge_mode_json_buffer);
    file->data = charge_mode_json_buffer;
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}