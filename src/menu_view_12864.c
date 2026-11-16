/*
    The mini 12864's view (ADR-010, gate 4). A fixed set of labels on a black screen, white text, the focused row in
    reverse, as the menu was drawn by u8g2 (its fonts, made from their BDF sources). White is a lit pixel.

    Layout, 128 x 64:
      0-10    the title in bold, "< " before it when it is a Back target; the marks at the right
      11      a line
      13-62   a list: five rows of 10 pixels, label left, value right; ">" for a row that opens something
              the editor: the number in ProFont 22, a bar under the digit being set, a hint
              a job: its state, its figure in ProFont 22, two lines, what a click does at the right of the last
*/
#include <stdio.h>
#include <string.h>

#include "menu_view_12864.h"

#define VIEW_WIDTH 128
#define VIEW_TITLE_H 11
#define VIEW_ROWS_Y 13
#define VIEW_ROW_H 10
#define VIEW_ROWS 5
#define VIEW_MARGIN 2

extern const lv_font_t menu_font_regular;
extern const lv_font_t menu_font_bold;
extern const lv_font_t menu_font_large;

static lv_obj_t *view_title_box;
static lv_obj_t *view_title;
static lv_obj_t *view_marks;
static lv_obj_t *view_rule;
static lv_obj_t *view_row_box[VIEW_ROWS];
static lv_obj_t *view_row_label[VIEW_ROWS];
static lv_obj_t *view_row_value[VIEW_ROWS];
static lv_obj_t *view_big;
static lv_obj_t *view_bar;
static lv_obj_t *view_lines[4];         // a job's state and lines, or the editor's hint
static lv_obj_t *view_action;
static uint32_t view_version = 0;
static int view_first = 0;              // the list's first row shown


static lv_obj_t *view_box(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    return box;
}


static lv_obj_t *view_label(lv_obj_t *parent, const lv_font_t *font, int x, int y, int w, lv_text_align_t align) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_label_set_text(label, "");
    return label;
}


// A text's width in a font: its glyphs' widths summed (LVGL's own measure is not in its public headers)
static int view_text_width(const char *text, size_t length, const lv_font_t *font) {
    int width = 0;
    for (size_t idx = 0; idx < length && text[idx] != '\0'; idx += 1) {
        width += (int) lv_font_get_glyph_width(font, (uint8_t) text[idx], (uint8_t) text[idx + 1]);
    }
    return width;
}


// A text set only when it changed: LVGL draws again only what changed
static void view_text(lv_obj_t *label, const char *text) {
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}


static void view_show(lv_obj_t *obj, bool shown) {
    lv_obj_set_hidden(obj, !shown);
}


// A box in reverse (lit, its text dark), or plain
static void view_reverse(lv_obj_t *box, lv_obj_t *label, lv_obj_t *value, bool reverse) {
    lv_obj_set_style_bg_opa(box, reverse ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_color_t text = reverse ? lv_color_black() : lv_color_white();
    lv_obj_set_style_text_color(label, text, 0);
    if (value != NULL) {
        lv_obj_set_style_text_color(value, text, 0);
    }
}


void menu_view_init(lv_display_t *display) {
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(screen, false);

    view_title_box = view_box(screen, 0, 0, VIEW_WIDTH, VIEW_TITLE_H);
    view_title = view_label(view_title_box, &menu_font_bold, VIEW_MARGIN, 0, VIEW_WIDTH - VIEW_MARGIN, LV_TEXT_ALIGN_LEFT);
    view_marks = view_label(view_title_box, &menu_font_regular, 64, 0, VIEW_WIDTH - 64 - VIEW_MARGIN,
                            LV_TEXT_ALIGN_RIGHT);
    view_rule = view_box(screen, 0, VIEW_TITLE_H, VIEW_WIDTH, 1);
    lv_obj_set_style_bg_opa(view_rule, LV_OPA_COVER, 0);

    for (int idx = 0; idx < VIEW_ROWS; idx += 1) {
        view_row_box[idx] = view_box(screen, 0, VIEW_ROWS_Y + idx * VIEW_ROW_H, VIEW_WIDTH, VIEW_ROW_H);
        view_row_label[idx] = view_label(view_row_box[idx], &menu_font_regular, VIEW_MARGIN, -1,
                                         VIEW_WIDTH - 2 * VIEW_MARGIN, LV_TEXT_ALIGN_LEFT);
        view_row_value[idx] = view_label(view_row_box[idx], &menu_font_regular, 40, -1, VIEW_WIDTH - 40 - VIEW_MARGIN,
                                         LV_TEXT_ALIGN_RIGHT);
    }

    view_big = view_label(screen, &menu_font_large, 0, 22, VIEW_WIDTH, LV_TEXT_ALIGN_CENTER);
    view_bar = view_box(screen, 0, 44, 10, 2);
    lv_obj_set_style_bg_opa(view_bar, LV_OPA_COVER, 0);
    view_lines[0] = view_label(screen, &menu_font_regular, VIEW_MARGIN, 12, VIEW_WIDTH - VIEW_MARGIN, LV_TEXT_ALIGN_LEFT);
    view_lines[1] = view_label(screen, &menu_font_regular, VIEW_MARGIN, 44, VIEW_WIDTH - VIEW_MARGIN, LV_TEXT_ALIGN_LEFT);
    view_lines[2] = view_label(screen, &menu_font_regular, VIEW_MARGIN, 54, 80, LV_TEXT_ALIGN_LEFT);
    view_lines[3] = view_label(screen, &menu_font_regular, VIEW_MARGIN, 54, VIEW_WIDTH - VIEW_MARGIN,
                               LV_TEXT_ALIGN_LEFT);
    view_action = view_label(screen, &menu_font_bold, 64, 54, VIEW_WIDTH - 64 - VIEW_MARGIN, LV_TEXT_ALIGN_RIGHT);
}


static void view_title_and_marks(const menu_screen_t *screen, bool focused) {
    char title[MENU_TEXT + 4];
    snprintf(title, sizeof(title), "%s%s", screen->title_back ? "< " : "", screen->title);
    view_text(view_title, title);
    char marks[16];
    snprintf(marks, sizeof(marks), "%s%s%s", screen->mark_simulated ? "SIM " : "", screen->mark_remote ? "R " : "",
             screen->mark_station ? "W" : (screen->mark_access_point ? "AP" : ""));
    view_text(view_marks, marks);
    // The marks are drawn over the title's end: the title is clipped short of them
    lv_obj_set_width(view_title, VIEW_WIDTH - VIEW_MARGIN - view_text_width(marks, strlen(marks), &menu_font_regular)
                     - 4);
    view_reverse(view_title_box, view_title, view_marks, focused);
}


static void view_list(const menu_screen_t *screen) {
    // The first row shown keeps the focus in sight; a new screen starts at its top
    int focus = screen->focus < 0 ? 0 : screen->focus;
    if (focus < view_first) {
        view_first = focus;
    }
    if (focus >= view_first + VIEW_ROWS) {
        view_first = focus - VIEW_ROWS + 1;
    }
    if (view_first > screen->row_count - VIEW_ROWS) {
        view_first = screen->row_count > VIEW_ROWS ? screen->row_count - VIEW_ROWS : 0;
    }
    for (int idx = 0; idx < VIEW_ROWS; idx += 1) {
        int row = view_first + idx;
        bool shown = row < screen->row_count;
        view_show(view_row_box[idx], shown);
        if (!shown) {
            continue;
        }
        view_text(view_row_label[idx], screen->row_label[row]);
        const char *value = screen->row_value[row];
        view_text(view_row_value[idx], value[0] == '\0' && screen->row_opens[row] ? ">" : value);
        view_reverse(view_row_box[idx], view_row_label[idx], view_row_value[idx], row == screen->focus);
    }
}


static void view_edit(const menu_screen_t *screen) {
    view_text(view_big, screen->edit_text);
    // The bar under the digit being set: the text is centred, so its start is half the space left
    int width = view_text_width(screen->edit_text, strlen(screen->edit_text), &menu_font_large);
    int x = (VIEW_WIDTH - width) / 2;
    int at = view_text_width(screen->edit_text, (size_t) screen->edit_at, &menu_font_large);
    int digit = (int) lv_font_get_glyph_width(&menu_font_large, (uint32_t) screen->edit_text[screen->edit_at], 0);
    lv_obj_set_pos(view_bar, x + at, 45);
    lv_obj_set_width(view_bar, digit);
    view_text(view_lines[3], "Turn: set  Click: next");
}


static void view_job(const menu_screen_t *screen) {
    view_text(view_lines[0], screen->job_state);
    view_text(view_big, screen->job_big);
    view_text(view_lines[1], screen->job_line[0]);
    view_text(view_lines[2], screen->job_line[1]);
    char action[MENU_TEXT + 4];
    if (screen->job_action[0] == '\0') {
        action[0] = '\0';
    }
    else {
        snprintf(action, sizeof(action), screen->job_actions_more ? "<%s>" : "[%s]", screen->job_action);
    }
    view_text(view_action, action);
}


void menu_view_draw(const menu_screen_t *screen) {
    if (screen->version != view_version) {
        view_version = screen->version;
        view_first = 0;
    }
    bool list = screen->kind == MENU_SCREEN_LIST;
    bool edit = screen->kind == MENU_SCREEN_EDIT;
    bool job = screen->kind == MENU_SCREEN_JOB;

    view_title_and_marks(screen, list && screen->focus < 0);
    for (int idx = 0; idx < VIEW_ROWS && !list; idx += 1) {
        view_show(view_row_box[idx], false);
    }
    view_show(view_big, edit || job);
    view_show(view_bar, edit);
    view_show(view_lines[0], job);
    view_show(view_lines[1], job);
    view_show(view_lines[2], job);
    view_show(view_lines[3], edit);
    view_show(view_action, job);

    if (list) {
        view_list(screen);
    }
    else if (edit) {
        view_edit(screen);
    }
    else {
        view_job(screen);
    }
}
