#!/usr/bin/env python3
"""Writes the images of the user manual (manuals/powder_analysis*.md) from the preview output.

    doc_images.py <preview out dir> <target dir> [<path to Google Chrome>]

- Display screens: the 4x LCD PNG of a preview screen with coloured markings on top (rectangles
  around buttons and areas, each with a numbered badge the manual refers to). The markings are
  defined in DISPLAY_IMAGES below, in LCD pixel coordinates; footer buttons are located
  automatically from the rendered frame.
- Web pages: screenshots of the banner-free scenario pages (out/web/doc/) taken with headless
  Chrome at phone width and one desktop width, see WEB_IMAGES. Skipped when Chrome is not there.

Standard library only; run through `make -C tools/powder_analysis_preview doc-images`.
"""
import json
import os
import subprocess
import sys

from lcd_png import SCALE, read_pbm, lcd_rows, write_png

RED = (220, 40, 40)         # controls: footer buttons
BLUE = (25, 95, 220)        # information: a row or an area the manual explains


def area(x, y, w, h, number):
    """Blue rectangle in LCD pixels with a numbered badge"""
    return ("rect", x, y, w, h, BLUE, number)


def row(index, number):
    """Blue rectangle around text row 0..2 (FIRST_ROW_Y 25, ROW_HEIGHT 10 in the module)"""
    return area(1, 16 + index * 10, 126, 12, number)


def rows(first, last, number):
    return area(1, 16 + first * 10, 126, 12 + (last - first) * 10, number)


def button(side, number):
    """Red rectangle around the left or right footer button, found in the rendered frame"""
    return ("button", side, RED, number)


# (preview screen name, manual image name, markings)
DISPLAY_IMAGES = [
    ("d1_target_weight", "display_d1_target_weight", [area(50, 19, 16, 21, 1), button("left", 2), button("right", 3)]),
    ("d2_shape", "display_d2_powder_shape", []),
    ("d4_prepare", "display_d4_prepare", [row(0, 1)]),
    ("d4_prepare_exact_step", "display_d4_prepare_warning", [rows(0, 1, 1)]),
    ("d5_flooding_running", "display_d5_flooding", [button("left", 1), button("right", 2)]),
    ("d6_empty_pan", "display_d6_empty_pan", [button("left", 1), button("right", 2)]),
    ("d7_trial_coarse_kp", "display_d7_throw_running", [row(1, 1), button("right", 2)]),
    ("d8_trial_result_over", "display_d8_throw_result", [row(0, 1), button("left", 2), button("right", 3)]),
    ("d8_trial_result_kp_correction", "display_d8_correction", [row(1, 1)]),
    ("d9_paused_zero", "display_d9_paused", [button("left", 1), button("right", 2)]),
    ("d10_abort_no", "display_d10_abort", [button("left", 1), button("right", 2)]),
    ("d11_result_coarse_overshoot", "display_d11_result_coarse", [row(1, 1), row(2, 2), button("right", 3)]),
    ("d12_result_fine_verified_corr", "display_d12_result_fine", [row(1, 1), row(2, 2), button("right", 3)]),
    ("d13_result_threshold", "display_d13_result_threshold", [rows(0, 1, 1), button("left", 2), button("right", 3)]),
    ("d13_result_threshold_no_data", "display_d13_result_threshold_no_data", []),
    ("d14_save_slot", "display_d14_save_slot", [row(2, 1)]),
    ("d15_save_confirm", "display_d15_save_confirm", [button("left", 1), button("right", 2)]),
    ("d16_activate", "display_d16_use_profile", [button("left", 1), button("right", 2)]),
    ("test_throw_done", "display_test_throw_done", [row(0, 1)]),
]

# (preview scenario, manual image name, page width in px, sections to show or None for the whole page)
WEB_IMAGES = [
    ("idle", "web_setup_phone", 420, None),
    ("flooding_running", "web_flooding_phone", 420, "status"),
    ("throw_running_coarse_kp", "web_throw_running_phone", 420, "status,chart"),
    ("verify_round2_correction", "web_verify_correction_phone", 420, "status"),
    ("result_verified", "web_result_phone", 420, "result"),
    ("use_now", "web_use_now_phone", 420, "status,result"),
    ("result_verified", "web_protocol_desktop", 1000, "protocol"),
    ("test_throw_done", "web_test_throw_phone", 420, "test"),
    ("result_verified", "web_overview_desktop", 1000, None),
]

# 3x5 digits for the badges
DIGITS = {
    "0": ["111", "101", "101", "101", "111"], "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"], "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"], "7": ["111", "001", "001", "001", "001"],
    "8": ["111", "101", "111", "101", "111"], "9": ["111", "101", "111", "001", "111"],
}


# ---------------------------------------------------------------------------------------------
# Drawing on the RGB rows (image coordinates = LCD coordinates * SCALE)
# ---------------------------------------------------------------------------------------------
def put(rows_rgb, x, y, colour):
    if 0 <= y < len(rows_rgb) and 0 <= x < len(rows_rgb[0]):
        rows_rgb[y][x] = colour


def draw_frame(rows_rgb, x0, y0, x1, y1, colour, thickness=2):
    """Rectangle outline with the corners (x0, y0) .. (x1, y1) inclusive"""
    for t in range(thickness):
        for x in range(x0 - t, x1 + t + 1):
            put(rows_rgb, x, y0 - t, colour)
            put(rows_rgb, x, y1 + t, colour)
        for y in range(y0 - t, y1 + t + 1):
            put(rows_rgb, x0 - t, y, colour)
            put(rows_rgb, x1 + t, y, colour)


BADGE_RADIUS = 9


def draw_badge(rows_rgb, cx, cy, colour, number):
    """Filled circle with the number in white, centred on (cx, cy)"""
    radius = BADGE_RADIUS
    for y in range(-radius, radius + 1):
        for x in range(-radius, radius + 1):
            if x * x + y * y <= radius * radius:
                put(rows_rgb, cx + x, cy + y, colour)
    glyph = DIGITS[str(number)]
    for gy, line in enumerate(glyph):
        for gx, bit in enumerate(line):
            if bit == "1":
                for dy in range(2):
                    for dx in range(2):
                        put(rows_rgb, cx - 3 + gx * 2 + dx, cy - 5 + gy * 2 + dy, (255, 255, 255))


def footer_buttons(pixels):
    """Bounding boxes (x0, y0, x1, y1) in LCD pixels of the footer buttons, left to right. A button
    is a frame (or an inverted block) in the footer rows; its edges are columns with a long
    vertical run of set pixels there."""
    footer_top = 48
    columns = []
    for x in range(len(pixels[0])):
        run = sum(pixels[y][x] for y in range(footer_top, len(pixels)))
        if run >= 10:
            columns.append(x)

    boxes = []
    for x in columns:
        if boxes and x - boxes[-1][2] <= 45:        # same button (buttons are at most ~45 px wide)
            boxes[-1][2] = x
        else:
            boxes.append([x, footer_top, x, len(pixels) - 1])
    for box in boxes:
        ys = [y for y in range(footer_top, len(pixels)) if any(pixels[y][x] for x in range(box[0], box[2] + 1))]
        box[1], box[3] = min(ys), max(ys)
    return [tuple(box) for box in boxes]


def annotate(pixels, markings):
    rows_rgb = lcd_rows(pixels)
    buttons = footer_buttons(pixels)
    for marking in markings:
        if marking[0] == "rect":
            _, x, y, w, h, colour, number = marking
            x0, y0, x1, y1 = x * SCALE, y * SCALE, (x + w) * SCALE - 1, (y + h) * SCALE - 1
        else:
            _, side, colour, number = marking
            box = buttons[0] if side == "left" else buttons[-1]
            assert len(buttons) == 2 or (side == "left") == (box[0] < 64), "button not found"
            x0, y0, x1, y1 = box[0] * SCALE - 3, box[1] * SCALE - 3, (box[2] + 1) * SCALE + 2, (box[3] + 1) * SCALE + 2
        draw_frame(rows_rgb, x0, y0, x1, y1, colour)
        # Badge on the top right corner, moved inward where the rectangle touches the image edge
        badge_x = min(x1 - 2, len(rows_rgb[0]) - BADGE_RADIUS - 1)
        badge_y = max(y0 + 1, BADGE_RADIUS + 1)
        draw_badge(rows_rgb, badge_x, badge_y, colour, number)
    return rows_rgb


# ---------------------------------------------------------------------------------------------
# Web screenshots with headless Chrome
# ---------------------------------------------------------------------------------------------
def chrome(chrome_path, profile_dir, args):
    base = [chrome_path, "--headless=new", "--disable-gpu", "--no-first-run", "--no-default-browser-check",
            "--lang=en-US", "--hide-scrollbars", "--virtual-time-budget=3000", "--user-data-dir=" + profile_dir]
    return subprocess.run(base + args, capture_output=True, text=True, timeout=60)


def screenshot(chrome_path, profile_dir, url, width, path):
    # First pass: the page reports its settled height in the title (see DOC_SCRIPT in build_preview.py).
    # The window is kept small, a page shorter than the window would report the window height.
    dom = chrome(chrome_path, profile_dir, ["--window-size=%d,200" % width, "--dump-dom", url]).stdout
    marker = "<title>height="
    assert marker in dom, "no page height from " + url
    height = int(dom.split(marker)[1].split("<")[0])
    chrome(chrome_path, profile_dir, ["--window-size=%d,%d" % (width, height), "--screenshot=" + path, url])


# ---------------------------------------------------------------------------------------------
def main():
    out_dir, target_dir = sys.argv[1], sys.argv[2]
    chrome_path = sys.argv[3] if len(sys.argv) > 3 else None
    os.makedirs(target_dir, exist_ok=True)

    display_dir = os.path.join(out_dir, "display")
    files = {screen["name"]: screen["file"] for screen in json.load(open(os.path.join(display_dir, "screens.json")))}
    for screen_name, image_name, markings in DISPLAY_IMAGES:
        pixels = read_pbm(os.path.join(display_dir, files[screen_name].replace(".png", ".pbm")))
        write_png(os.path.join(target_dir, image_name + ".png"), annotate(pixels, markings))
    print("%d display images" % len(DISPLAY_IMAGES))

    if chrome_path is None or not os.path.exists(chrome_path):
        print("Google Chrome not found, web screenshots skipped")
        return
    profile_dir = os.path.abspath(os.path.join(out_dir, "..", "build", "chrome_profile"))
    for scenario, image_name, width, sections in WEB_IMAGES:
        url = "file://" + os.path.abspath(os.path.join(out_dir, "web", "doc", scenario + ".html")) + "?w=%d" % width
        if sections:
            url += "&show=" + sections
        screenshot(chrome_path, profile_dir, url, width, os.path.join(target_dir, image_name + ".png"))
    print("%d web screenshots" % len(WEB_IMAGES))


main()
