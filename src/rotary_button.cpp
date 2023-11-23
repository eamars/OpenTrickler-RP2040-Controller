// 
//            -----
//        5V |10  9 | GND
//        -- | 8  7 | --
//    (DIN)  | 6  5#| (RESET)
//  (LCD_A0) | 4  3 | (LCD_CS)
// (BTN_ENC) | 2  1 | --
//            ------
//             EXP1
//            -----
//        5V |10  9 | GND
//   (RESET) | 8  7 | --
//   (MOSI)  | 6  5#| (EN2)
//        -- | 4  3 | (EN1)
//  (LCD_SCK)| 2  1 | --
//            ------
//             EXP2
// 
// For Pico W
// EXP1_2 (BTN_ENC) <-> PIN29 (GP22)
// EXP2_5 (ENCODER2) <-> PIN19 (GP14)
// EXP2_3 (ENCODER1) <-> PIN20 (GP15)
// EXP2_8 (BTN_RST) <-> PIN16 (GP12)

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include <FreeRTOS.h>
#include <queue.h>
#include "configuration.h"
#include "gpio_irq_handler.h"
#include "rotary_button.h"
#include "http_rest.h"
#include "eeprom.h"
#include "common.h"


// Configs
rotary_button_config_t rotary_button_config;

// Statics (to be shared between IRQ and tasks)
QueueHandle_t encoder_event_queue = NULL;


void _isr_on_encoder_update(uint gpio, uint32_t event){
    static uint8_t state = 2;
    static int8_t count = 0;

    bool en1 = gpio_get(BUTTON0_ENCODER_PIN1);
    bool en2 = gpio_get(BUTTON0_ENCODER_PIN2);

    switch (state) {
        case 0: {
            if (en1) {
                count += 1;
                state = 1;
            }
            else if (en2) {
                count -= 1;
                state = 3;
            }
            break;
        }
        case 1: {
            if (!en1) {
                count -= 1;
                state = 0;
            }
            else if (en2) {
                count += 1;
                state = 2;
            }
            break;
        }
        case 2: {
            if (!en1) {
                count += 1;
                state = 3;
            }
            else if (!en2) {
                count -= 1;
                state = 1;
            }
            break;
        }
        case 3: {
            if (!en1) {
                count += 1;
                state = 0;
            }
            else if (en2) {
                count -= 1;
                state = 2;
            }
            break;
        }
        default:
            break;
    }

    ButtonEncoderEvent_t button_encoder_event;
    if (count >= 4) {
        count = 0;

        if (rotary_button_config.inverted_encoder_direction) {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CCW;
        }
        else {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CW;
        }
        
        if (encoder_event_queue) {
            xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
        }
    }
    else if (count <= -4) {
        count = 0;

        if (rotary_button_config.inverted_encoder_direction) {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CW;
        }
        else {
            button_encoder_event = BUTTON_ENCODER_ROTATE_CCW;
        }

        if (encoder_event_queue) {
            xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
        }
    }
}

void _isr_on_button_enc_update(uint gpio, uint32_t event) {
    static TickType_t last_call_time = 0;
    const TickType_t debounce_timeout_ticks = pdMS_TO_TICKS(250);

    // Button debounce
    TickType_t current_call_time = xTaskGetTickCountFromISR();
     if ((current_call_time - last_call_time) > debounce_timeout_ticks) {
        ButtonEncoderEvent_t button_encoder_event = BUTTON_ENCODER_PRESSED;
        xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
    }
    last_call_time = current_call_time;
}

void _isr_on_button_rst_update(uint gpio, uint32_t event) {
    static TickType_t last_call_time = 0;
    const TickType_t debounce_timeout_ticks = pdMS_TO_TICKS(250);

    // Button debounce
    TickType_t current_call_time = xTaskGetTickCountFromISR();
    if ((current_call_time - last_call_time) > debounce_timeout_ticks) {
        ButtonEncoderEvent_t button_encoder_event = BUTTON_RST_PRESSED;
        xQueueSendFromISR(encoder_event_queue, &button_encoder_event, NULL);
    }
    last_call_time = current_call_time;
}
  

bool button_init() {
    printf("Initializing Button Task -- ");

    bool is_ok;

    // Read configuration
    memset(&rotary_button_config, 0x0, sizeof(rotary_button_config_t));
    is_ok = eeprom_read(EEPROM_ROTARY_BUTTON_CONFIG_BASE_ADDR, (uint8_t *)&rotary_button_config, sizeof(rotary_button_config_t));
    if (!is_ok) {
        printf("Unable to read from EEPROM at address %x\n", EEPROM_CHARGE_MODE_BASE_ADDR);
        return false;
    }

    if (rotary_button_config.rotary_button_data_rev != EEPROM_ROTARY_BUTTON_DATA_REV) {
        rotary_button_config.rotary_button_data_rev = EEPROM_ROTARY_BUTTON_DATA_REV;

        // Set default
        rotary_button_config.inverted_encoder_direction = false;

        // Write back
        is_ok = button_config_save();
        if (!is_ok) {
            printf("Unable to write to %x\n", EEPROM_ROTARY_BUTTON_CONFIG_BASE_ADDR);
            return false;
        }
    }

    // Configure button encoder
    gpio_init(BUTTON0_ENCODER_PIN1);
    gpio_set_dir(BUTTON0_ENCODER_PIN1, GPIO_IN);
    gpio_pull_up(BUTTON0_ENCODER_PIN1);

    gpio_init(BUTTON0_ENCODER_PIN2);
    gpio_set_dir(BUTTON0_ENCODER_PIN2, GPIO_IN);
    gpio_pull_up(BUTTON0_ENCODER_PIN2);

    // Configure button encoder
    gpio_init(BUTTON0_ENC_PIN);
    gpio_set_dir(BUTTON0_ENC_PIN, GPIO_IN);
    gpio_pull_up(BUTTON0_ENC_PIN);

    // // Configure button reset
    gpio_init(BUTTON0_RST_PIN);
    gpio_set_dir(BUTTON0_RST_PIN, GPIO_IN);
    gpio_pull_up(BUTTON0_RST_PIN);

    irq_handler.register_interrupt(BUTTON0_ENCODER_PIN1, gpio_irq_handler::irq_event::change, _isr_on_encoder_update);
    irq_handler.register_interrupt(BUTTON0_ENCODER_PIN2, gpio_irq_handler::irq_event::change, _isr_on_encoder_update);
    irq_handler.register_interrupt(BUTTON0_ENC_PIN, gpio_irq_handler::irq_event::fall, _isr_on_button_enc_update);
    irq_handler.register_interrupt(BUTTON0_RST_PIN, gpio_irq_handler::irq_event::fall, _isr_on_button_rst_update);

    encoder_event_queue = xQueueCreate(5, sizeof(ButtonEncoderEvent_t));
    if (encoder_event_queue == 0) {
        assert(false);
    }

    // Register to eeprom save all
    eeprom_register_handler(button_config_save);

    printf("done\n");

    return true;
}


bool button_config_save() {
    bool is_ok = eeprom_write(EEPROM_ROTARY_BUTTON_CONFIG_BASE_ADDR, (uint8_t *) &rotary_button_config, sizeof(rotary_button_config_t));
    return is_ok;
}


bool http_rest_button_control(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char button_control_json_buffer[256];
    memset(button_control_json_buffer, 0x0, sizeof(button_control_json_buffer));

    strcat(button_control_json_buffer, "{\"button_pressed\":[");

    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "CW") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_ENCODER_ROTATE_CW;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"CW\",");
            }
        }
        
        if (strcmp(params[idx], "CCW") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_ENCODER_ROTATE_CCW;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"CCW\",");
            }
        }

        if (strcmp(params[idx], "PRESS") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_ENCODER_PRESSED;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"PRESS\",");
            }
        }

        if (strcmp(params[idx], "RST") == 0) {
            if (strcmp(values[idx], "true") == 0){
                ButtonEncoderEvent_t button_event = BUTTON_RST_PRESSED;
                xQueueSend(encoder_event_queue, &button_event, 0);

                strcat(button_control_json_buffer, "\"RST\",");
            }
        }
    }

    // Remove trailing comma
    size_t len = strlen(button_control_json_buffer);
    if (button_control_json_buffer[len-1] == ',') {
        button_control_json_buffer[len-1] = 0;
    }

    strcat(button_control_json_buffer, "]}");

    // Send to client
    len = strlen(button_control_json_buffer);

    file->data = button_control_json_buffer;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_button_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    static char buf[128];

    // Control
    for (int idx = 0; idx < num_params; idx += 1) {
        if (strcmp(params[idx], "inv_dir") == 0) {
            if (strcmp(values[idx], "true") == 0){
                rotary_button_config.inverted_encoder_direction = true;
            }
            else if (strcmp(values[idx], "false") == 0) {
                rotary_button_config.inverted_encoder_direction = false;
            }
        }
    }

    // Response
    snprintf(buf, sizeof(buf), 
             "{\"inv_dir\":%s}", 
             boolean_to_string(rotary_button_config.inverted_encoder_direction));
    
    size_t response_len = strlen(buf);
    file->data = buf;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}