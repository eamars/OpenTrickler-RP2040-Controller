/*
    The menu's task (ADR-010, gate 4). It owns LVGL and the screen's one frame: LVGL draws only what changed into it,
    under the frame's lock, and the pages of the panel that changed are sent (display.c). The knob and the buttons
    become four keys:
      the knob a detent clockwise or anticlockwise: Next, Prev;
      a click of the knob, taken as it goes down: Enter;
      the reset button: Back. A click on a title's "<" is Back as well (menu_nav.c). No long press (the owner).
    A start asked from anywhere is taken here and begun in the job task; the job's screen is the menu's until it ends.
*/
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "lvgl/lvgl.h"
#include "pico/time.h"

#include "activity.h"
#include "display.h"
#include "jobs.h"
#include "menu.h"
#include "menu_nav.h"
#include "menu_view_12864.h"
#include "mini_12864_module.h"

// LVGL's one-bit frame starts with its palette: two colours of 4 bytes
#define MENU_PALETTE_BYTES 8
// The whole frame is sent again this often, so a glitch on the panel's cable does not stay on the glass
#define MENU_RESEND_MS 1000
// How often the screen's live values are read again: a job's weight often, a page's values less
#define MENU_REFRESH_JOB_MS 100
#define MENU_REFRESH_PAGE_MS 250
// How often LVGL's heap is looked at, for `screen`
#define MENU_HEAP_LOOK_MS 2000
// LVGL's heap beyond its own (LV_MEM_SIZE, 14 KB, in the image's RAM, which is full): a second pool from the FreeRTOS
// heap. With the hello's overlay made at the start, the 14 KB were at most 11.1 KB used and 736 bytes free in one
// piece at the least (the Pico W, 2026-11-17), where a draw takes up to 384 bytes and an allocation LVGL cannot make
// faults the core. It is taken from what an HTTPS poll leaves the FreeRTOS heap at its low mark: 8,168 bytes with the
// local test chains, about 9.5 KB with GitHub's (ADR-010, gate 5), so about 5 and 6.5 KB with this pool. 6 KB was
// tried first: 7 KB in one piece for LVGL, but 2 to 3.5 KB left to a fetch
#define MENU_LVGL_POOL_BYTES (3 * 1024)

extern QueueHandle_t encoder_event_queue;
extern mini_12864_module_config_t mini_12864_module_config;

// The screen's one frame, LVGL's: no second buffer, since the panel keeps the frame it shows (ADR-010, gate 4)
static uint8_t menu_frame[MENU_PALETTE_BYTES + DISPLAY_FRAME_BYTES] __attribute__((aligned(4)));
static lv_display_t *menu_display = NULL;
// Held while LVGL draws and while the frame is copied, so a copy is one whole frame
static SemaphoreHandle_t menu_frame_lock = NULL;
// The panel's pages a draw changed, sent with the draw's last area
static uint8_t menu_frame_pages = 0;
static menu_frame_times_t menu_times;
static uint32_t menu_send_last_us = 0;
// The panel shown inverted (its own command), changed under the frame's lock so a copy of the frame matches it: the
// stored setting, or a hello's own
static bool menu_panel_inverted = false;

// What the console sees: the screen last drawn, copied under its lock
static SemaphoreHandle_t menu_screen_lock = NULL;
static menu_screen_t menu_screen_drawn;
static menu_screen_t menu_screen_now;
static uint32_t menu_lvgl_heap_max = 0;
// The least LVGL's heap had free in one piece: a glyph is drawn through a buffer taken from it (up to 384 bytes)
static uint32_t menu_lvgl_biggest_free_min = UINT32_MAX;

// The hidden hello: asked from the web's task (menu_visit()), played here a step at a time. Negative steps are the
// flash: the panel inverted, then not, twice. The figure is then shown on a lit panel, whatever the setting
#define MENU_VISIT_STEP_MS 200
#define MENU_VISIT_FLASH_STEPS 4
static char menu_visit_asked_name[MENU_VISIT_NAME_MAX + 1];
static bool menu_visit_asked = false;
static bool menu_visit_asked_flash = false;
static uint64_t menu_visit_last_us = 0;
static bool menu_visiting = false;
static char menu_visit_name[MENU_VISIT_NAME_MAX + 1];
static int menu_visit_step = 0;
static uint64_t menu_visit_step_us = 0;

// The remote control's `screen <id>`: asked from its task, answered after the draw
static volatile int menu_current_form_id = -1;
static char menu_goto_id[32];
static volatile bool menu_goto_found = false;
static SemaphoreHandle_t menu_goto_drawn = NULL;


static uint32_t menu_tick(void) {
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}


// The frame's pages sent, and how long that took
static void menu_send(uint8_t pages) {
    uint64_t start_us = time_us_64();
    display_send_pages(menu_frame + MENU_PALETTE_BYTES, pages);
    menu_send_last_us = (uint32_t) (time_us_64() - start_us);
    menu_times.send_us = menu_send_last_us;
    if (menu_send_last_us > menu_times.send_max_us) {
        menu_times.send_max_us = menu_send_last_us;
    }
}


/*
    An area LVGL drew (direct mode: into the one frame, which keeps the rest). Its pages are noted, and sent with the
    draw's last area. LVGL's rows are bytes of 8 pixels across; the panel's pages are 8 rows deep (display.c)
*/
static void menu_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    (void) px_map;
    menu_view_paint(menu_frame + MENU_PALETTE_BYTES, area);
    int first = area->y1 < 0 ? 0 : area->y1 / 8;
    int last = area->y2 >= DISPLAY_HEIGHT ? DISPLAY_PAGES - 1 : area->y2 / 8;
    for (int page = first; page <= last; page += 1) {
        menu_frame_pages |= (uint8_t) (1u << page);
    }
    if (lv_display_flush_is_last(display)) {
        menu_send(menu_frame_pages);
        menu_frame_pages = 0;
    }
    lv_display_flush_ready(display);
}


// The frame as the panel shows it: inverted with it
void menu_frame_copy(uint8_t *out) {
    xSemaphoreTake(menu_frame_lock, portMAX_DELAY);
    memcpy(out, menu_frame + MENU_PALETTE_BYTES, DISPLAY_FRAME_BYTES);
    if (menu_panel_inverted) {
        for (size_t idx = 0; idx < DISPLAY_FRAME_BYTES; idx += 1) {
            out[idx] = (uint8_t) ~out[idx];
        }
    }
    xSemaphoreGive(menu_frame_lock);
}


// The panel inverted or not, sent only when it changes
static void menu_panel_invert(bool inverted) {
    if (inverted == menu_panel_inverted) {
        return;
    }
    xSemaphoreTake(menu_frame_lock, portMAX_DELAY);
    display_invert(inverted);
    menu_panel_inverted = inverted;
    xSemaphoreGive(menu_frame_lock);
}


void menu_frame_times(menu_frame_times_t *times) {
    xSemaphoreTake(menu_frame_lock, portMAX_DELAY);
    *times = menu_times;
    xSemaphoreGive(menu_frame_lock);
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
    xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
    *screen = menu_screen_drawn;
    *lvgl_heap_max = menu_lvgl_heap_max;
    xSemaphoreGive(menu_screen_lock);
}


uint32_t menu_lvgl_biggest_free(void) {
    return menu_lvgl_biggest_free_min;
}


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
        case MENU_REMOTE_SCREEN:
            menu_goto_found = menu_nav_goto(menu_goto_id);
            break;
        default:
            break;
    }
}


bool menu_visit(const char *name, bool *flash) {
    // Everywhere but over a charge (the owner, 2026-11-17)
    if (activity_get() == ACTIVITY_CHARGE) {
        return false;
    }
    xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
    uint64_t now_us = time_us_64();
    *flash = menu_visit_last_us == 0 || now_us - menu_visit_last_us >= (uint64_t) MENU_VISIT_QUIET_MS * 1000u;
    menu_visit_last_us = now_us;
    snprintf(menu_visit_asked_name, sizeof(menu_visit_asked_name), "%s", name);
    menu_visit_asked_flash = *flash;
    menu_visit_asked = true;
    xSemaphoreGive(menu_screen_lock);
    menu_wake();
    return true;
}


static void menu_visit_show(void) {
    if (menu_visit_step < 0) {
        menu_panel_invert(menu_visit_step % 2 == 0);
        return;
    }
    menu_panel_invert(true);
    if (!menu_view_visit(menu_visit_step, menu_visit_name)) {
        menu_visiting = false;
    }
}


// The hello ended, whole or cut short: the panel as it is, the page back
static void menu_visit_end(void) {
    if (!menu_visiting) {
        return;
    }
    menu_visiting = false;
    menu_panel_invert(mini_12864_module_config.inverted_display);
    menu_view_visit_end();
}


// A hello asked is begun, unless a charge has the screen; one playing goes on a step when its time is up
static void menu_visit_run(void) {
    xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
    bool asked = menu_visit_asked;
    bool flash = menu_visit_asked_flash;
    if (asked) {
        memcpy(menu_visit_name, menu_visit_asked_name, sizeof(menu_visit_name));
        menu_visit_asked = false;
    }
    xSemaphoreGive(menu_screen_lock);

    uint64_t now_us = time_us_64();
    if (activity_get() == ACTIVITY_CHARGE) {
        menu_visit_end();
        return;
    }
    if (asked) {
        menu_view_visit_end();
        menu_visiting = true;
        menu_visit_step = flash ? -MENU_VISIT_FLASH_STEPS : 0;
        menu_visit_step_us = now_us;
        menu_visit_show();
        return;
    }
    if (menu_visiting && now_us - menu_visit_step_us >= (uint64_t) MENU_VISIT_STEP_MS * 1000u) {
        menu_visit_step += 1;
        menu_visit_step_us = now_us;
        if (menu_visit_step >= MENU_VIEW_VISIT_STEPS) {
            menu_visit_end();
        }
        else {
            menu_visit_show();
        }
    }
}


// The wait for the next event: the refresh, or sooner when a hello's next step is due
static uint32_t menu_wait_ms(void) {
    uint32_t wait = menu_nav_in_job() ? MENU_REFRESH_JOB_MS : MENU_REFRESH_PAGE_MS;
    if (menu_visiting) {
        uint64_t spent_ms = (time_us_64() - menu_visit_step_us) / 1000;
        uint32_t left = spent_ms >= MENU_VISIT_STEP_MS ? 0 : (uint32_t) (MENU_VISIT_STEP_MS - spent_ms);
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

    // LVGL draws what changed and sends it (menu_flush), under the frame's lock: the draw's time is the rest
    xSemaphoreTake(menu_frame_lock, portMAX_DELAY);
    menu_send_last_us = 0;
    uint64_t start_us = time_us_64();
    lv_timer_handler();
    uint32_t took_us = (uint32_t) (time_us_64() - start_us);
    if (menu_send_last_us > 0) {
        menu_times.frames += 1;
        menu_times.draw_us = took_us - menu_send_last_us;
        if (menu_times.draw_us > menu_times.draw_max_us) {
            menu_times.draw_max_us = menu_times.draw_us;
        }
    }
    xSemaphoreGive(menu_frame_lock);

    menu_current_form_id = menu_nav_in_job() ? -1 : 0;
    xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
    menu_screen_drawn = menu_screen_now;
    xSemaphoreGive(menu_screen_lock);
    if (goto_asked) {
        // Answered after the draw, so a snap taken next shows the page
        xSemaphoreGive(menu_goto_drawn);
    }
}


void menu_init(void) {
    menu_screen_lock = xSemaphoreCreateMutex();
    menu_goto_drawn = xSemaphoreCreateBinary();
    menu_frame_lock = xSemaphoreCreateMutex();
    if (menu_screen_lock == NULL || menu_goto_drawn == NULL || menu_frame_lock == NULL) {
        assert(false);
    }
}


void menu_task(void *p) {
    (void) p;
    lv_init();
    void *pool = pvPortMalloc(MENU_LVGL_POOL_BYTES);
    if (pool == NULL || lv_mem_add_pool(pool, MENU_LVGL_POOL_BYTES) == NULL) {
        assert(false);
    }
    lv_tick_set_cb(menu_tick);
    menu_display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_color_format(menu_display, LV_COLOR_FORMAT_I1);
    // Direct: LVGL draws only the areas that changed, into the one frame, which keeps the rest
    lv_display_set_buffers(menu_display, menu_frame, NULL, sizeof(menu_frame), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(menu_display, menu_flush);
    menu_view_init(menu_display);
    menu_nav_init();
    menu_panel_invert(mini_12864_module_config.inverted_display);
    menu_draw(false);

    uint64_t heap_looked_us = 0;
    uint64_t resent_us = time_us_64();
    while (true) {
        ButtonEncoderEvent_t event = button_wait_for_input_ms(menu_wait_ms());
        bool goto_asked = event == MENU_REMOTE_SCREEN;
        // A hello ends at a press, and the press does nothing else: the person did not see the page under it. But
        // over a job the reset button still stops it
        bool pressed = event != BUTTON_NO_EVENT && event != MENU_WAKE && event != MENU_REMOTE_SCREEN;
        if (menu_visiting && (pressed || goto_asked)) {
            menu_visit_end();
            bool stop = event == BUTTON_RST_PRESSED && menu_nav_in_job();
            if (pressed && !stop) {
                event = BUTTON_NO_EVENT;
            }
        }
        if (event != BUTTON_NO_EVENT) {
            menu_event(event);
        }
        menu_jobs();
        menu_visit_run();
        if (!menu_visiting) {
            menu_panel_invert(mini_12864_module_config.inverted_display);
        }
        menu_nav_refresh();
        menu_draw(goto_asked);

        if (time_us_64() - resent_us >= (uint64_t) MENU_RESEND_MS * 1000u) {
            resent_us = time_us_64();
            xSemaphoreTake(menu_frame_lock, portMAX_DELAY);
            menu_send(DISPLAY_ALL_PAGES);
            xSemaphoreGive(menu_frame_lock);
        }

        if (time_us_64() - heap_looked_us >= (uint64_t) MENU_HEAP_LOOK_MS * 1000u) {
            heap_looked_us = time_us_64();
            lv_mem_monitor_t heap;
            lv_mem_monitor(&heap);
            xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
            menu_lvgl_heap_max = (uint32_t) heap.max_used;
            if (heap.free_biggest_size < menu_lvgl_biggest_free_min) {
                menu_lvgl_biggest_free_min = (uint32_t) heap.free_biggest_size;
            }
            xSemaphoreGive(menu_screen_lock);
        }
    }
}
