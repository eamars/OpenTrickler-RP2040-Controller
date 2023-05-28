#include <string.h>
#include <stdlib.h>
#include "rest_endpoints.h"
#include "http_rest.h"
#include "charge_mode.h"
#include "motors.h"
#include "scale.h"
#include "wireless.h"
#include "eeprom.h"
#include "rotary_button.h"


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
    rest_register_handler("/rest/button_control", http_rest_button_control);
    rest_register_handler("/rest/wireless_config", http_rest_wireless_config);
}