"""
The screens ADR-010's survey settled (`menu_design.md`, "Needed at the unit, and can be changed there"), mocked for
the owner before they are built. Each caption gives what its change costs a person at the knob, in actions (a
detent or a click) from the main menu at rest, as `knob_cost.py` counts them.

    venv\\Scripts\\python tools\\ui_mock\\parity_12864.py [--out <folder>]

Every value is edited in its row: a click, the turn, a click; the value in reverse while it is set. The target alone
keeps its screen of digits, the old menu's. Drawn with menu_12864.py's faces, grid and parts.
"""
import argparse
import sys
from pathlib import Path

import menu_12864 as m
from panel import Panel, W, write_sheet

LIST_ROWS = 4


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
    The cleanup's screen: its weight and flow on one line to read, then Speed and Gate as rows, and Stop and Zero as
    its softkeys. The turn goes Speed, Gate, Stop, Zero.

    Args:
        panel: the Panel.
        speed, weight, flow, gate: the values shown.
        focus: "speed", "gate", "stop" or "zero".
        editing: whether the speed is being set.
        title: its status.
    """
    m.title(panel, title)
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
    rows(p, [("Start", "", False), ("Target", "30.00 gr", True), ("Powder", "AR2209", False),
             ("Powder details", "", True)], 2)
    add(p, "Charge: Powder, a click to set")

    p = Panel(); m.title(p, "Charge", back=True)
    rows(p, [("Start", "", False), ("Target", "30.00 gr", True), ("Powder", "Varget", False),
             ("Powder details", "", True)], 2, editing=True)
    add(p, "Powder set in place: 9 actions")

    p = Panel(); m.title(p, "AR2209", back=True)
    lines = [("", "Coarse", "Fine"), ("Kp", "0.025", "0.040"), ("Ki", "0.000", "0.000"), ("Kd", "0.300", "0.150")]
    for idx, (key, coarse, fine) in enumerate(lines):
        base = m.CONTENT_TOP + idx * m.DENSE_H + m.DENSE_BASE
        font = m.BOLD if idx == 0 else m.TEXT
        p.text(font, 3, base, key)
        p.text(font, 0, base, coarse, align="right", right=78)
        p.text(font, 0, base, fine, align="right", right=W - 6)
    m.scrollbar(p, 0, 4, 6)
    add(p, "Restored: the old Profile details, read only")

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
    rows(p, [("Reading", "24.35 gr", False), ("Zero", "", False), ("Make", "Sartorius", False),
             ("Baud rate", "19200", False)], 2, total=6, editing=True)
    add(p, "Make set in place: 10")

    p = Panel(); m.title(p, "Scale", back=True)
    rows(p, [("Make", "A&D FX-i", False), ("Baud rate", "19200", False), ("Frame format", "8N1", False),
             ("Decimals", "2", False)], 2, first=2, total=6)
    add(p, "Frame format 11, decimals 12")

    p = Panel(); m.title(p, "Charge", back=True)
    rows(p, [("Fine stop", "0.05 gr", False), ("Coarse stop", "5.0 gr", False)], 0, editing=True)
    add(p, "Fine stop 10; coarse stop 5.0 to 3.0: 29")

    p = Panel(); m.title(p, "Not changed")
    m.message(p, "Not while", "a throw pours")
    m.softkeys(p, "", "OK")
    add(p, "A refused change")

    p = Panel(); m.title(p, "Display", back=True)
    rows(p, [("Invert knob", "Off", False), ("Invert screen", "On", False), ("Rotation", "Upside down", False),
             ("Backlight", "White", False)], 1, total=5)
    add(p, "Invert screen, a click: 9")

    p = Panel(); m.title(p, "Display", back=True)
    rows(p, [("Invert knob", "Off", False), ("Invert screen", "On", False), ("Rotation", "Upside down", False),
             ("Backlight", "Green", False)], 3, total=5, editing=True)
    add(p, "Backlight, 8 named colours: 16")

    p = Panel(); m.title(p, "Display", back=True)
    rows(p, [("Invert screen", "On", False), ("Rotation", "Upside down", False), ("Backlight", "White", False),
             ("Charge view", "Chart", False)], 3, first=1, total=5, editing=True)
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
    rows(p, [("Unit", "9CE00000", False), ("Build", "Release", False), ("Channel", "Stable", False),
             ("Check for update", "", False)], 2, first=3, total=7)
    add(p, "Channel set in place: 13")

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
