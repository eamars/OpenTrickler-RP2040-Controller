/*
 * The screen's commands on the remote control's console (ADR-001, ADR-010): the knob and the buttons as if used at the
 * unit, the screen in words, its frame, and the knob's tap. Handed to the console at start-up
 * (menu_console_init()), so the console names no screen (ADR-006, gate 6).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "menu.h"
#include "menu_nav.h"
#include "menu_view.h"
#include "mini_12864_module.h"
#include "remote_control.h"

// `press`: at most this many events per command, this far apart so the menu draws between them
#define REMOTE_CONTROL_PRESS_MAX 50
#define REMOTE_CONTROL_PRESS_GAP_MS 50

// `snap`: the screen's frame, sent in lines of this many bytes before base64
#define REMOTE_CONTROL_SNAP_CHUNK 256

static uint8_t remote_control_snap_buffer[MENU_FRAME_BYTES_MAX];
static char remote_control_snap_line[8 + (REMOTE_CONTROL_SNAP_CHUNK + 2) / 3 * 4 + 1];


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
        if (!button_send(button_encoder_event, 500)) {
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
 * (each "first" marks the first shown) and their `@button`, a dialog's `@line` lines, the code's `@code` text and its
 * `@line` lines, or the editor's and a job's `@figure` line; then `@softkeys` where there are any
 */
static void remote_control_screen_lines(const menu_screen_t *screen, remote_control_write_fn write, void *write_ctx) {
    static const char *const kinds[] = {"list", "dialog", "notes", "edit", "job", "control", "code"};
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
        case MENU_SCREEN_CODE:
            // The code's text, then the lines beside it as a dialog's
            snprintf(line, sizeof(line), "@code \"%s\"", screen->code);
            write(line, write_ctx);
            // fall through
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
    uint16_t width;
    uint16_t height;
    size_t frame_bytes;
    menu_frame_info(&width, &height, &frame_bytes);
    menu_frame_copy(remote_control_snap_buffer);

    snprintf(remote_control_snap_line, sizeof(remote_control_snap_line), "@snap %u %u %s", (unsigned) width,
             (unsigned) height, MENU_FRAME_FORMAT);
    if (write) {
        write(remote_control_snap_line, write_ctx);
    }
    for (size_t offset = 0; offset < frame_bytes; offset += REMOTE_CONTROL_SNAP_CHUNK) {
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


bool menu_console_init(void) {
    return remote_control_register("press", remote_control_press) &&
           remote_control_register("screen", remote_control_screen) &&
           remote_control_register("snap", remote_control_snap) &&
           remote_control_register("knob", remote_control_knob);
}
