/*
    The menu's task (ADR-010, gate 4). It owns LVGL and the panel: each frame LVGL draws into one bit a pixel is turned
    into the UC1701's columns, written into u8g2's buffer under the buffer's lock, and sent (u8g2 is the panel's driver
    only). The knob and the buttons become four keys:
      the knob a detent clockwise or anticlockwise: Next, Prev;
      a click of the knob, released within MENU_HOLD_MS: Enter;
      the reset button, or the knob held MENU_HOLD_MS: Back (at once, while held).
    A start asked from anywhere is taken here and begun in the job task; the job's screen is the menu's until it ends.
*/
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "lvgl/lvgl.h"
#include "pico/time.h"
#include "u8g2.h"

#include "activity.h"
#include "display.h"
#include "jobs.h"
#include "menu.h"
#include "menu_nav.h"
#include "menu_view_12864.h"
#include "mini_12864_module.h"

#define MENU_WIDTH 128
#define MENU_HEIGHT 64
#define MENU_FRAME_BYTES (MENU_WIDTH * MENU_HEIGHT / 8)
// LVGL's one-bit frame starts with its palette: two colours of 4 bytes
#define MENU_PALETTE_BYTES 8
// A hold of the knob this long is Back
#define MENU_HOLD_MS 600
// How often the screen's live values are read again: a job's weight often, a page's values less
#define MENU_REFRESH_JOB_MS 100
#define MENU_REFRESH_PAGE_MS 250
// How often LVGL's heap is looked at, for `screen`
#define MENU_HEAP_LOOK_MS 2000

extern QueueHandle_t encoder_event_queue;

static uint8_t menu_frame[MENU_FRAME_BYTES + MENU_PALETTE_BYTES] __attribute__((aligned(4)));
static lv_display_t *menu_display = NULL;

// What the console sees: the screen last drawn, copied under its lock
static SemaphoreHandle_t menu_screen_lock = NULL;
static menu_screen_t menu_screen_drawn;
static menu_screen_t menu_screen_now;
static uint32_t menu_lvgl_heap_max = 0;

// The remote control's `screen <id>`: asked from its task, answered after the draw
static volatile int menu_current_form_id = -1;
static char menu_goto_id[32];
static volatile bool menu_goto_found = false;
static SemaphoreHandle_t menu_goto_drawn = NULL;


static uint32_t menu_tick(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


/*
    A frame drawn: into u8g2's buffer as the UC1701 takes it, then to the panel. LVGL's rows are bytes of 8 pixels
    across, the leftmost the top bit; u8g2's pages are 8 rows deep, a byte a column, the top row the lowest bit.
    (LVGL's own lv_draw_i1_convert_to_vtiled() runs each column the whole height: not u8g2's pages)
*/
static void menu_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    (void) area;
    u8g2_t *u8g2 = get_display_handler();
    const uint8_t *rows = px_map + MENU_PALETTE_BYTES;
    acquire_display_buffer_access();
    uint8_t *pages = u8g2_GetBufferPtr(u8g2);
    memset(pages, 0, MENU_FRAME_BYTES);
    for (int y = 0; y < MENU_HEIGHT; y += 1) {
        const uint8_t *row = rows + y * (MENU_WIDTH / 8);
        uint8_t *page = pages + (y / 8) * MENU_WIDTH;
        uint8_t bit = (uint8_t) (1u << (y % 8));
        for (int x = 0; x < MENU_WIDTH; x += 1) {
            if (row[x / 8] & (0x80u >> (x % 8))) {
                page[x] |= bit;
            }
        }
    }
    u8g2_SendBuffer(u8g2);
    release_display_buffer_access();
    lv_display_flush_ready(display);
}


int menu_get_current_form_id(void) {
    return menu_current_form_id;
}


void menu_wake(void) {
    ButtonEncoderEvent_t event = MENU_WAKE;
    if (encoder_event_queue != NULL) {
        xQueueSend(encoder_event_queue, &event, 0);
    }
}


bool menu_goto_screen(const char *id, bool *found) {
    if (menu_goto_drawn == NULL || encoder_event_queue == NULL) {
        return false;
    }
    // An answer left over from a call that timed out must not be taken for this one
    xSemaphoreTake(menu_goto_drawn, 0);
    snprintf(menu_goto_id, sizeof(menu_goto_id), "%s", id);
    ButtonEncoderEvent_t event = MENU_REMOTE_SCREEN;
    if (xQueueSend(encoder_event_queue, &event, pdMS_TO_TICKS(100)) != pdTRUE ||
        xSemaphoreTake(menu_goto_drawn, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    *found = menu_goto_found;
    return true;
}


void menu_screen_copy(menu_screen_t *screen, uint32_t *lvgl_heap_max) {
    if (menu_screen_lock == NULL) {
        memset(screen, 0, sizeof(*screen));
        *lvgl_heap_max = 0;
        return;
    }
    xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
    *screen = menu_screen_drawn;
    *lvgl_heap_max = menu_lvgl_heap_max;
    xSemaphoreGive(menu_screen_lock);
}


bool menu_hold(uint32_t ms) {
    ButtonEncoderEvent_t down = BUTTON_ENCODER_DOWN;
    ButtonEncoderEvent_t up = BUTTON_ENCODER_UP;
    if (encoder_event_queue == NULL || xQueueSend(encoder_event_queue, &down, pdMS_TO_TICKS(500)) != pdTRUE) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
    return xQueueSend(encoder_event_queue, &up, pdMS_TO_TICKS(500)) == pdTRUE;
}


// The knob's button: a click is Enter at its release; a hold is Back once it is held MENU_HOLD_MS
static bool menu_knob_down = false;
static bool menu_knob_held = false;
static uint64_t menu_knob_down_us = 0;


static void menu_event(ButtonEncoderEvent_t event) {
    switch (event) {
        case BUTTON_ENCODER_ROTATE_CW:
            menu_nav_key(MENU_KEY_NEXT);
            break;
        case BUTTON_ENCODER_ROTATE_CCW:
            menu_nav_key(MENU_KEY_PREV);
            break;
        case BUTTON_ENCODER_PRESSED:
            menu_nav_key(MENU_KEY_ENTER);
            break;
        case BUTTON_RST_PRESSED:
            menu_nav_key(MENU_KEY_BACK);
            break;
        case BUTTON_ENCODER_DOWN:
            menu_knob_down = true;
            menu_knob_held = false;
            menu_knob_down_us = time_us_64();
            break;
        case BUTTON_ENCODER_UP:
            if (menu_knob_down && !menu_knob_held) {
                menu_nav_key(MENU_KEY_ENTER);
            }
            menu_knob_down = false;
            break;
        case MENU_REMOTE_SCREEN:
            menu_goto_found = menu_nav_goto(menu_goto_id);
            break;
        default:
            break;
    }
}


// The wait for the next event: the refresh, or sooner when a hold is due
static uint32_t menu_wait_ms(void) {
    uint32_t wait = menu_nav_in_job() ? MENU_REFRESH_JOB_MS : MENU_REFRESH_PAGE_MS;
    if (menu_knob_down && !menu_knob_held) {
        uint64_t held_ms = (time_us_64() - menu_knob_down_us) / 1000;
        uint32_t left = held_ms >= MENU_HOLD_MS ? 0 : (uint32_t) (MENU_HOLD_MS - held_ms);
        wait = left < wait ? left : wait;
    }
    return wait;
}


static void menu_jobs(void) {
    activity_t job;
    if (!menu_nav_in_job() && activity_take_start(&job)) {
        if (jobs_begin(job)) {
            menu_nav_job_begun(job);
        }
        else {
            printf("Menu: %s not begun, a job runs\n", activity_name(job));
            activity_clear_start();
        }
    }
    uint8_t exit_form;
    if (jobs_take_end(&exit_form)) {
        // A start the job did not begin (a driver not OK) is done with: it never starts later
        activity_clear_start();
        menu_nav_job_ended(exit_form);
    }
}


static void menu_draw(bool goto_asked) {
    menu_nav_screen(&menu_screen_now);
    menu_view_draw(&menu_screen_now);
    lv_timer_handler();

    menu_current_form_id = menu_nav_in_job() ? -1 : 0;
    xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
    menu_screen_drawn = menu_screen_now;
    xSemaphoreGive(menu_screen_lock);
    if (goto_asked) {
        // Answered after the draw, so a snap taken next shows the page
        xSemaphoreGive(menu_goto_drawn);
    }
}


void menu_task(void *p) {
    (void) p;
    menu_screen_lock = xSemaphoreCreateMutex();
    menu_goto_drawn = xSemaphoreCreateBinary();

    lv_init();
    lv_tick_set_cb(menu_tick);
    menu_display = lv_display_create(MENU_WIDTH, MENU_HEIGHT);
    lv_display_set_color_format(menu_display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(menu_display, menu_frame, NULL, sizeof(menu_frame), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(menu_display, menu_flush);
    menu_view_init(menu_display);
    menu_nav_init();
    menu_draw(false);

    uint64_t heap_looked_us = 0;
    while (true) {
        ButtonEncoderEvent_t event = button_wait_for_input_ms(menu_wait_ms());
        bool goto_asked = event == MENU_REMOTE_SCREEN;
        if (event != BUTTON_NO_EVENT) {
            menu_event(event);
        }
        if (menu_knob_down && !menu_knob_held &&
            (time_us_64() - menu_knob_down_us) / 1000 >= MENU_HOLD_MS) {
            menu_knob_held = true;
            menu_nav_key(MENU_KEY_BACK);
        }
        menu_jobs();
        menu_draw(goto_asked);

        if (time_us_64() - heap_looked_us >= (uint64_t) MENU_HEAP_LOOK_MS * 1000u) {
            heap_looked_us = time_us_64();
            lv_mem_monitor_t heap;
            lv_mem_monitor(&heap);
            xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
            menu_lvgl_heap_max = (uint32_t) heap.max_used;
            xSemaphoreGive(menu_screen_lock);
        }
    }
}
