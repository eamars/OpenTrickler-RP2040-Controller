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
#include "display.h"
#include "neopixel_led.h"

// Generated headers by html2header.py under scripts
#include "display_mirror.html.h"
#include "bootstrap.min.css.h"
#include "bootstrap.min.js.h"
#include "jquery-3.7.0.min.js.h"
#include "dashboard.html.h"


bool http_404_error(struct fs_file *file, int num_params, char *params[], char *values[]) {

    file->data = "{\"error\":404}";
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


bool http_bootstrap_css(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_bootstrap_min_css);

    file->data = html_bootstrap_min_css;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


bool http_bootstrap_js(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_bootstrap_min_js);

    file->data = html_bootstrap_min_js;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}

bool http_jquery_js(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_jquery_3_7_0_min_js);

    file->data = html_jquery_3_7_0_min_js;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


bool http_dashboard(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_dashboard_html);

    file->data = html_dashboard_html;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


bool rest_endpoints_init() {
    rest_register_handler("/", http_dashboard);
    rest_register_handler("/404", http_404_error);
    rest_register_handler("/rest/scale_weight", http_rest_scale_weight);
    rest_register_handler("/rest/scale_config", http_rest_scale_config);
    rest_register_handler("/rest/charge_mode_config", http_rest_charge_mode_config);
    rest_register_handler("/rest/charge_mode_setpoint", http_rest_charge_mode_setpoint);
    rest_register_handler("/rest/system_control", http_rest_system_control);
    rest_register_handler("/rest/coarse_motor_config", http_rest_coarse_motor_config);
    rest_register_handler("/rest/fine_motor_config", http_rest_fine_motor_config);
    rest_register_handler("/rest/motor_speed", http_rest_motor_speed);
    rest_register_handler("/rest/button_control", http_rest_button_control);
    rest_register_handler("/rest/button_config", http_rest_button_config);
    rest_register_handler("/rest/wireless_config", http_rest_wireless_config);
    rest_register_handler("/rest/neopixel_led_config", http_rest_neopixel_led_config);
    rest_register_handler("/display_buffer", http_get_display_buffer);
    rest_register_handler("/display_mirror", http_display_mirror);
    rest_register_handler("/css/bootstrap.min.css", http_bootstrap_css);
    rest_register_handler("/js/bootstrap.min.js", http_bootstrap_js);
    rest_register_handler("/js/jquery-3.7.0.min.js", http_jquery_js);
}
