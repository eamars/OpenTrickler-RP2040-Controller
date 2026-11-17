"""
The screens ADR-010's survey settled (`menu_design.md`, "Needed at the unit, and can be changed there"), mocked for
the owner before they are built. Each caption gives what its change costs a person at the knob, in actions (a
detent or a click) from the main menu at rest, as `knob_cost.py` counts them.

    venv\\Scripts\\python tools\\ui_mock\\parity_12864.py [--out <folder>]

A number is edited in its row: a click, the turn, a click; the value in reverse while it is set. A choice opens its
own page, every option with the current one checked (the owner, 2026-11-17). The target alone keeps its screen of
digits, the old menu's. Drawn with menu_12864.py's faces, grid and parts.
"""
import argparse
import sys
from pathlib import Path

import menu_12864 as m
from panel import Panel, W, write_sheet

LIST_ROWS = 4
# A choice's current option (scripts/bdf2lvgl.py, ICONS, U+E002)
CHECK = ["      #", "     ##", "#   ## ", "## ##  ", " ###   ", "  #    "]


def rows(panel, items, focus, first=0, total=None, editing=False, top=None, shown=LIST_ROWS):
    """
    A list page's rows, as menu_12864.rows, with info rows (a value only to read, never focused) and one more
    state: the focused row's value being set, the row plain and its value in reverse.

    Args:
        panel: the Panel.
        items: (label, value, opens), those shown.
        focus: the focused row's index among those shown, or None.
        first: the first row's index in the whole list.
        total: the rows in the whole list.
        editing: whether the focused row's value is being set.
        top: where the rows start (the content's top by default).
        shown: how many rows fit.
    """
    top = m.CONTENT_TOP if top is None else top
    total = total or len(items)
    edge = W - 5 if total > shown else W - 3
    for idx, (label, value, opens) in enumerate(items[:shown]):
        row_top = top + idx * m.ROW_H
        base = row_top + m.ROW_BASE
        reverse = idx == focus and not editing
        if reverse:
            panel.rect(0, row_top, edge + 2, m.ROW_H - 1)
        panel.text(m.TEXT, 3, base, label, lit=not reverse)
        right = edge
        if opens:
            panel.text(m.TEXT, 0, base, ">", lit=not reverse, align="right", right=right)
            right -= m.TEXT.width(">") + 3
        if value:
            if idx == focus and editing:
                width = m.TEXT.width(value) + 6
                panel.rect(right - width + 2, row_top, width, m.ROW_H - 1)
                panel.text(m.TEXT, 0, base, value, lit=False, align="right", right=right)
            else:
                panel.text(m.TEXT, 0, base, value, lit=not reverse, align="right", right=right)
    m.scrollbar(panel, first, shown, total, top=top)


def choice_page(panel, title, options, now, focus, first=0):
    """
    A choice's own page: its options, the current one with a check at its right.

    Args:
        panel: the Panel.
        title: the setting's label.
        options: every option.
        now: the current option's index.
        focus: the focused option's index.
        first: the first option shown.
    """
    m.title(panel, title, back=True)
    shown = options[first:first + LIST_ROWS]
    edge = W - 5 if len(options) > LIST_ROWS else W - 3
    for idx, label in enumerate(shown):
        row_top = m.CONTENT_TOP + idx * m.ROW_H
        reverse = first + idx == focus
        if reverse:
            panel.rect(0, row_top, edge + 2, m.ROW_H - 1)
        panel.text(m.TEXT, 3, row_top + m.ROW_BASE, label, lit=not reverse)
        if first + idx == now:
            panel.icon(edge - 7, row_top + 3, CHECK, lit=not reverse)
    m.scrollbar(panel, first, LIST_ROWS, len(options))


def title_focused(panel, text):
    """The "<" title with the focus: in reverse, a click goes back."""
    width = m.BOLD.width("< " + text) + 3
    panel.rect(0, 0, width, m.RULE_Y - 1)
    panel.text(m.BOLD, 1, m.TITLE_BASE, "< " + text, lit=False)


def digits_editor(panel, text, focus, editing, unit=""):
    """
    The target's screen: its digits with the focused one underlined, or in reverse while it is set.

    Args:
        panel: the Panel.
        text: the figure's text, its point included.
        focus: the focused character's index, or None when a softkey has the focus.
        editing: whether that digit is being set.
        unit: the unit beside it.
    """
    x, _ = m.figure(panel, text, unit)
    if focus is None:
        return
    at = x + m.FIGURE.width(text[:focus])
    width = m.FIGURE.width(text[focus])
    if editing:
        top = m.FIGURE_BASE - m.FIGURE.cap - 1
        panel.rect(at - 1, top, width + 1, m.FIGURE_BASE + 3 - top)
        panel.text(m.FIGURE, at, m.FIGURE_BASE, text[focus], lit=False)
    else:
        panel.rect(at, m.FIGURE_BASE + 3, width - 1, 2)


def cleanup(panel, speed, weight, flow, gate, focus, editing=False, title="Cleanup"):
    """
    The cleanup's screen: its "<" title, as every page's; its weight and flow on one line to read, then Speed and Gate
    as rows, and Stop and Zero as its softkeys. The turn goes the title, Speed, Gate, Stop, Zero.

    Args:
        panel: the Panel.
        speed, weight, flow, gate: the values shown.
        focus: "title", "speed", "gate", "stop" or "zero".
        editing: whether the speed is being set.
        title: its status.
    """
    m.title(panel, title, back=True)
    if focus == "title":
        title_focused(panel, title)
    panel.text(m.TEXT, 3, m.CONTENT_TOP + m.DENSE_BASE, weight)
    panel.text(m.TEXT, 0, m.CONTENT_TOP + m.DENSE_BASE, flow, align="right", right=W - 3)
    order = {"speed": 0, "gate": 1}
    rows(panel, [("Speed", speed, False), ("Gate", gate, False)], order.get(focus), editing=editing,
         top=m.CONTENT_TOP + m.DENSE_H + 1, shown=2)
    m.softkeys(panel, "Stop", "Zero", focus={"stop": "left", "zero": "right"}.get(focus))


def chart(panel, top, bottom, points, target):
    """A throw's weight over its time, a dotted line at the target."""
    left, right = 2, W - 3
    for x in range(left, right, 3):
        panel.set(x, int(top + (bottom - top) * (1 - target)))
    prev = None
    for idx, value in enumerate(points):
        x = left + (right - left) * idx // (len(points) - 1)
        y = int(bottom - (bottom - top) * value)
        if prev:
            px, py = prev
            for step in range(abs(x - px) + 1):
                panel.set(px + step, py + (y - py) * step // max(1, abs(x - px)))
        prev = (x, y)
    panel.hline(left, bottom, right - left)


def screens():
    """
    The settled screens, in the order a person meets them, each captioned with its cost.

    Returns:
        (panels, captions).
    """
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    # --- A session: the charge
    p = Panel(); m.title(p, "Charge", back=True)
    rows(p, [("Start", "", False), ("Target", "30.00 gr", True), ("Powder", "AR2209", True)], 2)
    add(p, "Charge: Powder opens its page")

    p = Panel()
    choice_page(p, "Powder", ["AR2206H", "AR2208", "AR2209", "Varget", "H4350", "N140", "N150", "IMR4064"], 2, 4,
                first=2)
    add(p, "Powder, its page: 9 actions")

    p = Panel(); m.title(p, "Target")
    digits_editor(p, "030.00", 4, False, "gr")
    m.softkeys(p, "Cancel", "Set", focus=None)
    add(p, "Target: a digit focused")

    p = Panel(); m.title(p, "Target")
    digits_editor(p, "030.10", 4, True, "gr")
    m.softkeys(p, "Cancel", "Set", focus=None)
    add(p, "Target +0.10: 12; a new load: 29")

    # The charge: the time at the title's right, as the old screen had it (live while it pours, the last throw's
    # while it waits); the titles short enough beside it
    p = Panel(); m.title(p, "Zeroing")
    p.text(m.TEXT, 0, m.TITLE_BASE, "12.35 s", align="right", right=W - 15)
    m.figure(p, "0.00", "gr"); m.context(p, "of 30.00, AR2209")
    m.softkeys(p, "Stop", "Zero", focus="right")
    add(p, "Restored: the target, powder, last time")

    p = Panel(); m.title(p, "Charging")
    p.text(m.TEXT, 0, m.TITLE_BASE, "8.40 s", align="right", right=W - 15)
    m.figure(p, "20.08", "gr"); m.context(p, "of 30.00, AR2209")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Restored: the time, live; Stop 1")

    p = Panel(); m.title(p, "Lift the cup")
    p.text(m.TEXT, 0, m.TITLE_BASE, "12.35 s", align="right", right=W - 15)
    m.figure(p, "30.02", "gr"); m.context(p, "Over by 0.02", bold=True)
    m.softkeys(p, "Stop", "Change", focus="right")
    add(p, "Restored: the throw's time, kept")

    p = Panel(); m.title(p, "Return the cup")
    p.text(m.TEXT, 0, m.TITLE_BASE, "12.35 s", align="right", right=W - 15)
    m.figure(p, "30.02", "gr"); m.context(p, "Over by 0.02", bold=True)
    m.softkeys(p, "Stop", "Change", focus="right")
    add(p, "The old screen's \"Return Cup\"")

    p = Panel(); m.title(p, "Charging")
    p.text(m.TEXT, 0, m.TITLE_BASE, "8.40 s", align="right", right=W - 15)
    p.text(m.BOLD, 3, 23, "20.08 / 30.00")
    p.text(m.TEXT, 0, 23, "AR2209", align="right", right=W - 3)
    chart(p, 26, 51, [min(1.0, (t / 40) ** 0.6 * 0.96) for t in range(41)], 1.0)
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Charging, Charge view: Chart")

    # --- A session: the cleanup
    p = Panel(); cleanup(p, "0.0 rev/s", "0.00 gr", "0.00 gr/s", "Open", "speed")
    add(p, "Restored: the weight, flow and gate")

    p = Panel(); cleanup(p, "3.0 rev/s", "12.34 gr", "0.21 gr/s", "Open", "speed", editing=True,
                         title="Cleanup: running")
    add(p, "Speed set live: 0 to 3 rev/s, 8")

    p = Panel(); cleanup(p, "3.0 rev/s", "14.02 gr", "0.21 gr/s", "Open", "gate", title="Cleanup: running")
    add(p, "Gate, a click: 5")

    p = Panel(); cleanup(p, "3.0 rev/s", "14.02 gr", "0.21 gr/s", "Open", "stop", title="Cleanup: running")
    add(p, "Stop: the reset button 1, the knob 6")

    p = Panel(); cleanup(p, "3.0 rev/s", "14.02 gr", "0.21 gr/s", "Open", "title")
    add(p, "Back: the title, up from Speed; ends it")

    # --- Settings: what the unit needs without the web
    p = Panel(); m.title(p, "Settings", back=True)
    rows(p, [("Scale", "", True), ("Charge", "", True), ("Display", "", True), ("Servo gate", "", True)], 0,
         total=8)
    add(p, "Settings")

    p = Panel(); m.title(p, "Settings", back=True)
    rows(p, [("Motors", "", True), ("Firmware", "", True), ("Restart", "", False), ("Factory reset", "", True)],
         None, first=4, total=8)
    add(p, "Settings, its end")

    p = Panel(); m.title(p, "Scale", back=True)
    rows(p, [("Reading", "24.35 gr", False), ("Zero", "", False), ("Make", "A&D FX-i", True),
             ("Baud rate", "19200", True)], 2, total=6)
    add(p, "Scale: each choice opens its page")

    p = Panel()
    choice_page(p, "Make", ["A&D FX-i", "Jennings", "Steinberg", "Ohaus", "Kern", "Radwag", "Creedmoor",
                            "Sartorius", "G&G"], 0, 7, first=5)
    add(p, "Make, its page: 15")

    p = Panel(); m.title(p, "Scale", back=True)
    rows(p, [("Make", "A&D FX-i", True), ("Baud rate", "19200", True), ("Frame format", "8N1", True),
             ("Decimals", "2", True)], 2, first=2, total=6)
    add(p, "Frame format 11, decimals 12")

    p = Panel(); m.title(p, "Charge", back=True)
    rows(p, [("Fine stop", "0.05 gr", False), ("Coarse stop", "5.0 gr", False)], 0, editing=True)
    add(p, "Fine stop 10; coarse stop 5.0 to 3.0: 29")

    p = Panel(); m.title(p, "Not changed")
    m.message(p, "Not while", "a throw pours")
    m.softkeys(p, "", "OK")
    add(p, "A refused change")

    p = Panel(); m.title(p, "Display", back=True)
    rows(p, [("Invert knob", "Off", False), ("Invert screen", "On", False), ("Rotation", "Upside down", True),
             ("Backlight", "White", True)], 1, total=5)
    add(p, "Invert screen, a click: 9")

    p = Panel()
    choice_page(p, "Backlight", ["White", "Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Magenta"], 0, 4,
                first=2)
    add(p, "Backlight, 8 named colours: 16")

    p = Panel()
    choice_page(p, "Charge view", ["Weight", "Chart"], 0, 1)
    add(p, "Charge view, weight or chart: 14")

    p = Panel(); m.title(p, "Servo gate", back=True)
    rows(p, [("Servo gate", "On", False), ("Shutters", "Closed", False), ("Shutter 0 closed", "0.050", False),
             ("Shutter 0 open", "0.120", False)], 1, total=6)
    add(p, "Restored: the gate, a click: 10")

    p = Panel(); m.title(p, "Servo gate", back=True)
    rows(p, [("Shutters", "Closed", False), ("Shutter 0 closed", "0.055", False), ("Shutter 0 open", "0.120", False),
             ("Shutter 1 closed", "0.050", False)], 1, first=1, total=6, editing=True)
    add(p, "A shutter's end, the servo moving: 17")

    p = Panel(); m.title(p, "Motors", back=True)
    rows(p, [("Coarse direction", "Normal", False), ("Fine direction", "Reversed", False)], 1)
    add(p, "A motor's direction, a click: 11")

    p = Panel(); m.title(p, "Firmware", back=True)
    rows(p, [("Unit", "9CE00000", False), ("Build", "Release", False), ("Channel", "Stable", True),
             ("Check for update", "", False)], 2, first=3, total=7)
    add(p, "Channel opens its page: 13")

    p = Panel(); m.title(p, "Factory reset")
    m.message(p, "Erase every setting?", "It cannot be undone")
    m.softkeys(p, "Cancel", "Erase", focus="left")
    add(p, "Factory reset, opens on Cancel: 14")

    # --- Wi-Fi: its state, why a join failed, and on or off
    p = Panel(); m.title(p, "Wi-Fi", back=True)
    m.dense(p, [("Joined", "HomeNet", "pair"), ("IP", "192.168.2.168", "pair"),
                ("Name", "opentrickler-9CE0.local", "pair"), ("Signal", "-59 dBm", "pair")])
    add(p, "Wi-Fi: its state")

    p = Panel(); m.title(p, "Wi-Fi", back=True)
    m.dense(p, [("Not joined", "Wrong key", "pair"), ("Network", "HomeNet", "pair"), ("Turn Wi-Fi off", "button",
                                                                                     True)], at="end")
    add(p, "Restored: why it did not join; off 6")
    return out, caps


def main():
    """
    Draw the sheet.

    Returns:
        The exit code.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=m.DEFAULT_OUT, help="the folder written")
    args = parser.parse_args()
    m.use_faces(m.DEFAULT_TEXT, m.DEFAULT_BOLD, m.DEFAULT_FIGURE)
    panels, captions = screens()
    write_sheet(args.out / "parity_12864.png", panels, captions, m.CAPTION)
    print(f"{len(panels)} screens: {args.out / 'parity_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
