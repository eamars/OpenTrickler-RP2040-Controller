/*
    ADR-010, gate 1: LVGL beside u8g2, to measure what it takes on the unit. A task of its own draws one menu page (a
    title and seven rows with their values, one focused) into a one-bit frame of its own, which is never sent to the
    panel: the menu keeps the panel. The console's `lvgl`:
      lvgl          starts it the first time; reports LVGL's heap (its size and the most it held), the task's stack
                    left, and how long the last frame took
      lvgl next     moves the focus one row down and draws it at once: that frame's time
      lvgl full     draws the whole page again: that frame's time
      lvgl frame    the frame as 64 `@lv` lines of hex, a row each, for a picture on the PC
*/
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "pico/time.h"

#include "lvgl/lvgl.h"

#include "measure_lvgl.h"

#define MEASURE_LVGL_WIDTH 128
#define MEASURE_LVGL_HEIGHT 64
#define MEASURE_LVGL_PALETTE_BYTES 8
#define MEASURE_LVGL_ROW_BYTES (MEASURE_LVGL_WIDTH / 8)
#define MEASURE_LVGL_FRAME_BYTES (MEASURE_LVGL_ROW_BYTES * MEASURE_LVGL_HEIGHT + MEASURE_LVGL_PALETTE_BYTES)
#define MEASURE_LVGL_TASK_STACK_WORDS 1536
#define MEASURE_LVGL_TASK_PRIORITY 2
#define MEASURE_LVGL_TIMER_MS 30
#define MEASURE_LVGL_ROWS 7
#define MEASURE_LVGL_REPLY_MS 3000

typedef enum {
    MEASURE_LVGL_REPORT,
    MEASURE_LVGL_NEXT,
    MEASURE_LVGL_FULL,
} measure_lvgl_ask_t;

static const char *measure_lvgl_labels[MEASURE_LVGL_ROWS][2] = {
    {"Scale", "A&D FX-i"},
    {"Baud rate", "19200"},
    {"Decimals", "2"},
    {"Invert knob", "Off"},
    {"Rotation", "0"},
    {"Version", "2.0.2"},
    {"Restart", ""},
};

static uint8_t measure_lvgl_frame[MEASURE_LVGL_FRAME_BYTES] __attribute__((aligned(4)));
static QueueHandle_t measure_lvgl_queue = NULL;
static SemaphoreHandle_t measure_lvgl_done = NULL;
static TaskHandle_t measure_lvgl_task_handle = NULL;
static lv_display_t *measure_lvgl_display = NULL;
static lv_obj_t *measure_lvgl_rows[MEASURE_LVGL_ROWS];
static int measure_lvgl_focus = 0;
static volatile uint32_t measure_lvgl_frame_us = 0;
static volatile uint32_t measure_lvgl_frames = 0;
static volatile uint32_t measure_lvgl_init_us = 0;


static uint32_t measure_lvgl_tick(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


// The frame stays in its buffer, for `lvgl frame`: nothing is sent to the panel
static void measure_lvgl_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    (void) area;
    (void) px_map;
    measure_lvgl_frames += 1;
    lv_display_flush_ready(display);
}


static void measure_lvgl_set_focus(int focus) {
    lv_obj_remove_state(measure_lvgl_rows[measure_lvgl_focus], LV_STATE_FOCUSED);
    measure_lvgl_focus = focus;
    lv_obj_add_state(measure_lvgl_rows[measure_lvgl_focus], LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(measure_lvgl_rows[measure_lvgl_focus], LV_ANIM_OFF);
}


// A settings page as the design has it: a title, then rows, each a label and its value
static void measure_lvgl_build_page(void) {
    lv_theme_t *theme = lv_theme_mono_init(measure_lvgl_display, false, &lv_font_unscii_8);
    lv_display_set_theme(measure_lvgl_display, theme);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "< Settings");

    lv_obj_t *list = lv_list_create(screen);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);

    for (int row = 0; row < MEASURE_LVGL_ROWS; row += 1) {
        lv_obj_t *button = lv_list_add_button(list, NULL, measure_lvgl_labels[row][0]);
        lv_obj_t *value = lv_label_create(button);
        lv_label_set_text(value, measure_lvgl_labels[row][1]);
        lv_obj_set_flex_grow(lv_obj_get_child(button, 0), 1);
        measure_lvgl_rows[row] = button;
    }
    measure_lvgl_set_focus(0);
}


static uint32_t measure_lvgl_draw_now(void) {
    uint64_t started = time_us_64();
    lv_refr_now(measure_lvgl_display);
    measure_lvgl_frame_us = (uint32_t) (time_us_64() - started);
    return measure_lvgl_frame_us;
}


static void measure_lvgl_task(void *p) {
    (void) p;
    uint64_t started = time_us_64();
    lv_init();
    lv_tick_set_cb(measure_lvgl_tick);
    measure_lvgl_display = lv_display_create(MEASURE_LVGL_WIDTH, MEASURE_LVGL_HEIGHT);
    lv_display_set_color_format(measure_lvgl_display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(measure_lvgl_display, measure_lvgl_frame, NULL, sizeof(measure_lvgl_frame),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(measure_lvgl_display, measure_lvgl_flush);
    measure_lvgl_build_page();
    measure_lvgl_init_us = (uint32_t) (time_us_64() - started);
    measure_lvgl_draw_now();
    xSemaphoreGive(measure_lvgl_done);

    while (true) {
        measure_lvgl_ask_t ask;
        if (xQueueReceive(measure_lvgl_queue, &ask, pdMS_TO_TICKS(MEASURE_LVGL_TIMER_MS)) == pdTRUE) {
            if (ask == MEASURE_LVGL_NEXT) {
                measure_lvgl_set_focus((measure_lvgl_focus + 1) % MEASURE_LVGL_ROWS);
                measure_lvgl_draw_now();
            }
            else if (ask == MEASURE_LVGL_FULL) {
                lv_obj_invalidate(lv_screen_active());
                measure_lvgl_draw_now();
            }
            xSemaphoreGive(measure_lvgl_done);
        }
        lv_timer_handler();
    }
}


static bool measure_lvgl_start(char *reply, size_t reply_size) {
    if (measure_lvgl_task_handle != NULL) {
        return true;
    }
    measure_lvgl_queue = xQueueCreate(2, sizeof(measure_lvgl_ask_t));
    measure_lvgl_done = xSemaphoreCreateBinary();
    if (measure_lvgl_queue == NULL || measure_lvgl_done == NULL ||
        xTaskCreate(measure_lvgl_task, "LVGL", MEASURE_LVGL_TASK_STACK_WORDS, NULL, MEASURE_LVGL_TASK_PRIORITY,
                    &measure_lvgl_task_handle) != pdPASS) {
        snprintf(reply, reply_size, "@err failed lvgl: no room for its task");
        return false;
    }
    if (xSemaphoreTake(measure_lvgl_done, pdMS_TO_TICKS(MEASURE_LVGL_REPLY_MS)) != pdTRUE) {
        snprintf(reply, reply_size, "@err failed lvgl: its start did not finish");
        return false;
    }
    return true;
}


static bool measure_lvgl_ask(measure_lvgl_ask_t ask, char *reply, size_t reply_size) {
    if (xQueueSend(measure_lvgl_queue, &ask, pdMS_TO_TICKS(MEASURE_LVGL_REPLY_MS)) != pdTRUE ||
        xSemaphoreTake(measure_lvgl_done, pdMS_TO_TICKS(MEASURE_LVGL_REPLY_MS)) != pdTRUE) {
        snprintf(reply, reply_size, "@err failed lvgl: no answer from its task");
        return false;
    }
    return true;
}


static void measure_lvgl_print_frame(remote_control_write_fn write, void *write_ctx) {
    char line[8 + 2 * MEASURE_LVGL_ROW_BYTES + 1];
    for (int row = 0; row < MEASURE_LVGL_HEIGHT; row += 1) {
        const uint8_t *bytes = measure_lvgl_frame + MEASURE_LVGL_PALETTE_BYTES + row * MEASURE_LVGL_ROW_BYTES;
        size_t len = snprintf(line, sizeof(line), "@lv ");
        for (int idx = 0; idx < MEASURE_LVGL_ROW_BYTES; idx += 1) {
            len += snprintf(line + len, sizeof(line) - len, "%02x", bytes[idx]);
        }
        write(line, write_ctx);
    }
}


bool measure_lvgl_console(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                          void *write_ctx) {
    if (!measure_lvgl_start(reply, reply_size)) {
        return false;
    }
    if (strcmp(args, "next") == 0 && !measure_lvgl_ask(MEASURE_LVGL_NEXT, reply, reply_size)) {
        return false;
    }
    if (strcmp(args, "full") == 0 && !measure_lvgl_ask(MEASURE_LVGL_FULL, reply, reply_size)) {
        return false;
    }
    if (strcmp(args, "frame") == 0) {
        measure_lvgl_print_frame(write, write_ctx);
    }

    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    snprintf(reply, reply_size, "@ok lvgl heap_size=%u heap_max_used=%u heap_used_now=%u frag=%u stack_left=%u "
             "init_us=%lu frame_us=%lu frames=%lu focus=%d",
             (unsigned) mem.total_size, (unsigned) mem.max_used, (unsigned) (mem.total_size - mem.free_size),
             (unsigned) mem.frag_pct, (unsigned) (uxTaskGetStackHighWaterMark(measure_lvgl_task_handle) * 4),
             (unsigned long) measure_lvgl_init_us, (unsigned long) measure_lvgl_frame_us,
             (unsigned long) measure_lvgl_frames, measure_lvgl_focus);
    return true;
}
