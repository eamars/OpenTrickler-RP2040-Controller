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

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "lwip/stats.h"
#include "lwip/memp.h"

#include "remote_control.h"
#include "charge_mode.h"
#include "scale.h"
#include "profile.h"
#include "eeprom.h"
#include "unit_id.h"
#include "version.h"
#include "menu.h"
#include "display.h"
#include "mini_12864_module.h"
#include "motors.h"
#include "settings.h"
#include "activity.h"
#include "update.h"
#include "wireless.h"
#include "http_server.h"


// Longest list of tasks `stats` looks through for the smallest stack margin
#define REMOTE_CONTROL_MAX_TASKS 24

// `reboot` restarts this long after its reply is out, so the host has read it
#define REMOTE_CONTROL_REBOOT_DELAY_MS 500

// `press`: at most this many events per command, this far apart so the menu draws between them
#define REMOTE_CONTROL_PRESS_MAX 50
#define REMOTE_CONTROL_PRESS_GAP_MS 50

// `snap`: the screen's frame, sent in lines of this many bytes before base64
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
extern QueueHandle_t encoder_event_queue;
extern mini_12864_module_config_t mini_12864_module_config;

static bool remote_control_reboot_pending = false;
static char remote_control_tmc_line[256];
static TaskStatus_t remote_control_task_status[REMOTE_CONTROL_MAX_TASKS];
static uint8_t remote_control_snap_buffer[DISPLAY_FRAME_BYTES];
static char remote_control_snap_line[8 + (REMOTE_CONTROL_SNAP_CHUNK + 2) / 3 * 4 + 1];


static bool remote_control_help(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx);


static bool remote_control_state(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    profile_t *profile = profile_get_selected();
    uint32_t up_s = (uint32_t) (time_us_64() / 1000000);
    char unit_id[UNIT_ID_LENGTH + 1];
    unit_id_get(unit_id, sizeof(unit_id));

    // The profile's name is stored without a terminator: print it by its length
    snprintf(reply, reply_size,
             "@ok state charge=%d weight=%.3f target=%.3f scale=\"%s\" ver=%s vcs=%s build=%s board=%s id=%s "
             "up=%lu profile=\"%.*s\" form=%d sim=%d coarse=%.3f fine=%.3f drivers=%s,%s",
             (int) charge_mode_config.charge_mode_state,
             scale_config.current_scale_measurement,
             charge_mode_config.target_charge_weight,
             get_scale_driver_string(),
             version_string, vcs_hash, build_type,
             REMOTE_CONTROL_BOARD,
             unit_id,
             (unsigned long) up_s,
             (int) sizeof(profile->name), profile->name,
             menu_get_current_form_id(),
             scale_simulation_is_on() ? 1 : 0,
             motor_get_velocity(SELECT_COARSE_TRICKLER_MOTOR),
             motor_get_velocity(SELECT_FINE_TRICKLER_MOTOR),
             motor_driver_get_status_string(SELECT_COARSE_TRICKLER_MOTOR),
             motor_driver_get_status_string(SELECT_FINE_TRICKLER_MOTOR));
    return true;
}


static bool remote_control_stats(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    UBaseType_t task_count = uxTaskGetSystemState(remote_control_task_status, REMOTE_CONTROL_MAX_TASKS, NULL);
    if (task_count == 0) {
        snprintf(reply, reply_size, "@err more than %d tasks", REMOTE_CONTROL_MAX_TASKS);
        return false;
    }

    // `stats tasks`: each task's stack never touched, in bytes, first (ADR-010, gate 5: the job task's and the poll's
    // stacks were sized from it)
    while (*args == ' ') {
        args += 1;
    }
    if (strcmp(args, "tasks") == 0) {
        for (UBaseType_t idx = 0; idx < task_count; idx += 1) {
            char line[80];
            snprintf(line, sizeof(line), "@task name=\"%s\" stack_left=%lu priority=%u",
                     remote_control_task_status[idx].pcTaskName,
                     (unsigned long) (remote_control_task_status[idx].usStackHighWaterMark * sizeof(StackType_t)),
                     (unsigned) remote_control_task_status[idx].uxCurrentPriority);
            write(line, write_ctx);
        }
    }

    // The task closest to overflowing its stack
    UBaseType_t min_idx = 0;
    for (UBaseType_t idx = 1; idx < task_count; idx += 1) {
        if (remote_control_task_status[idx].usStackHighWaterMark < remote_control_task_status[min_idx].usStackHighWaterMark) {
            min_idx = idx;
        }
    }

    snprintf(reply, reply_size, "@ok stats heap_free=%u heap_min=%u tasks=%u stack_min=\"%s\":%u bad_frames=%lu",
             (unsigned) xPortGetFreeHeapSize(),
             (unsigned) xPortGetMinimumEverFreeHeapSize(),
             (unsigned) task_count,
             remote_control_task_status[min_idx].pcTaskName,
             (unsigned) remote_control_task_status[min_idx].usStackHighWaterMark,
             (unsigned long) scale_get_bad_frames());
    return true;
}


// The pools `net` reports: those every web connection takes from
static const struct {
    const char *name;
    memp_t pool;
} remote_control_net_pools[] = {
    {"tcp_pcb", MEMP_TCP_PCB},
    {"tcp_seg", MEMP_TCP_SEG},
    {"pbuf", MEMP_PBUF},
    {"pbuf_pool", MEMP_PBUF_POOL},
};


static bool remote_control_net(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                               void *write_ctx) {
    // lwIP's heap and pools (ADR-003, gate 1): in use now, the most ever in use, the size, and the allocations
    // that failed. Read without lwIP's lock: counters, only read here
    size_t len = snprintf(reply, reply_size, "@ok net heap_used=%u heap_max=%u heap_size=%u heap_err=%lu",
                          (unsigned) lwip_stats.mem.used,
                          (unsigned) lwip_stats.mem.max,
                          (unsigned) lwip_stats.mem.avail,
                          (unsigned long) lwip_stats.mem.err);
    for (size_t idx = 0; idx < sizeof(remote_control_net_pools) / sizeof(remote_control_net_pools[0]); idx += 1) {
        const struct stats_mem *pool = lwip_stats.memp[remote_control_net_pools[idx].pool];
        const char *name = remote_control_net_pools[idx].name;
        if (len >= reply_size) {
            break;
        }
        len += snprintf(reply + len, reply_size - len, " %s_used=%u %s_max=%u %s_size=%u %s_err=%lu",
                        name, (unsigned) pool->used,
                        name, (unsigned) pool->max,
                        name, (unsigned) pool->avail,
                        name, (unsigned long) pool->err);
    }
    // The event streams (ADR-008, gate 2): open now, opened and refused since the start
    http_server_stats_t server;
    http_server_get_stats(&server);
    if (len < reply_size) {
        snprintf(reply + len, reply_size - len, " live=%u live_opened=%lu live_refused=%lu", server.slots_live,
                 (unsigned long) server.live_opened, (unsigned long) server.live_refused);
    }
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


/*
 * The screen in words (ADR-010): a `@screen` line; then a list's `@row` lines, a card's or notes' `@line` lines
 * (each "first" marks the first shown) and their `@button`, a dialog's `@line` lines, or the editor's and a job's
 * `@figure` line; then `@softkeys` where there are any
 */
static void remote_control_screen_lines(const menu_screen_t *screen, remote_control_write_fn write, void *write_ctx) {
    static const char *const kinds[] = {"list", "dialog", "notes", "edit", "job", "control"};
    char line[160];
    snprintf(line, sizeof(line), "@screen id=%s kind=%s title=\"%s\"%s%s%s focus=%d marks=%s%s%s%s%s", screen->id,
             kinds[screen->kind], screen->title, screen->title_note[0] != '\0' ? " note=\"" : "", screen->title_note,
             screen->title_note[0] != '\0' ? "\"" : "", (int) screen->focus, screen->mark_station ? "W" : "",
             screen->mark_access_point ? "AP" : "", screen->mark_remote ? "R" : "", screen->mark_simulated ? "S" : "",
             screen->title_back ? " back" : "");
    write(line, write_ctx);
    switch (screen->kind) {
        case MENU_SCREEN_LIST:
        case MENU_SCREEN_CONTROL:
            for (int idx = 0; idx < screen->row_count; idx += 1) {
                snprintf(line, sizeof(line), "@row %d label=\"%s\" value=\"%s\"%s%s%s%s", idx,
                         screen->row_label[idx], screen->row_value[idx], screen->row_opens[idx] ? " opens" : "",
                         screen->editing && idx == screen->focus ? " editing" : "",
                         screen->row_chosen[idx] ? " chosen" : "", screen->row_info[idx] ? " info" : "");
                write(line, write_ctx);
            }
            break;
        case MENU_SCREEN_NOTES:
        case MENU_SCREEN_DIALOG:
            for (int idx = 0; idx < screen->row_count; idx += 1) {
                // A key's value, at the line's right, when it has one
                char value[MENU_TEXT + 10] = "";
                if (screen->row_value[idx][0] != '\0') {
                    snprintf(value, sizeof(value), " value=\"%s\"", screen->row_value[idx]);
                }
                snprintf(line, sizeof(line), "@line %d \"%s\"%s%s%s", idx, screen->row_label[idx], value,
                         screen->row_bold[idx] ? " bold" : "", idx == screen->first ? " first" : "");
                write(line, write_ctx);
            }
            if (screen->button[0] != '\0') {
                snprintf(line, sizeof(line), "@button \"%s\"%s", screen->button,
                         screen->button_reached ? " reached" : "");
                write(line, write_ctx);
            }
            break;
        case MENU_SCREEN_EDIT:
        case MENU_SCREEN_JOB:
            snprintf(line, sizeof(line), "@figure \"%s\" unit=\"%s\" at=%d%s line=\"%s\"%s", screen->figure,
                     screen->unit, (int) screen->edit_at, screen->editing ? " editing" : "", screen->context,
                     screen->context_bold ? " bold" : "");
            write(line, write_ctx);
            break;
    }
    if (screen->softkey_left[0] != '\0' || screen->softkey_right[0] != '\0') {
        const char *focused = screen->softkey_focus == 0 ? " focus=left" :
                              (screen->softkey_focus == 1 ? " focus=right" : "");
        snprintf(line, sizeof(line), "@softkeys left=\"%s\" right=\"%s\"%s", screen->softkey_left,
                 screen->softkey_right, focused);
        write(line, write_ctx);
    }
}


// `screen`: what is on screen, in words; `screen list`: every page's id; `screen <id>`: open that page, pressing
// nothing (ADR-010, gate 4, in place of MUI's `form`)
static bool remote_control_screen(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    if (strcmp(args, "list") == 0) {
        char line[48];
        for (size_t idx = 0; idx < menu_nav_page_count(); idx += 1) {
            snprintf(line, sizeof(line), "@page %s", menu_nav_page_id(idx));
            write(line, write_ctx);
        }
        snprintf(reply, reply_size, "@ok screen pages=%u", (unsigned) menu_nav_page_count());
        return true;
    }
    if (*args != '\0') {
        bool found = false;
        if (!menu_goto_screen(args, &found)) {
            snprintf(reply, reply_size, "@err the menu did not answer");
            return false;
        }
        if (!found) {
            snprintf(reply, reply_size, "@err no such page, or a job has the screen");
            return false;
        }
    }

    static menu_screen_t screen;
    uint32_t heap_max;
    menu_screen_copy(&screen, &heap_max);
    remote_control_screen_lines(&screen, write, write_ctx);
    // The frame's times (ADR-010, gate 4): the last draw and send, the longest of each, in microseconds
    menu_frame_times_t times;
    menu_frame_times(&times);
    snprintf(reply, reply_size,
             "@ok screen id=%s lvgl_heap_max=%lu lvgl_biggest_free_min=%lu frames=%lu draw_us=%lu draw_max_us=%lu "
             "send_us=%lu send_max_us=%lu", screen.id, (unsigned long) heap_max,
             (unsigned long) menu_lvgl_biggest_free(), (unsigned long) times.frames, (unsigned long) times.draw_us,
             (unsigned long) times.draw_max_us, (unsigned long) times.send_us, (unsigned long) times.send_max_us);
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


// `snap`: the screen's frame as the web's mirror has it (MENU_FRAME_FORMAT): as a person sees it, since the panel
// turns itself (ADR-010, gate 4)
static bool remote_control_snap(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    // One copy, under the frame's lock: a whole frame
    menu_frame_copy(remote_control_snap_buffer);

    snprintf(remote_control_snap_line, sizeof(remote_control_snap_line), "@snap %u %u %s", (unsigned) DISPLAY_WIDTH,
             (unsigned) DISPLAY_HEIGHT, MENU_FRAME_FORMAT);
    if (write) {
        write(remote_control_snap_line, write_ctx);
    }
    for (size_t offset = 0; offset < DISPLAY_FRAME_BYTES; offset += REMOTE_CONTROL_SNAP_CHUNK) {
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
    // The simulated scale's faults (ADR-013): bad, quiet <ms>, rate <per second>; never stored
    else if (strcmp(args, "bad") == 0 || strncmp(args, "quiet ", 6) == 0) {
        if (!scale_simulation_is_on()) {
            snprintf(reply, reply_size, "@err the simulation is off");
            return false;
        }
        if (args[0] == 'b') {
            simulated_scale_send_bad();
        }
        else {
            char *end;
            long ms = strtol(args + 6, &end, 10);
            if (end == args + 6 || *end != '\0' || ms < 1 || ms > 60000) {
                snprintf(reply, reply_size, "@err usage: sim quiet <ms, 1 to 60000>");
                return false;
            }
            simulated_scale_go_quiet((uint32_t) ms);
        }
    }
    else if (strncmp(args, "rate ", 5) == 0) {
        char *end;
        float per_second = strtof(args + 5, &end);
        if (end == args + 5 || *end != '\0' || !simulated_scale_set_rate(per_second)) {
            snprintf(reply, reply_size, "@err usage: sim rate <readings a second, 1 to 50>");
            return false;
        }
    }
    else if (strncmp(args, "weight ", 7) == 0) {
        // The simulated scale shows this weight until `sim weight off` (ADR-012); never stored
        if (strcmp(args + 7, "off") == 0) {
            simulated_scale_force_weight(false, 0.0f);
        }
        else {
            char *end;
            float weight = strtof(args + 7, &end);
            if (end == args + 7 || *end != '\0' || !isfinite(weight)) {
                snprintf(reply, reply_size, "@err usage: sim weight <a number|off>");
                return false;
            }
            simulated_scale_force_weight(true, weight);
        }
    }
    else if (*args != '\0') {
        snprintf(reply, reply_size, "@err usage: sim [on|off|bad|quiet <ms>|rate <per second>|weight <a number|off>]");
        return false;
    }

    snprintf(reply, reply_size, "@ok sim=%d scale=\"%s\" rate=%.1f", scale_simulation_is_on() ? 1 : 0,
             get_scale_driver_string(), simulated_scale_get_rate());
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


// `knob`: the knob's tap (mini_12864_module.h), an `@kt <at_us> <kind>` line an entry, then cleared
static bool remote_control_knob(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    (void) args;
    static knob_tap_entry_t entries[128];
    uint32_t lost;
    size_t count = mini_12864_knob_tap(entries, 128, &lost);
    char line[32];
    for (size_t idx = 0; idx < count; idx += 1) {
        snprintf(line, sizeof(line), "@kt %lu %c", (unsigned long) entries[idx].at_us, entries[idx].kind);
        if (write) {
            write(line, write_ctx);
        }
    }
    snprintf(reply, reply_size, "@ok knob entries=%u lost=%lu", (unsigned) count, (unsigned long) lost);
    return true;
}


// `motor <coarse|fine> <rev/s>`: enable the driver and turn the trickler; 0 stops it and disables the driver.
// `motor`: the steps made so far
static bool remote_control_motor(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    // Alone: the steps counted on each STEP pin, those fed to the PIO, the feeds that came late, and the rate the
    // pulses were last fed at, with the unit's clock so that two replies give a speed (ADR-012)
    if (*args == '\0') {
        step_pulses_stats_t coarse;
        step_pulses_stats_t fine;
        motor_get_step_stats(SELECT_COARSE_TRICKLER_MOTOR, &coarse);
        motor_get_step_stats(SELECT_FINE_TRICKLER_MOTOR, &fine);
        snprintf(reply, reply_size,
                 "@ok motor t_us=%llu counted=%d coarse_made=%lu coarse_fed=%lu coarse_underruns=%lu coarse_rate=%.2f "
                 "fine_made=%lu fine_fed=%lu fine_underruns=%lu fine_rate=%.2f",
                 (unsigned long long) time_us_64(), (coarse.counted_on_pin && fine.counted_on_pin) ? 1 : 0,
                 (unsigned long) coarse.steps_made, (unsigned long) coarse.steps_fed, (unsigned long) coarse.underruns,
                 motor_get_step_rate(SELECT_COARSE_TRICKLER_MOTOR),
                 (unsigned long) fine.steps_made, (unsigned long) fine.steps_fed, (unsigned long) fine.underruns,
                 motor_get_step_rate(SELECT_FINE_TRICKLER_MOTOR));
        return true;
    }

    size_t name_len = strcspn(args, " ");
    motor_select_t selected_motor;
    if (name_len == 6 && strncmp(args, "coarse", 6) == 0) {
        selected_motor = SELECT_COARSE_TRICKLER_MOTOR;
    }
    else if (name_len == 4 && strncmp(args, "fine", 4) == 0) {
        selected_motor = SELECT_FINE_TRICKLER_MOTOR;
    }
    else {
        snprintf(reply, reply_size, "@err usage: motor | motor <coarse|fine> <rev/s>");
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


typedef struct {
    remote_control_write_fn write;
    void *write_ctx;
} remote_control_tmc_ctx_t;


static void remote_control_tmc_emit(const char *line, void *ctx) {
    remote_control_tmc_ctx_t *tmc_ctx = (remote_control_tmc_ctx_t *) ctx;

    snprintf(remote_control_tmc_line, sizeof(remote_control_tmc_line), "@tmc %s", line);
    if (tmc_ctx->write) {
        tmc_ctx->write(remote_control_tmc_line, tmc_ctx->write_ctx);
    }
}


/*
 * `tmc <coarse|fine>`: the driver's registers by field, as `@tmc r ...` lines read from the chip now and `@tmc w ...`
 * lines for those that can only be written, then its serial line's counts (ADR-012).
 * `tmc <coarse|fine> setup <n>`: the driver set up n times from the stored settings, nine checked writes each.
 * `tmc <coarse|fine> lose` and `find`: for proofs, the driver made to look absent, and found again. Never stored.
 * `tmc <coarse|fine> fault <short|hot|reset|supply|none>`: for proofs, the driver read to report a fault. Never stored.
 */
static bool remote_control_tmc(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
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
        snprintf(reply, reply_size, "@err usage: tmc <coarse|fine> [setup <n>|lose|find|fault <word>]");
        return false;
    }

    const char *rest = args + name_len;
    while (*rest == ' ') {
        rest += 1;
    }

    int setups_failed = 0;
    if (strncmp(rest, "setup ", 6) == 0) {
        // The motors are still: a driver is not written while a mode turns it
        if (menu_get_current_form_id() < 0) {
            snprintf(reply, reply_size, "@err a mode is running");
            return false;
        }

        char *end;
        long count = strtol(rest + 6, &end, 10);
        if (end == rest + 6 || *end != '\0' || count < 1 || count > 1000) {
            snprintf(reply, reply_size, "@err setup: 1 to 1000 times");
            return false;
        }
        for (long idx = 0; idx < count; idx += 1) {
            if (!motor_driver_setup(selected_motor)) {
                setups_failed += 1;
            }
        }
    }
    else if (strncmp(rest, "fault ", 6) == 0) {
        // For proofs, never stored: the driver is read to report this fault until `fault none`
        if (!motor_driver_inject_fault(selected_motor, rest + 6)) {
            snprintf(reply, reply_size, "@err fault: short, hot, reset, supply or none");
            return false;
        }
    }
    else if (strcmp(rest, "lose") == 0 || strcmp(rest, "find") == 0) {
        // For proofs, never stored: the driver looks absent until `find`; a mode, or `setup`, then sets it up again
        motor_driver_set_lost(selected_motor, rest[0] == 'l');
    }
    else if (*rest != '\0') {
        snprintf(reply, reply_size, "@err usage: tmc <coarse|fine> [setup <n>|lose|find|fault <word>]");
        return false;
    }
    else {
        remote_control_tmc_ctx_t tmc_ctx = {write, write_ctx};
        if (!motor_driver_dump(selected_motor, remote_control_tmc_emit, &tmc_ctx)) {
            snprintf(reply, reply_size, "@err the %s driver does not answer", get_motor_select_string(selected_motor));
            return false;
        }
    }

    motor_driver_stats_t stats;
    motor_driver_get_stats(selected_motor, &stats);
    snprintf(reply, reply_size,
             "@ok tmc ready=%d ma=%u writes=%lu write_retries=%lu read_retries=%lu failures=%lu setups_failed=%d",
             stats.ready ? 1 : 0, (unsigned) stats.current_ma,
             (unsigned long) stats.writes, (unsigned long) stats.write_retries,
             (unsigned long) stats.read_retries, (unsigned long) stats.failures, setups_failed);
    return setups_failed == 0;
}


// `charge <gr>` starts charge mode as the API does; `charge stop` stops it, as the API's stop does (ADR-003, gate 4)
static bool remote_control_charge(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    if (strcmp(args, "stop") == 0) {
        if (!activity_request_stop(ACTIVITY_CHARGE, ACTIVITY_BY_CONSOLE)) {
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
    if (activity_get() != ACTIVITY_NONE || menu_get_current_form_id() < 0) {
        snprintf(reply, reply_size, "@err a mode is running");
        return false;
    }
    if (!charge_mode_target_fits(target)) {
        snprintf(reply, reply_size, "@err %.3f is not a target the menu's digits can show", target);
        return false;
    }
    switch (charge_mode_request_start(ACTIVITY_BY_CONSOLE, target, -1)) {
        case CHARGE_START_OK:
            break;
        case CHARGE_START_DRIVER_FAULT:
            snprintf(reply, reply_size, "@err a motor driver is not OK: %s", motors_get_fault());
            return false;
        default:
            snprintf(reply, reply_size, "@err charge mode is already running or asked");
            return false;
    }

    snprintf(reply, reply_size, "@ok charge target=%.3f", target);
    return true;
}


// `setting [<path> [name=value ...|save]]`: the API's settings' tables, read and written (ADR-003, gate 3)
static bool remote_control_setting(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                   void *write_ctx) {
    settings_console(args, reply, reply_size, write, write_ctx);
    return strncmp(reply, "@ok", 3) == 0;
}


// `wifi leave`: the station leaves its network, as when its router goes away; it joins again (ADR-004)
static bool remote_control_wifi(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    (void) write;
    (void) write_ctx;
    while (*args == ' ') {
        args += 1;
    }
#if RC_TEST_AP_PROBE
    if (strcmp(args, "join") == 0) {
        bool asked = wireless_probe_join();
        snprintf(reply, reply_size, asked ? "@ok wifi join asked" : "@err failed wifi join");
        return asked;
    }
    if (strcmp(args, "scan") == 0) {
        uint32_t took_ms = 0;
        int found = wireless_probe_scan(&took_ms);
        if (found < 0) {
            snprintf(reply, reply_size, "@err failed wifi scan");
            return false;
        }
        for (uint8_t i = 0; i < found; i++) {
            const cyw43_ev_scan_result_t *r = wireless_probe_scan_found(i);
            char line[96];
            snprintf(line, sizeof(line), "@scan ssid=%.*s ch=%u rssi=%d auth=%u bssid=%02x:%02x:%02x:%02x:%02x:%02x",
                     r->ssid_len, r->ssid, r->channel, r->rssi, r->auth_mode, r->bssid[0], r->bssid[1], r->bssid[2],
                     r->bssid[3], r->bssid[4], r->bssid[5]);
            write(line, write_ctx);
        }
        snprintf(reply, reply_size, "@ok wifi scan found=%d took_ms=%lu", found, (unsigned long) took_ms);
        return true;
    }
#endif
    if (strcmp(args, "leave") != 0) {
        snprintf(reply, reply_size, "@err usage wifi leave");
        return false;
    }
    bool left = wireless_station_leave();
    snprintf(reply, reply_size, left ? "@ok wifi left" : "@err failed wifi leave");
    return left;
}


// `update [try]`: the image slots, and a restart into a trial of the other one (ADR-004)
static bool remote_control_update(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    (void) write;
    (void) write_ctx;
    return update_console(args, reply, reply_size);
}


static const remote_control_command_t remote_control_commands[] = {
    {"help", remote_control_help},
    {"state", remote_control_state},
    {"stats", remote_control_stats},
    {"net", remote_control_net},
    {"reboot", remote_control_reboot},
    {"press", remote_control_press},
    {"screen", remote_control_screen},
    {"snap", remote_control_snap},
    {"sim", remote_control_sim},
    {"scale", remote_control_scale},
    {"motor", remote_control_motor},
    {"knob", remote_control_knob},
    {"tmc", remote_control_tmc},
    {"charge", remote_control_charge},
    {"setting", remote_control_setting},
    {"update", remote_control_update},
    {"wifi", remote_control_wifi},
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
