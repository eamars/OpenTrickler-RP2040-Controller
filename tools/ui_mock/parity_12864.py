"""
The screens ADR-010's survey adds (`menu_design.md`, "Every setting on the screen and on the web"), mocked for the
owner to settle before they are built: the settings pages made from the settings tables, the editors they need
(number, text, colour, the network list), Wi-Fi's settings, the cleanup as one system, and the charge's screen with
the powder, the time and the throw's number, with and without a chart.

    venv\\Scripts\\python tools\\ui_mock\\parity_12864.py [--out <folder>]

Drawn with menu_12864.py's faces, grid and parts, so they sit beside its sheet.
"""
import argparse
import sys
from pathlib import Path

import menu_12864 as m
from panel import H, Panel, W, write_sheet

LIST_ROWS = 4


def rows(panel, items, focus, first=0, total=None, editing=False):
    """
    A list page's rows, as menu_12864.rows, and one more state: the focused row's value being set (editing), shown
    with the row plain and its value in reverse.

    Args:
        panel: the Panel.
        items: (label, value, opens), those shown.
        focus: the focused row's index among those shown, or None.
        first: the first row's index in the whole list.
        total: the rows in the whole list.
        editing: whether the focused row's value is being set.
    """
    total = total or len(items)
    edge = W - 5 if total > LIST_ROWS else W - 3
    for idx, (label, value, opens) in enumerate(items[:LIST_ROWS]):
        top = m.CONTENT_TOP + idx * m.ROW_H
        base = top + m.ROW_BASE
        reverse = idx == focus and not editing
        if reverse:
            panel.rect(0, top, edge + 2, m.ROW_H - 1)
        panel.text(m.TEXT, 3, base, label, lit=not reverse)
        right = edge
        if opens:
            panel.text(m.TEXT, 0, base, ">", lit=not reverse, align="right", right=right)
            right -= m.TEXT.width(">") + 3
        if value:
            if idx == focus and editing:
                width = m.TEXT.width(value) + 4
                panel.rect(right - width + 2, top, width, m.ROW_H - 1)
                panel.text(m.TEXT, 0, base, value, lit=False, align="right", right=right)
            else:
                panel.text(m.TEXT, 0, base, value, lit=not reverse, align="right", right=right)
    m.scrollbar(panel, first, LIST_ROWS, total)


def digits_editor(panel, text, focus, editing, unit=""):
    """
    The number editor, as the target's: the figure with the focused digit underlined, or in reverse while it is set.

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


def text_editor(panel, text, focus, editing, wheel=None, count=None):
    """
    The text editor: the text on one line, the focused place underlined (or in reverse while it is set), "+" after
    the last character to add one; while a character is set, the characters around it in the set under the line.

    Args:
        panel: the Panel.
        text: the text.
        focus: the focused place: a character's index, len(text) for "+", or None when a softkey has the focus.
        editing: whether the focused character is being set.
        wheel: the characters around the one being set, it in the middle, or None.
        count: "n of max", or None.
    """
    shown = text + "+"
    base = 29
    x = 3
    # A long text shows its end: the place being set stays in sight
    while m.TEXT.width(shown) > W - 8 and len(shown) > 1:
        shown = shown[1:]
        focus = None if focus is None else focus - 1
        x = 3
    if m.TEXT.width(shown) < W - 6:
        panel.text(m.TEXT, x, base, shown)
    else:
        panel.text(m.TEXT, x, base, shown)
    if focus is not None and focus >= 0:
        at = x + m.TEXT.width(shown[:focus])
        width = m.TEXT.width(shown[focus])
        if editing:
            panel.rect(at - 1, base - m.TEXT.cap - 1, width + 1, m.TEXT.cap + 3)
            panel.text(m.TEXT, at, base, shown[focus], lit=False)
        else:
            panel.rect(at, base + 2, width, 1)
    if wheel:
        panel.text(m.TEXT, 0, 42, wheel, align="center")
    elif count:
        panel.text(m.TEXT, 0, 42, count, align="center")


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
                yy = py + (y - py) * step // max(1, abs(x - px))
                panel.set(px + step, yy)
        prev = (x, y)
    panel.hline(left, bottom, right - left)


def screens():
    """
    Every screen the survey adds, in the order a person meets them.

    Returns:
        (panels, captions).
    """
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    # --- Settings: every resource of the tables
    p = Panel(); m.title(p, "Settings", back=True)
    rows(p, [("Charge", "", True), ("Powder profiles", "", True), ("Scale", "", True), ("Motors", "", True)], 0,
         total=11)
    add(p, "Settings: every resource")

    p = Panel(); m.title(p, "Settings", back=True)
    rows(p, [("Wi-Fi", "", True), ("Display", "", True), ("Firmware", "", True), ("Restart", "", False)], 0,
         first=6, total=11)
    add(p, "Settings, further down")

    p = Panel(); m.title(p, "Settings", back=True)
    rows(p, [("Servo gate", "", True), ("LEDs", "", True), ("Wi-Fi", "", True), ("Display", "", True)], 0,
         first=4, total=11)
    add(p, "Settings: the gate, the LEDs")

    # --- Charge's settings, by their groups
    p = Panel(); m.title(p, "Charge", back=True)
    rows(p, [("Thresholds", "", True), ("Coarse stop", "", True), ("Precharge", "", True), ("Colours", "", True)],
         0, total=5)
    add(p, "Charge: its groups")

    p = Panel(); m.title(p, "Thresholds", back=True)
    rows(p, [("Coarse stop", "1.00 gr", True), ("Fine stop", "0.03 gr", True), ("Steady margin", "0.02 gr", True),
             ("Zero margin", "0.04 gr", True)], 1)
    add(p, "A group's numbers")

    p = Panel(); m.title(p, "Coarse stop", back=True)
    rows(p, [("Gate at stop", "0.50", True), ("Back off", "On", False), ("Turns", "0.250", True),
             ("Speed", "2.00 rev/s", True)], 2)
    add(p, "Short labels within a group")

    p = Panel(); m.title(p, "Back-off turns")
    digits_editor(p, "0.250", 2, False, "turns")
    m.context(p, "0.125 to 2.000")
    m.softkeys(p, "Cancel", "Set", focus=None)
    add(p, "Number editor: a digit focused")

    p = Panel(); m.title(p, "Back-off turns")
    digits_editor(p, "0.375", 2, True, "turns")
    m.context(p, "0.125 to 2.000")
    m.softkeys(p, "Cancel", "Set", focus=None)
    add(p, "Number editor: the digit set")

    p = Panel(); m.title(p, "Not changed")
    m.message(p, "Not while", "a job runs")
    m.softkeys(p, "", "OK")
    add(p, "A refused write (motors in a job)")

    # --- Colours
    p = Panel(); m.title(p, "Colours", back=True)
    rows(p, [("Good throw", "#00ff00", True), ("Under", "#ffff00", True), ("Over", "#ff0000", True),
             ("Not ready", "#0000ff", True)], 0)
    add(p, "Colours as #rrggbb")

    p = Panel(); m.title(p, "Good throw", back=True)
    rows(p, [("Red", "0", True), ("Green", "255", True), ("Blue", "0", True)], 1)
    add(p, "A colour: three numbers, the LED shows it")

    # --- Powder profiles
    p = Panel(); m.title(p, "Powder profiles", back=True)
    rows(p, [("AR2208", "", True), ("AR2209", "", True), ("Varget", "", True), ("Profile 4", "", True)], 1,
         total=8)
    add(p, "Powder profiles: by name")

    p = Panel(); m.title(p, "AR2209", back=True)
    rows(p, [("Name", "AR2209", True), ("Coarse trickler", "", True), ("Fine trickler", "", True)], 1)
    add(p, "A profile: its groups")

    p = Panel(); m.title(p, "Coarse trickler", back=True)
    rows(p, [("Kp", "0.0250", True), ("Ki", "0.0000", True), ("Kd", "0.3000", True),
             ("Minimum", "0.08 rev/s", True)], 0, total=5)
    add(p, "A profile's gains")

    p = Panel(); m.title(p, "Name")
    text_editor(p, "AR2209", 6, False, count="6 of 15")
    m.softkeys(p, "Cancel", "Set", focus=None)
    add(p, "Text editor: '+' adds a character")

    p = Panel(); m.title(p, "Name")
    text_editor(p, "AR2209", 5, True, wheel="6  7  8  [9]  A  B  C")
    m.softkeys(p, "Cancel", "Set", focus=None)
    add(p, "Text editor: a character set")

    # --- Motors, gate, LEDs
    p = Panel(); m.title(p, "Coarse motor", back=True)
    rows(p, [("Motor", "", True), ("Driver", "", True), ("Wiring", "", True)], 1)
    add(p, "A motor: its groups")

    p = Panel(); m.title(p, "Driver", back=True)
    rows(p, [("Current", "800 mA", True), ("Microsteps", "16", True), ("R sense", "110 mOhm", True)], 0)
    add(p, "A motor's driver")

    p = Panel(); m.title(p, "Servo gate", back=True)
    rows(p, [("Gate", "On", False), ("Open", "", False), ("Close", "", False), ("Position", "0.50", True)], 1,
         total=6)
    add(p, "Servo gate: moved here, set up below")

    p = Panel(); m.title(p, "LEDs", back=True)
    rows(p, [("Backlight", "#ffffff", True), ("LED 1", "#ffffff", True), ("LED 2", "#ffffff", True),
             ("PWM3", "", True)], 0)
    add(p, "LEDs")

    # --- Scale, Firmware, Wi-Fi
    p = Panel(); m.title(p, "Scale", back=True)
    rows(p, [("Make", "A&D FX-i", True), ("Baud rate", "19200", True), ("Frame format", "8N1", True),
             ("Restart to apply", "", False)], 2, first=2, total=6)
    add(p, "Scale: its frame format")

    p = Panel(); m.title(p, "Firmware", back=True)
    rows(p, [("Board", "pico_w", False), ("Channel", "Stable", True), ("Source", "github.com", True),
             ("Check for update", "", False)], 1, first=3, total=7)
    add(p, "Firmware: channel and source set")

    p = Panel(); m.title(p, "Wi-Fi", back=True)
    m.dense(p, [("State", "Wrong key", "pair"), ("Network", "HomeNet", "pair"), ("IP", "none", "pair"),
                ("Signal", "-59 dBm", "pair")])
    add(p, "Wi-Fi: why it did not join")

    p = Panel(); m.title(p, "Wi-Fi", back=True)
    rows(p, [("Wi-Fi", "On", False), ("Network", "HomeNet", True), ("Key", "********", True),
             ("Security", "WPA2", True)], 1, total=5)
    add(p, "Settings > Wi-Fi")

    p = Panel(); m.title(p, "Network", back=True)
    rows(p, [("HomeNet", "-48 WPA2", False), ("Neighbour", "-71 WPA2", False), ("Cafe", "-80 Open", False),
             ("Other", "", True)], 0)
    add(p, "Network: the scan, strongest first")

    # --- The cleanup, as one system
    p = Panel(); m.title(p, "Cleanup")
    rows(p, [("Speed", "2.0 rev/s", False), ("Weight", "12.34 gr", False), ("Flow", "0.12 gr/s", False),
             ("Gate", "Open", False)], 0, total=6)
    add(p, "Cleanup: the speed focused")

    p = Panel(); m.title(p, "Cleanup")
    rows(p, [("Speed", "3.0 rev/s", False), ("Weight", "14.02 gr", False), ("Flow", "0.21 gr/s", False),
             ("Gate", "Open", False)], 0, total=6, editing=True)
    add(p, "Cleanup: the speed set live")

    p = Panel(); m.title(p, "Cleanup")
    rows(p, [("Flow", "0.21 gr/s", False), ("Gate", "Open", False), ("Zero", "", False), ("Stop", "", False)], 3,
         first=2, total=6)
    add(p, "Cleanup: Gate, Zero, Stop")

    # --- The charge, with the powder, the time and the throw's number
    p = Panel(); m.title(p, "Charging")
    p.text(m.TEXT, 0, m.TITLE_BASE, "#3  8.4 s", align="right", right=W - 15)
    m.figure(p, "20.08", "gr"); m.context(p, "of 30.00, AR2209")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Charge: no chart")

    p = Panel(); m.title(p, "Charging")
    p.text(m.TEXT, 0, m.TITLE_BASE, "#3  8.4 s", align="right", right=W - 15)
    p.text(m.BOLD, 3, 23, "20.08 / 30.00")
    p.text(m.TEXT, 0, 23, "AR2209", align="right", right=W - 3)
    curve = [min(1.0, (t / 40) ** 0.6 * 0.96) for t in range(41)]
    chart(p, 26, 51, curve, 1.0)
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Charge: with a chart")
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
