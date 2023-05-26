#include <string.h>
#include <stdlib.h>
#include "rest_endpoints.h"
#include "http_rest.h"
#include "charge_mode.h"
#include "motors.h"
#include "scale.h"
#include "wireless.h"


bool http_rest_eeprom_config(struct fs_file *file, int num_params, char *params[], char *values[]) {

    file->data = "It Works";
    file->len = 9;
    file->index = 9;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_charge_mode_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t data_length;
    file->data = charge_mode_config_to_json();
    data_length = strlen(file->data);
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_motor_config(struct fs_file *file, int num_params, char *params[], char *values[]){
    static char error_msg_buffer[32];
    const char * error_msg = NULL;
    const char * response;
    if (num_params != 1) {
        error_msg = "invalid_num_args";
    }
    else {
        if (strcmp(params[0], "motor") != 0) {
            error_msg = "invalid_param";
        }
        else {
            if (strcmp(values[0], "coarse") == 0) {
                response = motor_config_to_json(SELECT_COARSE_TRICKLER_MOTOR);
            }
            else if (strcmp(values[1], "fine")) {
                response = motor_config_to_json(SELECT_FINE_TRICKLER_MOTOR);
            }
            else {
                error_msg = "invalid_value";
            }
        }
    }

    if (error_msg) {
        snprintf(error_msg_buffer, sizeof(error_msg_buffer),
                 "{\"error\":%s}", error_msg);
        response = error_msg_buffer;
    }
    
    size_t response_len = strlen(response);
    file->data = response;
    file->len = response_len;
    file->index = response_len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_scale_weight(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t data_length;
    file->data = scale_weight_to_json();
    data_length = strlen(file->data);
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_wireless_config(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t data_length;
    file->data = wireless_config_to_json();
    data_length = strlen(file->data);
    file->len = data_length;
    file->index = data_length;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_404_error(struct fs_file *file, int num_params, char *params[], char *values[]) {

    file->data = "{\"error\":404}";
    file->len = 13;
    file->index = 13;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool rest_endpoints_init() {
    rest_register_handler("/404", http_404_error);
    rest_register_handler("/rest/scale_weight", http_rest_scale_weight);
    rest_register_handler("/rest/charge_mode_config", http_rest_charge_mode_config);
    rest_register_handler("/rest/eeprom_config", http_rest_eeprom_config);
    rest_register_handler("/rest/motor_config", http_rest_motor_config);
    rest_register_handler("/rest/wireless_config", http_rest_wireless_config);
}