#include <string.h>
#include <stdlib.h>
#include "rest_endpoints.h"
#include "http_rest.h"
#include "charge_mode.h"
#include "motors.h"
#include "scale.h"
#include "wireless.h"
#include "eeprom.h"
#include "mini_12864_module.h"
#include "display.h"
#include "neopixel_led.h"
#include "profile.h"
#include "cleanup_mode.h"
#include "servo_gate.h"
#include "system_control.h"

// Generated headers by html2header.py under scripts
#include "display_mirror.html.h"
#include "web_portal.html.h"
#include "wizard.html.h"


bool http_404_error(struct fs_file *file, int num_params, char *params[], char *values[]) {

    file->data = "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n\r\n"
                 "{\"error\":404}";
    file->len = 13;
    file->index = 13;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_display_mirror(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_display_mirror_html);

    file->data = html_display_mirror_html;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


bool http_web_portal(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_web_portal_html);

    file->data = html_web_portal_html;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


bool http_wizard(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_wizard_html);

    file->data = html_wizard_html;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}



bool rest_endpoints_init(bool default_wizard) {
    if (default_wizard) {
        rest_register_handler("/", http_wizard);
    }
    else {
        rest_register_handler("/", http_web_portal);
    }
    
    rest_register_handler("/mobile", http_web_portal);
    rest_register_handler("/wizard", http_wizard);
    rest_register_handler("/404", http_404_error);
    rest_register_handler("/rest/scale_action", http_rest_scale_action);
    rest_register_handler("/rest/scale_config", http_rest_scale_config);
    rest_register_handler("/rest/charge_mode_config", http_rest_charge_mode_config);
    rest_register_handler("/rest/charge_mode_state", http_rest_charge_mode_state);
    rest_register_handler("/rest/cleanup_mode_state", http_rest_cleanup_mode_state);
    rest_register_handler("/rest/system_control", http_rest_system_control);
    rest_register_handler("/rest/coarse_motor_config", http_rest_coarse_motor_config);
    rest_register_handler("/rest/fine_motor_config", http_rest_fine_motor_config);
    rest_register_handler("/rest/button_control", http_rest_button_control);
    rest_register_handler("/rest/mini_12864_config", http_rest_mini_12864_module_config);
    rest_register_handler("/rest/wireless_config", http_rest_wireless_config);
    rest_register_handler("/rest/neopixel_led_config", http_rest_neopixel_led_config);
    rest_register_handler("/rest/profile_config", http_rest_profile_config);
    rest_register_handler("/rest/profile_summary", http_rest_profile_summary);
    rest_register_handler("/rest/servo_gate_state", http_rest_servo_gate_state);
    rest_register_handler("/rest/servo_gate_config", http_rest_servo_gate_config);
    rest_register_handler("/display_buffer", http_get_display_buffer);
    rest_register_handler("/display_mirror", http_display_mirror);

    return true;
}
