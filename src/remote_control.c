/*
 * The remote control's commands (ADR-001, step 3). One command line in, one reply line out:
 * `@ok <command> ...` or `@err <reason>`. Lines a command sends before its reply also start with `@`;
 * everything else on the console is log. No transport here: rc_console.c feeds it lines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <u8g2.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "remote_control.h"
#include "charge_mode.h"
#include "scale.h"
#include "profile.h"
#include "eeprom.h"
#include "version.h"
#include "menu.h"
#include "display.h"
#include "mini_12864_module.h"
#include "motors.h"


// Longest list of tasks `stats` looks through for the smallest stack margin
#define REMOTE_CONTROL_MAX_TASKS 24

// `reboot` restarts this long after its reply is out, so the host has read it
#define REMOTE_CONTROL_REBOOT_DELAY_MS 500

// `press`: at most this many events per command, this far apart so the menu draws between them
#define REMOTE_CONTROL_PRESS_MAX 50
#define REMOTE_CONTROL_PRESS_GAP_MS 50

// `snap`: the u8g2 buffer of the 128 x 64 display, sent in lines of this many bytes before base64
#define REMOTE_CONTROL_SNAP_BUFFER_SIZE 1024
#define REMOTE_CONTROL_SNAP_CHUNK 256

#if PICO_RP2350
#define REMOTE_CONTROL_BOARD "pico2_w"
#else
#define REMOTE_CONTROL_BOARD "pico_w"
#endif


typedef bool (*remote_control_command_fn)(const char *args, char *reply, size_t reply_size,
                                          remote_control_write_fn write, void *write_ctx);

typedef struct {
    const char *name;
    remote_control_command_fn fn;
} remote_control_command_t;


extern charge_mode_config_t charge_mode_config;
extern scale_config_t scale_config;
extern eeprom_metadata_t metadata;
extern QueueHandle_t encoder_event_queue;
extern mini_12864_module_config_t mini_12864_module_config;

static bool remote_control_reboot_pending = false;
static TaskStatus_t remote_control_task_status[REMOTE_CONTROL_MAX_TASKS];
static uint8_t remote_control_snap_buffer[REMOTE_CONTROL_SNAP_BUFFER_SIZE];
static char remote_control_snap_line[8 + (REMOTE_CONTROL_SNAP_CHUNK + 2) / 3 * 4 + 1];


static bool remote_control_help(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx);


static bool remote_control_state(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    profile_t *profile = profile_get_selected();
    uint32_t up_s = (uint32_t) (time_us_64() / 1000000);

    // unique_id and the profile name are stored without a terminator: print them by their length
    snprintf(reply, reply_size,
             "@ok state charge=%d weight=%.3f target=%.3f scale=\"%s\" ver=%s vcs=%s build=%s board=%s id=%.*s "
             "up=%lu profile=\"%.*s\" form=%d sim=%d coarse=%.3f fine=%.3f",
             (int) charge_mode_config.charge_mode_state,
             scale_config.current_scale_measurement,
             charge_mode_config.target_charge_weight,
             get_scale_driver_string(),
             version_string, vcs_hash, build_type,
             REMOTE_CONTROL_BOARD,
             (int) sizeof(metadata.unique_id), metadata.unique_id,
             (unsigned long) up_s,
             (int) sizeof(profile->name), profile->name,
             menu_get_current_form_id(),
             scale_simulation_is_on() ? 1 : 0,
             motor_get_velocity(SELECT_COARSE_TRICKLER_MOTOR),
             motor_get_velocity(SELECT_FINE_TRICKLER_MOTOR));
    return true;
}


static bool remote_control_stats(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    UBaseType_t task_count = uxTaskGetSystemState(remote_control_task_status, REMOTE_CONTROL_MAX_TASKS, NULL);
    if (task_count == 0) {
        snprintf(reply, reply_size, "@err more than %d tasks", REMOTE_CONTROL_MAX_TASKS);
        return false;
    }

    // The task closest to overflowing its stack
    UBaseType_t min_idx = 0;
    for (UBaseType_t idx = 1; idx < task_count; idx += 1) {
        if (remote_control_task_status[idx].usStackHighWaterMark < remote_control_task_status[min_idx].usStackHighWaterMark) {
            min_idx = idx;
        }
    }

    snprintf(reply, reply_size, "@ok stats heap_free=%u heap_min=%u tasks=%u stack_min=\"%s\":%u",
             (unsigned) xPortGetFreeHeapSize(),
             (unsigned) xPortGetMinimumEverFreeHeapSize(),
             (unsigned) task_count,
             remote_control_task_status[min_idx].pcTaskName,
             (unsigned) remote_control_task_status[min_idx].usStackHighWaterMark);
    return true;
}


static bool remote_control_reboot(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    remote_control_reboot_pending = true;
    snprintf(reply, reply_size, "@ok reboot");
    return true;
}


// `press <cw|ccw|enc|rst> [n]`: the encoder and the buttons, as if turned or pressed at the unit
static bool remote_control_press(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    // No sscanf: it pulls about 39 KB of newlib's scanf into the image
    size_t name_len = strcspn(args, " ");
    long count = 1;
    if (args[name_len] != '\0') {
        char *end;
        count = strtol(args + name_len, &end, 10);
        if (*end != '\0') {
            count = 0;
        }
    }
    if (name_len == 0 || count < 1 || count > REMOTE_CONTROL_PRESS_MAX) {
        snprintf(reply, reply_size, "@err usage: press <cw|ccw|enc|rst> [1..%d]", REMOTE_CONTROL_PRESS_MAX);
        return false;
    }

    ButtonEncoderEvent_t button_encoder_event;
    if (name_len == 2 && strncmp(args, "cw", 2) == 0) {
        button_encoder_event = BUTTON_ENCODER_ROTATE_CW;
    }
    else if (name_len == 3 && strncmp(args, "ccw", 3) == 0) {
        button_encoder_event = BUTTON_ENCODER_ROTATE_CCW;
    }
    else if (name_len == 3 && strncmp(args, "enc", 3) == 0) {
        button_encoder_event = BUTTON_ENCODER_PRESSED;
    }
    else if (name_len == 3 && strncmp(args, "rst", 3) == 0) {
        button_encoder_event = BUTTON_RST_PRESSED;
    }
    else {
        snprintf(reply, reply_size, "@err no such button \"%.*s\"", (int) name_len, args);
        return false;
    }

    for (int idx = 0; idx < count; idx += 1) {
        if (xQueueSend(encoder_event_queue, &button_encoder_event, pdMS_TO_TICKS(500)) != pdTRUE) {
            snprintf(reply, reply_size, "@err input queue full after %d", idx);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(REMOTE_CONTROL_PRESS_GAP_MS));
    }

    snprintf(reply, reply_size, "@ok press");
    return true;
}


// `form`: the form on screen (-1 while a mode has it); `form <id>`: show that form, pressing nothing
static bool remote_control_form(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    if (*args != '\0') {
        char *end;
        long form_id = strtol(args, &end, 10);
        if (end == args || *end != '\0' || form_id < 0 || form_id > 255) {
            snprintf(reply, reply_size, "@err usage: form [0..255]");
            return false;
        }

        bool found = false;
        if (!menu_goto_form((uint8_t) form_id, &found)) {
            snprintf(reply, reply_size, "@err the menu does not have the screen");
            return false;
        }
        if (!found) {
            snprintf(reply, reply_size, "@err no such form");
            return false;
        }
    }

    snprintf(reply, reply_size, "@ok form=%d", menu_get_current_form_id());
    return true;
}


static void remote_control_base64(const uint8_t *data, size_t len, char *out) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = 0;
    for (size_t idx = 0; idx < len; idx += 3) {
        uint32_t triple = (uint32_t) data[idx] << 16;
        if (idx + 1 < len) {
            triple |= (uint32_t) data[idx + 1] << 8;
        }
        if (idx + 2 < len) {
            triple |= data[idx + 2];
        }
        out[out_len++] = alphabet[(triple >> 18) & 0x3f];
        out[out_len++] = alphabet[(triple >> 12) & 0x3f];
        out[out_len++] = idx + 1 < len ? alphabet[(triple >> 6) & 0x3f] : '=';
        out[out_len++] = idx + 2 < len ? alphabet[triple & 0x3f] : '=';
    }
    out[out_len] = '\0';
}


// `snap`: the display buffer as u8g2 holds it. r=<0..3> is u8g2's rotation (R0 to R3), which the host undoes
static bool remote_control_snap(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    u8g2_t *display_handler = get_display_handler();
    size_t buffer_size = 8 * u8g2_GetBufferTileHeight(display_handler) * u8g2_GetBufferTileWidth(display_handler);
    if (buffer_size != REMOTE_CONTROL_SNAP_BUFFER_SIZE) {
        snprintf(reply, reply_size, "@err display buffer is %u bytes", (unsigned) buffer_size);
        return false;
    }

    // One copy, so every line comes from the same moment. It can still catch a frame half drawn: the runner
    // snaps twice and keeps the picture only when both are equal
    memcpy(remote_control_snap_buffer, u8g2_GetBufferPtr(display_handler), buffer_size);

    snprintf(remote_control_snap_line, sizeof(remote_control_snap_line), "@snap 128 64 u8g2 r=%d",
             (int) mini_12864_module_config.display_rotation);
    if (write) {
        write(remote_control_snap_line, write_ctx);
    }
    for (size_t offset = 0; offset < buffer_size; offset += REMOTE_CONTROL_SNAP_CHUNK) {
        strcpy(remote_control_snap_line, "@sd ");
        remote_control_base64(remote_control_snap_buffer + offset, REMOTE_CONTROL_SNAP_CHUNK,
                              remote_control_snap_line + 4);
        if (write) {
            write(remote_control_snap_line, write_ctx);
        }
    }

    snprintf(reply, reply_size, "@ok snap");
    return true;
}


// `sim on|off`: main's simulated scale stands in for the stored one, for this run only (ADR-001, F2)
static bool remote_control_sim(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                               void *write_ctx) {
    if (strcmp(args, "on") == 0) {
        if (!scale_simulation_start()) {
            snprintf(reply, reply_size, "@err the simulation cannot start: is the stored driver the simulated one?");
            return false;
        }
    }
    else if (strcmp(args, "off") == 0) {
        if (charge_mode_config.charge_mode_state != CHARGE_MODE_EXIT) {
            snprintf(reply, reply_size, "@err charge mode is running");
            return false;
        }
        scale_simulation_stop();
    }
    else if (*args != '\0') {
        snprintf(reply, reply_size, "@err usage: sim [on|off]");
        return false;
    }

    snprintf(reply, reply_size, "@ok sim=%d scale=\"%s\"", scale_simulation_is_on() ? 1 : 0, get_scale_driver_string());
    return true;
}


// scale tap on|off: the A&D driver copies each frame to the console as "@sf" lines (ADR-013); never stored
static bool remote_control_scale(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    if (strcmp(args, "tap on") == 0) {
        scale_tap_set(true);
    }
    else if (strcmp(args, "tap off") == 0) {
        scale_tap_set(false);
    }
    else if (*args != '\0') {
        snprintf(reply, reply_size, "@err usage: scale [tap on|tap off]");
        return false;
    }

    snprintf(reply, reply_size, "@ok scale tap=%d scale=\"%s\"", scale_tap_is_on() ? 1 : 0, get_scale_driver_string());
    return true;
}


// `motor <coarse|fine> <rev/s>`: enable the driver and turn the trickler; 0 stops it and disables the driver
static bool remote_control_motor(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    size_t name_len = strcspn(args, " ");
    motor_select_t selected_motor;
    if (name_len == 6 && strncmp(args, "coarse", 6) == 0) {
        selected_motor = SELECT_COARSE_TRICKLER_MOTOR;
    }
    else if (name_len == 4 && strncmp(args, "fine", 4) == 0) {
        selected_motor = SELECT_FINE_TRICKLER_MOTOR;
    }
    else {
        snprintf(reply, reply_size, "@err usage: motor <coarse|fine> <rev/s>");
        return false;
    }

    char *end;
    float speed = strtof(args + name_len, &end);
    if (end == args + name_len || *end != '\0' || !isfinite(speed) ||
        fabsf(speed) > get_motor_max_speed(selected_motor)) {
        snprintf(reply, reply_size, "@err speed: a number of rev/s within the motor's maximum, %u",
                 (unsigned) get_motor_max_speed(selected_motor));
        return false;
    }

    // The modes drive the motors themselves
    if (menu_get_current_form_id() < 0) {
        snprintf(reply, reply_size, "@err a mode is running");
        return false;
    }

    if (speed == 0.0f) {
        motor_enable(selected_motor, false);    // also commands speed 0
    }
    else {
        motor_enable(selected_motor, true);
        motor_set_speed(selected_motor, speed);
    }

    snprintf(reply, reply_size, "@ok motor");
    return true;
}


// `charge <gr>` starts charge mode as the web page does; `charge stop` presses reset, as at the unit
static bool remote_control_charge(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    if (strcmp(args, "stop") == 0) {
        if (!charge_mode_stop_remote()) {
            snprintf(reply, reply_size, "@err charge mode is not running");
            return false;
        }
        snprintf(reply, reply_size, "@ok charge stop");
        return true;
    }

    char *end;
    float target = strtof(args, &end);
    if (end == args || *end != '\0' || !isfinite(target) || target <= 0.0f || target > 1000.0f) {
        snprintf(reply, reply_size, "@err usage: charge <0..1000> | charge stop");
        return false;
    }
    if (menu_get_current_form_id() < 0) {
        snprintf(reply, reply_size, "@err a mode is running");
        return false;
    }
    if (!charge_mode_start_remote(target)) {
        snprintf(reply, reply_size, "@err charge mode is already running");
        return false;
    }

    snprintf(reply, reply_size, "@ok charge target=%.3f", target);
    return true;
}


static const remote_control_command_t remote_control_commands[] = {
    {"help", remote_control_help},
    {"state", remote_control_state},
    {"stats", remote_control_stats},
    {"reboot", remote_control_reboot},
    {"press", remote_control_press},
    {"form", remote_control_form},
    {"snap", remote_control_snap},
    {"sim", remote_control_sim},
    {"scale", remote_control_scale},
    {"motor", remote_control_motor},
    {"charge", remote_control_charge},
};

#define REMOTE_CONTROL_COMMAND_COUNT (sizeof(remote_control_commands) / sizeof(remote_control_commands[0]))


static bool remote_control_help(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    size_t len = snprintf(reply, reply_size, "@ok help");
    for (size_t idx = 0; idx < REMOTE_CONTROL_COMMAND_COUNT && len < reply_size; idx += 1) {
        len += snprintf(reply + len, reply_size - len, " %s", remote_control_commands[idx].name);
    }
    return true;
}


bool remote_control_execute(const char *line, char *reply, size_t reply_size, remote_control_write_fn write,
                            void *write_ctx) {
    // The command is the first word; the rest of the line is its arguments
    while (*line == ' ') {
        line += 1;
    }
    size_t name_len = strcspn(line, " ");
    const char *args = line + name_len;
    while (*args == ' ') {
        args += 1;
    }

    for (size_t idx = 0; idx < REMOTE_CONTROL_COMMAND_COUNT; idx += 1) {
        const char *name = remote_control_commands[idx].name;
        if (strlen(name) == name_len && strncmp(name, line, name_len) == 0) {
            return remote_control_commands[idx].fn(args, reply, reply_size, write, write_ctx);
        }
    }

    snprintf(reply, reply_size, "@err unknown command \"%.*s\"", (int) name_len, line);
    return false;
}


void remote_control_after_reply(void) {
    if (remote_control_reboot_pending) {
        watchdog_reboot(0, 0, REMOTE_CONTROL_REBOOT_DELAY_MS);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
