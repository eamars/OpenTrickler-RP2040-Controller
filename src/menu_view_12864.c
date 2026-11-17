/*
    The mini 12864's view (ADR-010): the navigator's screen drawn with LVGL on 128 x 64 one-bit pixels, as the skill
    embedded-ui-design sets it and tools/ui_mock/menu_12864.py draws it. White is a lit pixel.

    One family, Helvetica: 8 regular for text, 8 bold for titles and emphasis, 18 bold for a job's figure. The grid:
      0-10    the title in bold ("< " before it when it is a Back target), the marks at the right; its baseline 9
      11      a rule
      13-63   a list: four rows of 13 px, label left, value right, ">" for a row that opens something
              a card or notes: lines of 11 px (the text's 10 and 1), notes ending in their button; a scroll bar
              a dialog: two lines centred, on baselines 29 and 40
              the editor and a job: the figure and its unit on baseline 38, a line under it on 49
      53-63   the softkeys: Back's at the left, the action at the right; the focused one in reverse
    Every text is placed by its baseline, so the faces' heights decide nothing else.
*/
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "menu_view_12864.h"

#define VIEW_W 128
#define VIEW_H 64
#define VIEW_TITLE_BASE 9
#define VIEW_TITLE_H 11
#define VIEW_RULE_Y 11
#define VIEW_CONTENT_TOP 13
#define VIEW_ROW_H 13
#define VIEW_ROWS 4
#define VIEW_LINE_H 11
#define VIEW_LINES 4
#define VIEW_LINE_WIDTH (VIEW_W - 3 - 5)        // a card's or notes' line, clear of the scroll bar
// The charge's chart: its head on baseline 23, the chart from 26 to 51, the softkeys under it
#define VIEW_CHART_HEAD_BASE 23
#define VIEW_CHART_X 2
#define VIEW_CHART_TOP 26
#define VIEW_CHART_W 124
#define VIEW_CHART_H 26
#define VIEW_MESSAGE_BASE_1 29
#define VIEW_MESSAGE_BASE_2 40
#define VIEW_FIGURE_BASE 38
#define VIEW_CONTEXT_BASE 49
#define VIEW_SOFTKEY_TOP 53
#define VIEW_SOFTKEY_BASE 61
#define VIEW_MARGIN 3
// The regular font's icons (scripts/bdf2lvgl.py, ICONS), U+E000 to U+E003 in UTF-8
#define VIEW_ICON_STATION "\xEE\x80\x80"
#define VIEW_ICON_ACCESS_POINT "\xEE\x80\x81"
#define VIEW_ICON_CHOSEN "\xEE\x80\x82"
#define VIEW_ICON_REMOTE "\xEE\x80\x83"

extern const lv_font_t menu_font_regular;
extern const lv_font_t menu_font_bold;
extern const lv_font_t menu_font_figure;
// The hidden hello's figure, fonts/claude_chan.txt in 16 x 16 tiles: four poses of nine, from 'A' (scripts/bdf2lvgl.py)
extern const lv_font_t menu_font_chan;

static lv_obj_t *view_title_box;
static lv_obj_t *view_title;
static lv_obj_t *view_marks;
static lv_obj_t *view_rows[VIEW_ROWS];          // a list's rows, a card's or notes' lines
static lv_obj_t *view_row_label[VIEW_ROWS];
static lv_obj_t *view_row_value[VIEW_ROWS];
static lv_obj_t *view_row_more[VIEW_ROWS];      // ">"
static lv_obj_t *view_button;                   // notes' button
static lv_obj_t *view_button_label;
static lv_obj_t *view_track;
static lv_obj_t *view_thumb;
static lv_obj_t *view_message[2];
static lv_obj_t *view_figure;
static lv_obj_t *view_unit;
static lv_obj_t *view_cursor;
static lv_obj_t *view_edit_box;                 // the editor's digit being set, in reverse
static lv_obj_t *view_edit_digit;
static lv_obj_t *view_context;
// The charge's chart: its head, the powder or the verdict beside it, and its area, painted into the frame by
// menu_view_paint() when LVGL flushes it (LVGL draws no sloped line without its complex drawing)
static lv_obj_t *view_chart;
static lv_obj_t *view_chart_head;
static lv_obj_t *view_chart_note;
static bool view_chart_shown = false;
static uint8_t view_chart_count = 0;
static uint32_t view_chart_step_ms = MENU_CHART_STEP_MS;
static uint8_t view_chart_points[MENU_CHART_POINTS];
static lv_obj_t *view_softkey_left_box;
static lv_obj_t *view_softkey_left;
static lv_obj_t *view_softkey_box;
static lv_obj_t *view_softkey_right;
static void view_visit_create(lv_obj_t *screen);
static void view_bubble_paint(uint8_t *rows, const lv_area_t *area);
static void view_chart_draw(const menu_screen_t *screen, bool shown);
static void view_control(const menu_screen_t *screen);
static uint32_t view_version = 0;
static int view_first = 0;                      // a list's first row shown


// --- Helpers

static lv_obj_t *view_box(lv_obj_t *parent, int x, int y, int w, int h, bool lit) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(box, lit ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    return box;
}


static lv_obj_t *view_label(lv_obj_t *parent, const lv_font_t *font, lv_text_align_t align) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(label, "");
    return label;
}


// A label's top for its text to stand on `base`: its font's ascent above it (as the mock-up places text)
static int view_top(const lv_font_t *font, int base) {
    return base - (int) (font->line_height - font->base_line) + 1;
}


// A label placed: its left edge, its width, its baseline (in its parent)
static void view_place(lv_obj_t *label, int x, int width, int base) {
    const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
    lv_obj_set_pos(label, x, view_top(font, base));
    lv_obj_set_size(label, width, (int) font->line_height);
}


// The character at text[*idx], read as UTF-8 (the icons are three bytes), *idx moved past it
static uint32_t view_next_char(const char *text, size_t *idx) {
    uint8_t first = (uint8_t) text[*idx];
    size_t more = first >= 0xF0 ? 3 : (first >= 0xE0 ? 2 : (first >= 0xC0 ? 1 : 0));
    uint32_t code = more == 0 ? first : first & (0x3Fu >> more);
    *idx += 1;
    for (size_t part = 0; part < more && text[*idx] != '\0'; part += 1) {
        code = (code << 6) | ((uint8_t) text[*idx] & 0x3Fu);
        *idx += 1;
    }
    return code;
}


// A text's width in a font: its glyphs' widths summed (LVGL's own measure is not in its public headers). Length is
// in bytes
static int view_text_width(const char *text, size_t length, const lv_font_t *font) {
    int width = 0;
    size_t idx = 0;
    while (idx < length && text[idx] != '\0') {
        uint32_t code = view_next_char(text, &idx);
        size_t after = idx;
        uint32_t next = idx < length && text[idx] != '\0' ? view_next_char(text, &after) : 0;
        width += (int) lv_font_get_glyph_width(font, code, next);
    }
    return width;
}


// The navigator's measure, for wrapping cards and notes (menu_nav_set_text_width)
static int view_measure(const char *text, bool bold) {
    return view_text_width(text, strlen(text), bold ? &menu_font_bold : &menu_font_regular);
}


// A text set only when it changed: LVGL draws again only what changed
static void view_text(lv_obj_t *label, const char *text) {
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}


static void view_font(lv_obj_t *label, const lv_font_t *font) {
    if (lv_obj_get_style_text_font(label, 0) != font) {
        lv_obj_set_style_text_font(label, font, 0);
    }
}


static void view_show(lv_obj_t *obj, bool shown) {
    lv_obj_set_hidden(obj, !shown);
}


// A box in reverse (lit, its text dark), or plain. Set only when it changes: LVGL redraws an object whenever a style
// of it is set, even to the value it had, and the view is drawn at every refresh (ADR-010, gate 4)
static void view_reverse(lv_obj_t *box, bool reverse, lv_obj_t *a, lv_obj_t *b, lv_obj_t *c) {
    lv_opa_t opa = reverse ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (lv_obj_get_style_bg_opa(box, 0) != opa) {
        lv_obj_set_style_bg_opa(box, opa, 0);
    }
    lv_color_t text = reverse ? lv_color_black() : lv_color_white();
    lv_obj_t *labels[] = {a, b, c};
    for (size_t idx = 0; idx < 3; idx += 1) {
        if (labels[idx] != NULL && !lv_color_eq(lv_obj_get_style_text_color(labels[idx], 0), text)) {
            lv_obj_set_style_text_color(labels[idx], text, 0);
        }
    }
}


// A value being set in place: in reverse in its plain row, a pixel of lit room at each side
static void view_value_reverse(lv_obj_t *value, bool reverse) {
    lv_opa_t opa = reverse ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (lv_obj_get_style_bg_opa(value, 0) != opa) {
        lv_obj_set_style_bg_opa(value, opa, 0);
        lv_obj_set_style_bg_color(value, lv_color_white(), 0);
        lv_obj_set_style_pad_right(value, reverse ? 2 : 0, 0);
    }
    if (reverse && !lv_color_eq(lv_obj_get_style_text_color(value, 0), lv_color_black())) {
        lv_obj_set_style_text_color(value, lv_color_black(), 0);
    }
}


// --- The objects

void menu_view_init(lv_display_t *display) {
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(screen, false);

    view_title_box = view_box(screen, 0, 0, VIEW_W, VIEW_TITLE_H, false);
    view_title = view_label(view_title_box, &menu_font_bold, LV_TEXT_ALIGN_LEFT);
    view_marks = view_label(view_title_box, &menu_font_regular, LV_TEXT_ALIGN_RIGHT);
    view_place(view_marks, 64, VIEW_W - 64 - 1, VIEW_TITLE_BASE);
    view_box(screen, 0, VIEW_RULE_Y, VIEW_W, 1, true);

    for (int idx = 0; idx < VIEW_ROWS; idx += 1) {
        view_rows[idx] = view_box(screen, 0, VIEW_CONTENT_TOP + idx * VIEW_ROW_H, VIEW_W, VIEW_ROW_H - 1, false);
        view_row_label[idx] = view_label(view_rows[idx], &menu_font_regular, LV_TEXT_ALIGN_LEFT);
        view_row_value[idx] = view_label(view_rows[idx], &menu_font_regular, LV_TEXT_ALIGN_RIGHT);
        view_row_more[idx] = view_label(view_rows[idx], &menu_font_regular, LV_TEXT_ALIGN_RIGHT);
        lv_label_set_text(view_row_more[idx], ">");
    }
    view_button = view_box(screen, 2, 0, VIEW_W - 8, VIEW_LINE_H, true);
    view_button_label = view_label(view_button, &menu_font_regular, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_color(view_button_label, lv_color_black(), 0);
    view_track = view_box(screen, VIEW_W - 1, VIEW_CONTENT_TOP, 1, VIEW_H - VIEW_CONTENT_TOP, true);
    view_thumb = view_box(screen, VIEW_W - 3, VIEW_CONTENT_TOP, 3, 8, true);

    for (int idx = 0; idx < 2; idx += 1) {
        view_message[idx] = view_label(screen, &menu_font_regular, LV_TEXT_ALIGN_CENTER);
        view_place(view_message[idx], 0, VIEW_W, idx == 0 ? VIEW_MESSAGE_BASE_1 : VIEW_MESSAGE_BASE_2);
    }
    view_figure = view_label(screen, &menu_font_figure, LV_TEXT_ALIGN_LEFT);
    view_unit = view_label(screen, &menu_font_regular, LV_TEXT_ALIGN_LEFT);
    view_cursor = view_box(screen, 0, VIEW_FIGURE_BASE + 3, 8, 2, true);
    view_edit_box = view_box(screen, 0, 0, 8, 8, true);
    view_edit_digit = view_label(view_edit_box, &menu_font_figure, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_color(view_edit_digit, lv_color_black(), 0);
    view_context = view_label(screen, &menu_font_regular, LV_TEXT_ALIGN_CENTER);
    view_place(view_context, 0, VIEW_W, VIEW_CONTEXT_BASE);
    view_chart = view_box(screen, VIEW_CHART_X, VIEW_CHART_TOP, VIEW_CHART_W, VIEW_CHART_H, false);
    view_chart_head = view_label(screen, &menu_font_bold, LV_TEXT_ALIGN_LEFT);
    view_place(view_chart_head, VIEW_MARGIN, 80, VIEW_CHART_HEAD_BASE);
    view_chart_note = view_label(screen, &menu_font_regular, LV_TEXT_ALIGN_RIGHT);
    view_place(view_chart_note, 64, VIEW_W - 64 - VIEW_MARGIN, VIEW_CHART_HEAD_BASE);
    view_show(view_chart, false);
    view_show(view_chart_head, false);
    view_show(view_chart_note, false);

    view_softkey_left_box = view_box(screen, 0, VIEW_SOFTKEY_TOP, 64, VIEW_H - VIEW_SOFTKEY_TOP, false);
    view_softkey_left = view_label(view_softkey_left_box, &menu_font_regular, LV_TEXT_ALIGN_CENTER);
    view_softkey_box = view_box(screen, 64, VIEW_SOFTKEY_TOP, 64, VIEW_H - VIEW_SOFTKEY_TOP, false);
    view_softkey_right = view_label(view_softkey_box, &menu_font_regular, LV_TEXT_ALIGN_CENTER);

    // The hello's overlay last: the screen's last child, over everything
    view_visit_create(screen);

    menu_nav_set_text_width(view_measure, VIEW_LINE_WIDTH, VIEW_LINES);
}


// --- The parts of a screen

static void view_title_and_marks(const menu_screen_t *screen, bool focused) {
    // Wi-Fi as an icon of the regular font: full signal bars joined as a station, with no strength (the owner:
    // clearest on the small screen), a hotspot as its own access point. The remote mark a phone; the simulated
    // scale's the word SIM
    char marks[40];
    snprintf(marks, sizeof(marks), "%s%s%s%s%s", screen->title_note, screen->title_note[0] != '\0' ? "  " : "",
             screen->mark_simulated ? "SIM " : "", screen->mark_remote ? VIEW_ICON_REMOTE " " : "",
             screen->mark_station ? VIEW_ICON_STATION : (screen->mark_access_point ? VIEW_ICON_ACCESS_POINT : ""));
    view_text(view_marks, marks);
    char title[MENU_TEXT + 4];
    snprintf(title, sizeof(title), "%s%s", screen->title_back ? "< " : "", screen->title);
    view_text(view_title, title);
    // The marks as wide as they are, the time before them; the title clipped short of them
    int marks_w = view_text_width(marks, strlen(marks), &menu_font_regular);
    view_place(view_marks, VIEW_W - 1 - marks_w - 2, marks_w + 2, VIEW_TITLE_BASE);
    view_place(view_title, 1, VIEW_W - 1 - marks_w - (marks_w > 0 ? 4 : 0), VIEW_TITLE_BASE);
    view_reverse(view_title_box, focused, view_title, view_marks, NULL);
}


// A scroll bar at the right edge when there is more than is shown: a track, and a thumb as long as the share shown
static void view_scrollbar(int first, int shown, int total) {
    bool scrolls = total > shown;
    view_show(view_track, scrolls);
    view_show(view_thumb, scrolls);
    if (!scrolls) {
        return;
    }
    int span = VIEW_H - VIEW_CONTENT_TOP;
    int length = span * shown / total;
    length = length < 4 ? 4 : length;
    int at = VIEW_CONTENT_TOP + (span - length) * first / (total - shown);
    lv_obj_set_pos(view_thumb, VIEW_W - 3, at);
    lv_obj_set_height(view_thumb, length);
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
    bool scrolls = screen->row_count > VIEW_ROWS;
    int edge = scrolls ? VIEW_W - 5 : VIEW_W - VIEW_MARGIN;
    for (int idx = 0; idx < VIEW_ROWS; idx += 1) {
        int row = view_first + idx;
        bool shown = row < screen->row_count;
        view_show(view_rows[idx], shown);
        if (!shown) {
            continue;
        }
        lv_obj_set_pos(view_rows[idx], 0, VIEW_CONTENT_TOP + idx * VIEW_ROW_H);
        lv_obj_set_size(view_rows[idx], edge + 2, VIEW_ROW_H - 1);
        view_font(view_row_label[idx], &menu_font_regular);
        view_text(view_row_label[idx], screen->row_label[row]);
        // A choice's current option: a check where a value stands
        view_text(view_row_value[idx], screen->row_chosen[row] ? VIEW_ICON_CHOSEN : screen->row_value[row]);
        view_show(view_row_value[idx], true);      // a card hides it
        // A card uses these two for its columns, in bold on a heading: a list's are regular, and ">"
        view_font(view_row_value[idx], &menu_font_regular);
        view_font(view_row_more[idx], &menu_font_regular);
        view_text(view_row_more[idx], ">");
        // The text centred in the row's 12 px: its baseline 9 below the row's top
        int more_w = screen->row_opens[row] ? view_text_width(">", 1, &menu_font_regular) + 3 : 0;
        view_show(view_row_more[idx], screen->row_opens[row]);
        view_place(view_row_more[idx], edge - 10, 10, 9);
        int value_w = view_text_width(screen->row_value[row], strlen(screen->row_value[row]), &menu_font_regular);
        bool setting = row == screen->focus && screen->editing;
        // A value being set is as wide as its text and 2 px each side, so its reverse hugs it
        if (setting) {
            view_place(view_row_value[idx], edge - more_w - value_w - 3, value_w + 4, 9);
        }
        else {
            view_place(view_row_value[idx], 40, edge - 40 - more_w, 9);
        }
        view_place(view_row_label[idx], VIEW_MARGIN, edge - VIEW_MARGIN - more_w - (value_w > 0 ? value_w + 3 : 0),
                   9);
        view_reverse(view_rows[idx], row == screen->focus && !setting, view_row_label[idx],
                     setting ? NULL : view_row_value[idx], view_row_more[idx]);
        view_value_reverse(view_row_value[idx], setting);
    }
    view_scrollbar(view_first, VIEW_ROWS, screen->row_count);
}


// A card or notes: four lines of 11 px from the first shown; notes end in their button, a line of its own
static void view_lines(const menu_screen_t *screen) {
    bool notes = screen->kind == MENU_SCREEN_NOTES;
    int total = screen->row_count + (notes && screen->button[0] != '\0' ? 1 : 0);
    bool button_shown = false;
    for (int idx = 0; idx < VIEW_LINES; idx += 1) {
        int line = screen->first + idx;
        int top = VIEW_CONTENT_TOP + idx * VIEW_LINE_H;
        bool text = line < screen->row_count;
        view_show(view_rows[idx], text);
        if (line == screen->row_count && notes && screen->button[0] != '\0') {
            lv_obj_set_pos(view_button, 2, top);
            view_text(view_button_label, screen->button);
            view_place(view_button_label, 0, VIEW_W - 8, VIEW_LINE_H - 3);
            button_shown = true;
        }
        if (!text) {
            continue;
        }
        lv_obj_set_pos(view_rows[idx], 0, top);
        lv_obj_set_size(view_rows[idx], VIEW_W - 5, VIEW_LINE_H);
        view_reverse(view_rows[idx], false, view_row_label[idx], view_row_value[idx], view_row_more[idx]);
        view_font(view_row_label[idx], screen->row_bold[line] ? &menu_font_bold : &menu_font_regular);
        view_text(view_row_label[idx], screen->row_label[line]);
        // A key at the left, its value at the right; the key clipped short of the value
        const char *value = screen->row_value[line];
        view_font(view_row_value[idx], &menu_font_regular);
        view_show(view_row_value[idx], value[0] != '\0');
        view_show(view_row_more[idx], false);
        int value_w = value[0] != '\0' ? view_text_width(value, strlen(value), &menu_font_regular) + 6 : 0;
        if (value[0] != '\0') {
            view_text(view_row_value[idx], value);
            view_place(view_row_value[idx], VIEW_MARGIN, VIEW_LINE_WIDTH, VIEW_LINE_H - 3);
        }
        view_place(view_row_label[idx], VIEW_MARGIN, VIEW_LINE_WIDTH - value_w, VIEW_LINE_H - 3);
    }
    view_show(view_button, button_shown);
    view_scrollbar(screen->first, VIEW_LINES, total);
}


static void view_dialog(const menu_screen_t *screen) {
    for (int idx = 0; idx < 2; idx += 1) {
        view_text(view_message[idx], idx < screen->row_count ? screen->row_label[idx] : "");
    }
}


// The figure and its unit, centred together; the editor's focused digit underlined, in reverse while it is set
static void view_figure_and_unit(const menu_screen_t *screen) {
    int figure_w = view_text_width(screen->figure, strlen(screen->figure), &menu_font_figure);
    int unit_w = screen->unit[0] != '\0' ? view_text_width(screen->unit, strlen(screen->unit), &menu_font_regular) + 2
                                         : 0;
    int x = (VIEW_W - figure_w - unit_w) / 2;
    view_text(view_figure, screen->figure);
    view_place(view_figure, x, figure_w + 2, VIEW_FIGURE_BASE);
    view_text(view_unit, screen->unit);
    view_place(view_unit, x + figure_w + 2, unit_w + 2, VIEW_FIGURE_BASE);
    bool focused = screen->kind == MENU_SCREEN_EDIT && screen->edit_at >= 0;
    bool setting = focused && screen->editing;
    view_show(view_cursor, focused && !setting);
    view_show(view_edit_box, setting);
    if (focused) {
        int at = view_text_width(screen->figure, (size_t) screen->edit_at, &menu_font_figure);
        int digit = (int) lv_font_get_glyph_width(&menu_font_figure, (uint32_t) screen->figure[screen->edit_at], 0);
        lv_obj_set_pos(view_cursor, x + at, VIEW_FIGURE_BASE + 3);
        lv_obj_set_width(view_cursor, digit - 1);
        if (setting) {
            // The digit again, dark on a lit box a pixel wider each side, from the figure's top to under its base
            char one[2] = {screen->figure[screen->edit_at], 0};
            int top = view_top(&menu_font_figure, VIEW_FIGURE_BASE);
            lv_obj_set_pos(view_edit_box, x + at - 1, top);
            lv_obj_set_size(view_edit_box, digit + 1, VIEW_FIGURE_BASE + 3 - top);
            view_text(view_edit_digit, one);
            view_place(view_edit_digit, 0, digit + 1, VIEW_FIGURE_BASE - top);
        }
    }
    view_font(view_context, screen->context_bold ? &menu_font_bold : &menu_font_regular);
    view_text(view_context, screen->context);
}


// One softkey: its box as wide as its text and 3 px each side, at the left or the right edge, in reverse if focused
static void view_softkey(lv_obj_t *box, lv_obj_t *label, const char *text, bool at_left, bool focused) {
    bool shown = text[0] != '\0';
    view_show(box, shown);
    if (!shown) {
        return;
    }
    int width = view_text_width(text, strlen(text), &menu_font_regular) + 6;
    lv_obj_set_pos(box, at_left ? 0 : VIEW_W - width, VIEW_SOFTKEY_TOP);
    lv_obj_set_width(box, width);
    view_text(label, text);
    view_place(label, 0, width, VIEW_SOFTKEY_BASE - VIEW_SOFTKEY_TOP);
    view_reverse(box, focused, label, NULL, NULL);
}


static void view_softkeys(const menu_screen_t *screen) {
    view_softkey(view_softkey_left_box, view_softkey_left, screen->softkey_left, true, screen->softkey_focus == 0);
    view_softkey(view_softkey_box, view_softkey_right, screen->softkey_right, false, screen->softkey_focus == 1);
}


void menu_view_draw(const menu_screen_t *screen) {
    if (screen->version != view_version) {
        view_version = screen->version;
        view_first = 0;
    }
    bool list = screen->kind == MENU_SCREEN_LIST;
    bool lines = screen->kind == MENU_SCREEN_NOTES;
    bool dialog = screen->kind == MENU_SCREEN_DIALOG;
    bool figure = screen->kind == MENU_SCREEN_EDIT || screen->kind == MENU_SCREEN_JOB;
    bool control = screen->kind == MENU_SCREEN_CONTROL;

    view_title_and_marks(screen, (list || lines || control) && screen->focus == -1);
    if (!list && !lines) {
        for (int idx = 0; idx < VIEW_ROWS; idx += 1) {
            view_show(view_rows[idx], false);
        }
        view_show(view_track, false);
        view_show(view_thumb, false);
    }
    if (!lines) {
        view_show(view_button, false);
    }
    view_show(view_message[0], dialog);
    view_show(view_message[1], dialog);
    view_show(view_figure, figure);
    view_show(view_unit, figure);
    view_show(view_context, figure);
    if (!figure) {
        view_show(view_cursor, false);
        view_show(view_edit_box, false);
    }
    if (!dialog && !figure && !control) {
        view_show(view_softkey_left_box, false);
        view_show(view_softkey_box, false);
    }

    if (list) {
        view_list(screen);
    }
    else if (lines) {
        view_lines(screen);
    }
    else if (dialog) {
        view_dialog(screen);
    }
    else if (control) {
        view_control(screen);
    }
    else {
        view_figure_and_unit(screen);
    }
    view_chart_draw(screen, figure && screen->chart);
    if (dialog || figure || control) {
        view_softkeys(screen);
    }
}


/*
 * A job of rows (the cleanup): its row 0 a line to read (the weight left, the flow right) under the title, then its
 * rows of 13 px, the focused one in reverse, a value being set in reverse in its plain row; the softkeys under them
 */
static void view_control(const menu_screen_t *screen) {
    int edge = VIEW_W - VIEW_MARGIN;
    for (int idx = 0; idx < VIEW_ROWS; idx += 1) {
        bool shown = idx < screen->row_count;
        view_show(view_rows[idx], shown);
        if (!shown) {
            continue;
        }
        int top = idx == 0 ? VIEW_CONTENT_TOP : VIEW_CONTENT_TOP + VIEW_LINE_H + (idx - 1) * VIEW_ROW_H;
        int base = idx == 0 ? VIEW_LINE_H - 3 : 9;
        lv_obj_set_pos(view_rows[idx], 0, top);
        lv_obj_set_size(view_rows[idx], VIEW_W, idx == 0 ? VIEW_LINE_H : VIEW_ROW_H - 1);
        view_font(view_row_label[idx], &menu_font_regular);
        view_font(view_row_value[idx], &menu_font_regular);
        view_text(view_row_label[idx], screen->row_label[idx]);
        view_text(view_row_value[idx], screen->row_value[idx]);
        view_show(view_row_value[idx], true);
        view_show(view_row_more[idx], false);
        bool setting = idx == screen->focus && screen->editing;
        int value_w = view_text_width(screen->row_value[idx], strlen(screen->row_value[idx]), &menu_font_regular);
        if (setting) {
            view_place(view_row_value[idx], edge - value_w - 3, value_w + 4, base);
        }
        else {
            view_place(view_row_value[idx], 40, edge - 40, base);
        }
        view_place(view_row_label[idx], VIEW_MARGIN, edge - VIEW_MARGIN - (value_w > 0 ? value_w + 3 : 0), base);
        view_reverse(view_rows[idx], idx == screen->focus && !setting, view_row_label[idx],
                     setting ? NULL : view_row_value[idx], NULL);
        view_value_reverse(view_row_value[idx], setting);
    }
    view_show(view_track, false);
    view_show(view_thumb, false);
}


// Set one lit pixel in the frame's rows (1 lit, the leftmost pixel the top bit), within an area
static void view_paint_pixel(uint8_t *rows, const lv_area_t *area, int x, int y) {
    if (x < area->x1 || x > area->x2 || y < area->y1 || y > area->y2 || x < 0 || x >= VIEW_W || y < 0 ||
        y >= VIEW_H) {
        return;
    }
    rows[y * (VIEW_W / 8) + x / 8] |= (uint8_t) (0x80u >> (x % 8));
}


void menu_view_paint(uint8_t *rows, const lv_area_t *area) {
    view_bubble_paint(rows, area);
    if (!view_chart_shown) {
        return;
    }
    int bottom = VIEW_CHART_TOP + VIEW_CHART_H - 1;
    int span = VIEW_CHART_H - 2;
    // The target, dotted; the base, a line
    int target_y = bottom - span * MENU_CHART_TARGET / MENU_CHART_FULL;
    for (int x = VIEW_CHART_X; x < VIEW_CHART_X + VIEW_CHART_W; x += 1) {
        view_paint_pixel(rows, area, x, bottom);
        if (x % 3 == 0) {
            view_paint_pixel(rows, area, x, target_y);
        }
    }
    // The time across: 10 s, or the throw's time so far when longer, so the curve then reaches the right edge and
    // what came before is pressed into the width (the owner, 2026-11-18)
    uint32_t last_ms = view_chart_count > 0 ? (uint32_t) (view_chart_count - 1) * view_chart_step_ms : 0;
    uint32_t across_ms = last_ms > MENU_CHART_ACROSS_MS ? last_ms : MENU_CHART_ACROSS_MS;
    int width = VIEW_CHART_W - 3;
    // The curve: each point at its time, joined to the one before along the line, a column at each x
    int prev_x = VIEW_CHART_X + 1;
    int prev_y = bottom;
    for (int idx = 0; idx < view_chart_count; idx += 1) {
        int x = VIEW_CHART_X + 1 + (int) ((uint64_t) idx * view_chart_step_ms * (uint64_t) width / across_ms);
        int y = bottom - span * view_chart_points[idx] / MENU_CHART_FULL;
        int run = x - prev_x;
        int was = prev_y;
        for (int cx = run > 0 ? prev_x + 1 : x; cx <= x; cx += 1) {
            int cy = run > 0 ? prev_y + (y - prev_y) * (cx - prev_x) / run : y;
            int from = was < cy ? was : cy;
            int to = was < cy ? cy : was;
            for (int yy = from; yy <= to; yy += 1) {
                view_paint_pixel(rows, area, cx, yy);
            }
            was = cy;
        }
        prev_x = x;
        prev_y = y;
    }
}


// The chart's head and its area: the area made to be drawn again when its points change, so it is painted again
static void view_chart_draw(const menu_screen_t *screen, bool shown) {
    view_show(view_chart, shown);
    view_show(view_chart_head, shown);
    view_show(view_chart_note, shown);
    if (shown != view_chart_shown) {
        view_chart_shown = shown;
        lv_obj_invalidate(view_chart);
    }
    if (!shown) {
        return;
    }
    view_show(view_figure, false);
    view_show(view_unit, false);
    view_show(view_context, false);
    view_text(view_chart_head, screen->chart_head);
    view_font(view_chart_note, screen->context_bold ? &menu_font_bold : &menu_font_regular);
    view_text(view_chart_note, screen->context);
    if (screen->chart_count != view_chart_count || screen->chart_step_ms != view_chart_step_ms ||
        memcmp(screen->chart_points, view_chart_points, screen->chart_count) != 0) {
        view_chart_count = screen->chart_count;
        view_chart_step_ms = screen->chart_step_ms;
        memcpy(view_chart_points, screen->chart_points, screen->chart_count);
        lv_obj_invalidate(view_chart);
    }
}


// --- The hidden hello (the owner, 2026-11-17): an agent says who it is over the API, and Claude-chan peeks up from
// the bottom of the screen, looks about, blinks and goes. Slow steps for a passive LCD (the skill embedded-ui-design).
// It is the screen's last child, opaque and as large as the screen: LVGL then draws from it up and skips the page
// under it. On LVGL's top layer the page was drawn as well, and with eight objects of its own the two ran LVGL's heap
// out: a draw task's allocation failed and the core faulted (2026-11-17). Its four objects are made once, at the
// start, and hidden: made at each hello, they needed 2,048 bytes of LVGL's heap in one piece, which the menu's growth
// took away unnoticed, so a hello was let go and only flashed while the API said it was shown (the owner, 2026-11-17)

// The layout (the owner, 2026-11-18, from mockups/claude_chan_new.png): she rises at the bottom right; a round bubble
// at the top left says who she is, drawn over her, its tail pointing at her face
#define VIEW_VISIT_FIGURE_SIZE 48
#define VIEW_VISIT_FIGURE_X (VIEW_W - VIEW_VISIT_FIGURE_SIZE - 2)
#define VIEW_VISIT_WORDS "is watching ya!"
// The bubble: a superellipse of power 2.5 (2 an ellipse, higher squarer; 2.5 round, and two lines still fit), as wide
// as its words
#define VIEW_BUBBLE_CY 17
#define VIEW_BUBBLE_B 16
#define VIEW_BUBBLE_RIGHT_MAX (VIEW_W - 3)
#define VIEW_BUBBLE_TIP_X_MIN 86     // the tail's tip: on her, left of her face
#define VIEW_BUBBLE_TIP_Y 42
// The figure's poses, in fonts/claude_chan.txt from the top, each nine 16 x 16 tiles of menu_font_chan from 'A' on
typedef enum {
    VIEW_VISIT_AHEAD = 0,
    VIEW_VISIT_LEFT,
    VIEW_VISIT_RIGHT,
    VIEW_VISIT_BLINK,
} view_visit_pose_t;

typedef struct {
    int8_t top;         // the figure's top: 16 stands it on the bottom edge
    uint8_t pose;
    bool words;
} view_visit_step_t;

static const view_visit_step_t view_visit_steps[MENU_VIEW_VISIT_STEPS] = {
    {50, VIEW_VISIT_AHEAD, false}, {40, VIEW_VISIT_AHEAD, false}, {16, VIEW_VISIT_AHEAD, true},
    {16, VIEW_VISIT_LEFT, true}, {16, VIEW_VISIT_RIGHT, true}, {16, VIEW_VISIT_AHEAD, true},
    {16, VIEW_VISIT_BLINK, true}, {16, VIEW_VISIT_AHEAD, true}, {16, VIEW_VISIT_AHEAD, true},
    {40, VIEW_VISIT_AHEAD, true},
};

// Four objects: the dark ground, the figure, the name and the words. The figure is one label of nine tiles, a pose
// a different nine, so the eyes need no objects of their own. The bubble is no object: LVGL here draws no curve
// (LV_DRAW_SW_COMPLEX is off), so it is painted into the frame at the flush, as the chart is (menu_view_paint). The
// name and the words have dark boxes of their own, which cover her where the bubble overlaps her; the paint clears
// the rest of the bubble's inside around them, and draws its edge
static lv_obj_t *view_visit = NULL;
static lv_obj_t *view_visit_figure;
static lv_obj_t *view_visit_name;
static lv_obj_t *view_visit_words;

// The bubble's shape, made once a hello: each row's span of the oval and of the tail (x0 > x1 for none)
typedef struct {
    int8_t x0;
    int8_t x1;
    int8_t tail_x0;
    int8_t tail_x1;
} view_bubble_row_t;

static view_bubble_row_t view_bubble_rows[VIEW_H];
static bool view_bubble_shown = false;
static lv_area_t view_bubble_keep[2];       // the name's and the words' boxes: the paint leaves them as LVGL drew them


// The figure's text for a pose: three rows of three tiles
static void view_visit_pose(char *text, view_visit_pose_t pose) {
    char tile = (char) ('A' + 9 * (int) pose);
    size_t at = 0;
    for (int row = 0; row < 3; row += 1) {
        for (int col = 0; col < 3; col += 1) {
            text[at++] = tile++;
        }
        text[at++] = row < 2 ? '\n' : '\0';
    }
}


// Made at the start, after every other object, so it is the screen's last child; hidden until a hello
static void view_visit_create(lv_obj_t *screen) {
    view_visit = view_box(screen, 0, 0, VIEW_W, VIEW_H, false);
    lv_obj_set_style_bg_color(view_visit, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(view_visit, LV_OPA_COVER, 0);

    view_visit_figure = view_label(view_visit, &menu_font_chan, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_size(view_visit_figure, VIEW_VISIT_FIGURE_SIZE, VIEW_VISIT_FIGURE_SIZE);

    // The words in the bubble, each on a dark box of its own: over her, where the bubble overlaps her
    view_visit_name = view_label(view_visit, &menu_font_bold, LV_TEXT_ALIGN_CENTER);
    view_visit_words = view_label(view_visit, &menu_font_regular, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *boxed[] = {view_visit_name, view_visit_words};
    for (size_t idx = 0; idx < 2; idx += 1) {
        lv_obj_set_style_bg_color(boxed[idx], lv_color_black(), 0);
        lv_obj_set_style_bg_opa(boxed[idx], LV_OPA_COVER, 0);
    }
    view_text(view_visit_words, VIEW_VISIT_WORDS);
    view_show(view_visit, false);
}


// t to the power 2.5, the bubble's (t from 0 to 1): with sqrtf, not powf, which took 1.5 KB more of flash
static float view_pow25(float t) {
    return t * t * sqrtf(t);
}


// The r from 0 to 1 whose power 2.5 is v, by halving: the inverse of view_pow25
static float view_root25(float v) {
    float lo = 0.0f;
    float hi = 1.0f;
    for (int step = 0; step < 20; step += 1) {
        float mid = (lo + hi) / 2.0f;
        if (view_pow25(mid) < v) {
            lo = mid;
        }
        else {
            hi = mid;
        }
    }
    return lo;
}


// The bubble for a name: its shape row by row, and the words placed in it. Once a hello, so the flush's paint needs
// no arithmetic beyond comparisons
static void view_bubble_make(const char *name) {
    int name_w = view_text_width(name, strlen(name), &menu_font_bold);
    int words_w = view_text_width(VIEW_VISIT_WORDS, strlen(VIEW_VISIT_WORDS), &menu_font_regular);
    // The oval as small as holds the two lines: their half-width tw, half-height th, its corners inside
    float tw = (float) (name_w > words_w ? name_w : words_w) / 2.0f + 2.0f;
    const float th = 12.0f;
    const float b = (float) VIEW_BUBBLE_B;
    float a = tw / view_root25(1.0f - view_pow25(th / b)) + 3.0f;
    float a_max = (float) (VIEW_BUBBLE_RIGHT_MAX - 2) / 2.0f;
    a = a > a_max ? a_max : a;
    float cx = 2.0f + a;
    const float cy = (float) VIEW_BUBBLE_CY;
    // The tail: from the oval's lower right to a tip on her, wherever the oval ends
    const float k = 0.3f;
    float tx = cx + k * a;
    float ty = cy + b * view_root25(1.0f - view_pow25(k)) - 3.0f;
    float tip_x = tx + 8.0f > (float) VIEW_BUBBLE_TIP_X_MIN ? tx + 8.0f : (float) VIEW_BUBBLE_TIP_X_MIN;
    float tip_y = (float) VIEW_BUBBLE_TIP_Y;
    for (int y = 0; y < VIEW_H; y += 1) {
        view_bubble_row_t *row = &view_bubble_rows[y];
        row->x0 = 1;
        row->x1 = 0;
        row->tail_x0 = 1;
        row->tail_x1 = 0;
        float dy = fabsf((float) y - cy) / b;
        if (dy <= 1.0f) {
            float half = a * view_root25(1.0f - view_pow25(dy));
            row->x0 = (int8_t) ceilf(cx - half);
            row->x1 = (int8_t) floorf(cx + half);
        }
        if ((float) y >= ty && (float) y <= tip_y) {
            float f = ((float) y - ty) / (tip_y - ty);
            row->tail_x0 = (int8_t) ceilf(tx - 4.0f + (tip_x - (tx - 4.0f)) * f);
            row->tail_x1 = (int8_t) floorf(tx + 4.0f + (tip_x - (tx + 4.0f)) * f);
        }
    }
    // The words centred in the oval, the name clipped to it; their baselines 15 and 27
    int width_max = (int) (2.0f * a) - 8;
    int name_box = name_w < width_max ? name_w : width_max;
    lv_obj_t *labels[] = {view_visit_name, view_visit_words};
    const int widths[] = {name_box, words_w};
    const int bases[] = {VIEW_BUBBLE_CY - 2, VIEW_BUBBLE_CY + 10};
    for (size_t idx = 0; idx < 2; idx += 1) {
        int x = (int) lroundf(cx) - widths[idx] / 2;
        view_place(labels[idx], x, widths[idx], bases[idx]);
        lv_obj_update_layout(labels[idx]);
        lv_obj_get_coords(labels[idx], &view_bubble_keep[idx]);
    }
}


static bool view_bubble_has(int x, int y) {
    if (y < 0 || y >= VIEW_H) {
        return false;
    }
    const view_bubble_row_t *row = &view_bubble_rows[y];
    return (x >= row->x0 && x <= row->x1) || (x >= row->tail_x0 && x <= row->tail_x1);
}


static void view_paint_clear(uint8_t *rows, const lv_area_t *area, int x, int y) {
    if (x < area->x1 || x > area->x2 || y < area->y1 || y > area->y2 || x < 0 || x >= VIEW_W || y < 0 ||
        y >= VIEW_H) {
        return;
    }
    rows[y * (VIEW_W / 8) + x / 8] &= (uint8_t) ~(0x80u >> (x % 8));
}


// The bubble into the frame, within the area flushed: its edge lit, its inside cleared but for the words' boxes,
// and the two strokes of surprise at its corner
static void view_bubble_paint(uint8_t *rows, const lv_area_t *area) {
    if (!view_bubble_shown) {
        return;
    }
    for (int y = area->y1; y <= area->y2 && y < VIEW_H; y += 1) {
        for (int x = area->x1; x <= area->x2 && x < VIEW_W; x += 1) {
            if (!view_bubble_has(x, y)) {
                continue;
            }
            bool edge = !view_bubble_has(x + 1, y) || !view_bubble_has(x - 1, y) || !view_bubble_has(x, y + 1) ||
                        !view_bubble_has(x, y - 1);
            if (edge) {
                view_paint_pixel(rows, area, x, y);
                continue;
            }
            bool kept = false;
            for (size_t idx = 0; idx < 2 && !kept; idx += 1) {
                const lv_area_t *box = &view_bubble_keep[idx];
                kept = x >= box->x1 && x <= box->x2 && y >= box->y1 && y <= box->y2;
            }
            if (!kept) {
                view_paint_clear(rows, area, x, y);
            }
        }
    }
    const int strokes[][2] = {{1, 1}, {2, 2}, {5, 0}, {5, 1}};
    for (size_t idx = 0; idx < sizeof(strokes) / sizeof(strokes[0]); idx += 1) {
        view_paint_pixel(rows, area, strokes[idx][0], strokes[idx][1]);
    }
}


bool menu_view_visit(int step, const char *name) {
    if (step < 0 || step >= MENU_VIEW_VISIT_STEPS) {
        return false;
    }
    if (step == 0 || strcmp(lv_label_get_text(view_visit_name), name) != 0) {
        view_text(view_visit_name, name);
        view_bubble_make(name);
    }
    view_show(view_visit, true);
    const view_visit_step_t *at = &view_visit_steps[step];
    char pose[12];
    view_visit_pose(pose, (view_visit_pose_t) at->pose);
    view_text(view_visit_figure, pose);
    lv_obj_set_pos(view_visit_figure, VIEW_VISIT_FIGURE_X, at->top);
    view_show(view_visit_name, at->words);
    view_show(view_visit_words, at->words);
    // The bubble is painted, not drawn: the whole screen is flushed again when it comes or goes
    if (at->words != view_bubble_shown) {
        view_bubble_shown = at->words;
        lv_obj_invalidate(view_visit);
    }
    return true;
}


void menu_view_visit_end(void) {
    view_show(view_visit, false);
    view_bubble_shown = false;
}
