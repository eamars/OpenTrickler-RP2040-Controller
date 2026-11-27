/*
    The menu's task (ADR-010, gate 4). It draws through the view the start-up handed it (menu_view.h), which owns the
    drawing library and the panel's frame (ADR-006, gate 6). The knob and the buttons become four keys:
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

#include "chip.h"

#include "activity.h"
#include "api_v1.h"
#include "jobs.h"
#include "menu.h"
#include "menu_nav.h"
#include "menu_view.h"
#include "mini_12864_module.h"

// The whole frame is sent again this often, so a glitch on the panel's cable does not stay on the glass
#define MENU_RESEND_MS 1000
// How often the screen's live values are read again: a job's weight often, a page's values less
#define MENU_REFRESH_JOB_MS 100
#define MENU_REFRESH_PAGE_MS 250
// How often LVGL's heap is looked at, for `screen`
#define MENU_HEAP_LOOK_MS 2000

extern QueueHandle_t encoder_event_queue;
extern mini_12864_module_config_t mini_12864_module_config;

// The build's one view, handed over by the start-up's wiring before menu_init()
static const menu_view_t *menu_view = NULL;

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


void menu_set_view(const menu_view_t *view) {
    menu_view = view;
}


// The frame as the panel shows it: the view's
void menu_frame_copy(uint8_t *out) {
    menu_view->frame_copy(out);
}


void menu_frame_info(uint16_t *width, uint16_t *height, size_t *bytes) {
    *width = menu_view->width;
    *height = menu_view->height;
    *bytes = menu_view->frame_bytes;
}


// The panel inverted or not: the view sends it only when it changes
static void menu_panel_invert(bool inverted) {
    menu_view->set_inverted(inverted);
}


void menu_frame_times(menu_frame_times_t *times) {
    menu_view->frame_times(times);
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
    uint64_t now_us = chip_time_us();
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
    if (!menu_view->visit(menu_visit_step, menu_visit_name)) {
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
    menu_view->visit_end();
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

    uint64_t now_us = chip_time_us();
    if (activity_get() == ACTIVITY_CHARGE) {
        menu_visit_end();
        return;
    }
    if (asked) {
        menu_view->visit_end();
        menu_visiting = true;
        menu_visit_step = flash ? -MENU_VISIT_FLASH_STEPS : 0;
        menu_visit_step_us = now_us;
        menu_visit_show();
        return;
    }
    if (menu_visiting && now_us - menu_visit_step_us >= (uint64_t) MENU_VISIT_STEP_MS * 1000u) {
        menu_visit_step += 1;
        menu_visit_step_us = now_us;
        if (menu_visit_step >= menu_view->visit_steps) {
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
        uint64_t spent_ms = (chip_time_us() - menu_visit_step_us) / 1000;
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
    menu_view->draw(&menu_screen_now);

    menu_current_form_id = menu_nav_in_job() ? -1 : 0;
    xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
    menu_screen_drawn = menu_screen_now;
    xSemaphoreGive(menu_screen_lock);
    if (goto_asked) {
        // Answered after the draw, so a snap taken next shows the page
        xSemaphoreGive(menu_goto_drawn);
    }
}


// The screen as the jobs see it (activity.h)
static bool menu_takes_starts(void) {
    return menu_current_form_id >= 0;
}

static const activity_screen_t menu_as_screen = {.wake = menu_wake, .takes_starts = menu_takes_starts};

// And as the API gives it (api_v1.h): the view's frame for the page's mirror, and the hello
_Static_assert(MENU_VISIT_NAME_MAX == API_V1_HELLO_NAME_MAX, "the hello's name: the API's length is the menu's");
static api_v1_screen_t menu_as_api_screen = {.frame_copy = menu_frame_copy, .hello = menu_visit};


void menu_init(void) {
    // The view first: the build's wiring hands one over before this (menu_set_view())
    if (menu_view == NULL || menu_view->frame_bytes > MENU_FRAME_BYTES_MAX) {
        assert(false);
    }
    menu_as_api_screen.width = menu_view->width;
    menu_as_api_screen.height = menu_view->height;
    menu_as_api_screen.frame_bytes = menu_view->frame_bytes;
    menu_as_api_screen.format = menu_view->format;
    activity_set_screen(&menu_as_screen);
    api_v1_set_screen(&menu_as_api_screen);
    menu_screen_lock = xSemaphoreCreateMutex();
    menu_goto_drawn = xSemaphoreCreateBinary();
    if (menu_screen_lock == NULL || menu_goto_drawn == NULL || !menu_view->prepare()) {
        assert(false);
    }
}


void menu_task(void *p) {
    (void) p;
    menu_view->init();
    menu_nav_init();
    menu_panel_invert(mini_12864_module_config.inverted_display);
    menu_draw(false);

    uint64_t heap_looked_us = 0;
    uint64_t resent_us = chip_time_us();
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

        if (chip_time_us() - resent_us >= (uint64_t) MENU_RESEND_MS * 1000u) {
            resent_us = chip_time_us();
            menu_view->resend();
        }

        if (chip_time_us() - heap_looked_us >= (uint64_t) MENU_HEAP_LOOK_MS * 1000u) {
            heap_looked_us = chip_time_us();
            uint32_t max_used;
            uint32_t biggest_free;
            menu_view->heap(&max_used, &biggest_free);
            xSemaphoreTake(menu_screen_lock, portMAX_DELAY);
            menu_lvgl_heap_max = max_used;
            if (biggest_free < menu_lvgl_biggest_free_min) {
                menu_lvgl_biggest_free_min = biggest_free;
            }
            xSemaphoreGive(menu_screen_lock);
        }
    }
}
