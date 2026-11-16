"""
The menu's screens on the mini 12864, as designed (ADR-010; `.agents/skills/embedded-ui-design` is the guideline
this follows). Draws each screen as the unit would, onto one sheet, so a change of design is seen before it is built.

    venv\\Scripts\\python tools\\ui_mock\\menu_12864.py [--out <folder>] [--text <font>] [--bold <font>] [--figure <font>]

The faces are names from u8g2's BDF files or its compiled catalogue; every baseline and the dense pages' line follow
from their heights, so another face is drawn in the same grid.

The design system, in one place:
  type      Helvetica, one family in three sizes (the owner, 2026-11-17: "All Helvetica"): helvR08 for all
            text, helvB08 for titles and emphasis (caps 8 px), helvB18 for the one figure a job screen is about
            (caps 19 px), its unit in the text face beside it.
  grid      a title band, its baseline at 9, a rule at 11; content from 13. Menus and lists: rows of 13 px, four
            a screen. Dense pages: lines of the text's height and 1 px (11 px). Softkeys on the bottom row, 53 to
            63.
  kinds     pages (lists), cards (read-only, dense), dialogs (a message, two softkeys), jobs (the title is the
            status; one figure; one line under it; two softkeys), notes (dense, their action at the end).
  softkeys  the left is Back (the reset button, or the knob held), plain; the right is a click, in reverse.
"""
import argparse
import sys
from pathlib import Path

from panel import Font, H, Panel, REPO, W, write_sheet

DEFAULT_TEXT, DEFAULT_BOLD, DEFAULT_FIGURE = "helvR08", "helvB08", "helvB18"
CAPTION = Font.bdf("helvR14")

TITLE_BASE, RULE_Y = 9, 11
CONTENT_TOP, ROW_H = 13, 13
SOFTKEY_TOP = 53
FIGURE_BASE, CONTEXT_BASE = 38, 49

# The faces, set by use_faces(): what follows from their heights is set with them
TEXT = BOLD = FIGURE = None
DENSE_H = ROW_BASE = DENSE_BASE = SOFTKEY_BASE = 0


def use_faces(text, bold, figure):
    """
    The faces the screens are drawn in, and what follows from their heights: a row's baseline (the text centred in
    the row's 12 px bar), the dense pages' line (the text's height and 1 px), the softkeys' baseline.

    Args:
        text: the text face's name.
        bold: the titles' face.
        figure: the figures' face.
    """
    global TEXT, BOLD, FIGURE, DENSE_H, ROW_BASE, DENSE_BASE, SOFTKEY_BASE
    TEXT, BOLD, FIGURE = Font.load(text), Font.load(bold), Font.load(figure)
    height = TEXT.cap + TEXT.descent
    ROW_BASE = (ROW_H - 1 - height) // 2 + TEXT.cap
    DENSE_H = height + 1
    DENSE_BASE = TEXT.cap
    SOFTKEY_BASE = SOFTKEY_TOP + (H - SOFTKEY_TOP - height) // 2 + TEXT.cap

# The title's Wi-Fi marks (scripts/bdf2lvgl.py, ICONS): full bars as a station, a hotspot as its own access point
BARS = ["         ##", "         ##", "      ## ##", "      ## ##", "   ## ## ##", "   ## ## ##",
        "## ## ## ##", "## ## ## ##"]
DEFAULT_OUT = REPO / "docs" / "developments_plans" / "ADR-010-lvgl" / "mockups"


def title(panel, text, back=False, sim=False):
    """
    The title band: its text, "< " when Back leads up a level, the marks at the right, a rule under it.

    Args:
        panel: the Panel.
        text: the title, or on a job screen its status.
        back: whether the title leads back.
        sim: the simulated scale's mark.
    """
    panel.text(BOLD, 1, TITLE_BASE, ("< " if back else "") + text)
    panel.icon(W - 11, 1, BARS)
    if sim:
        panel.text(TEXT, 0, TITLE_BASE, "SIM", align="right", right=W - 15)
    panel.hline(0, RULE_Y, W)


def scrollbar(panel, first, shown, total, top=CONTENT_TOP):
    """
    A scroll bar at the right edge, on a page that runs on: a 1 px track and a 3 px thumb as long as the share shown,
    as the unit draws it (a dotted track needs LVGL's complex drawing, off to save flash).

    Args:
        panel: the Panel.
        first: the first line or row shown.
        shown: how many fit.
        total: how many there are.
        top: where the content starts.
    """
    if total <= shown:
        return
    span = H - top
    length = max(4, span * shown // total)
    at = top + (span - length) * first // max(1, total - shown)
    panel.rect(W - 1, top, 1, span)
    panel.rect(W - 3, at, 3, length)


def rows(panel, items, focus, first=0, total=None):
    """
    A page's rows: label at the left, value at the right, ">" when the row opens something.

    Args:
        panel: the Panel.
        items: (label, value, opens), those shown.
        focus: the focused one's index, in reverse.
        first: the first row's index in the whole list.
        total: the rows in the whole list.
    """
    total = total or len(items)
    edge = W - 5 if total > 4 else W - 3
    for idx, (label, value, opens) in enumerate(items[:4]):
        top = CONTENT_TOP + idx * ROW_H
        base = top + ROW_BASE
        lit = idx != focus
        if idx == focus:
            panel.rect(0, top, edge + 2, ROW_H - 1)
        panel.text(TEXT, 3, base, label, lit=lit)
        right = edge
        if opens:
            panel.text(TEXT, 0, base, ">", lit=lit, align="right", right=right)
            right -= TEXT.width(">") + 3
        if value:
            panel.text(TEXT, 0, base, value, lit=lit, align="right", right=right)
    scrollbar(panel, first, 4, total)


def wrap(font, text, width):
    """
    A paragraph broken into lines at spaces, each no wider than `width`.

    Args:
        font: the Font it is drawn in.
        text: the paragraph.
        width: the widest line, in pixels.

    Returns:
        Its lines.
    """
    lines, line = [], ""
    for word in text.split():
        trial = f"{line} {word}" if line else word
        if line and font.width(trial) > width:
            lines.append(line)
            line = word
        else:
            line = trial
    return lines + ([line] if line else [])


def dense(panel, items, at="top"):
    """
    A dense page: paragraphs wrapped to the panel's width in the text face, one line each `DENSE_H`, shown from its
    top or its end, with a scroll bar when it runs on.

    Args:
        panel: the Panel.
        items: the page, each a paragraph, (heading, "bold"), or (label, "button", focused).
        at: "top" or "end", where the page is shown from.
    """
    shown = (H - CONTENT_TOP) // DENSE_H
    width = W - 3 - 5
    lines = []
    for item in items:
        if isinstance(item, tuple) and item[1] == "button":
            lines.append(item)
        elif isinstance(item, tuple):
            lines.extend((line, "bold") for line in wrap(BOLD, item[0], width))
        else:
            lines.extend(wrap(TEXT, item, width))
    first = 0 if at == "top" else max(0, len(lines) - shown)
    for idx, line in enumerate(lines[first:first + shown]):
        y = CONTENT_TOP + idx * DENSE_H
        base = y + DENSE_BASE
        if isinstance(line, tuple) and line[1] == "button":
            if line[2]:
                panel.rect(2, y, W - 8, DENSE_H)
                panel.text(TEXT, 0, base, line[0], lit=False, align="center")
            else:
                panel.frame(2, y, W - 8, DENSE_H)
                panel.text(TEXT, 0, base, line[0], align="center")
        elif isinstance(line, tuple):
            panel.text(BOLD, 3, base, line[0])
        else:
            panel.text(TEXT, 3, base, line)
    scrollbar(panel, first, shown, len(lines))


def softkeys(panel, left, right):
    """
    The bottom row's softkeys.

    Args:
        panel: the Panel.
        left: what Back does, or "".
        right: what a click does, or "".
    """
    if left:
        panel.text(TEXT, 1, SOFTKEY_BASE, left)
    if right:
        width = TEXT.width(right)
        panel.rect(W - width - 6, SOFTKEY_TOP, width + 6, H - SOFTKEY_TOP)
        panel.text(TEXT, 0, SOFTKEY_BASE, right, lit=False, align="right", right=W - 3)


def figure(panel, value, unit, base=FIGURE_BASE):
    """
    A job screen's figure, centred with its unit.

    Args:
        panel: the Panel.
        value: the figure's text.
        unit: its unit, in the text face.
        base: its baseline.

    Returns:
        (its left edge, its width).
    """
    width = FIGURE.width(value)
    unit_w = TEXT.width(unit) + 2 if unit else 0
    x = (W - width - unit_w) // 2
    panel.text(FIGURE, x, base, value)
    if unit:
        panel.text(TEXT, x + width + 2, base, unit)
    return x, width


def context(panel, text, bold=False):
    """The line under a job screen's figure, centred."""
    panel.text(BOLD if bold else TEXT, 0, CONTEXT_BASE, text, align="center")


def message(panel, first, second):
    """A dialog's message, two lines centred."""
    panel.text(TEXT, 0, 29, first, align="center")
    panel.text(TEXT, 0, 40, second, align="center")


def screens():
    """
    Every screen of the menu as designed.

    Returns:
        (panels, captions).
    """
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    p = Panel(); title(p, "OpenTrickler")
    rows(p, [("Charge", "", True), ("Cleanup", "", False), ("Wi-Fi", "", True), ("Settings", "", True)], 0)
    add(p, "Main menu")

    p = Panel(); title(p, "OpenTrickler")
    rows(p, [("Update 9.0.1", "", True), ("Charge", "", True), ("Cleanup", "", False), ("Wi-Fi", "", True)], 0,
         total=5)
    add(p, "Main menu, an update found")

    p = Panel(); title(p, "Charge", back=True)
    rows(p, [("Start", "", False), ("Target", "30.00 gr", True), ("Powder", "AR2209", True)], 0)
    add(p, "Charge page")

    p = Panel(); title(p, "Charge")
    message(p, "Put the empty cup", "on the scale")
    softkeys(p, "Cancel", "Start")
    add(p, "Before a charge")

    p = Panel(); title(p, "Waiting for zero")
    figure(p, "0.00", "gr"); context(p, "of 30.00")
    softkeys(p, "Stop", "Zero")
    add(p, "Charge: waiting for zero")

    p = Panel(); title(p, "Charging", sim=True)
    figure(p, "20.08", "gr"); context(p, "of 30.00")
    softkeys(p, "Stop", "")
    add(p, "Charge: charging (simulated scale)")

    p = Panel(); title(p, "Lift the cup")
    figure(p, "30.00", "gr"); context(p, "Good", bold=True)
    softkeys(p, "Stop", "Target")
    add(p, "Charge: the throw done")

    p = Panel(); title(p, "Put the cup back")
    figure(p, "30.04", "gr"); context(p, "Over by 0.04", bold=True)
    softkeys(p, "Stop", "Target")
    add(p, "Charge: cup away, the throw kept")

    p = Panel(); title(p, "Cleanup: running")
    figure(p, "2.0", "rev/s"); context(p, "Turn the knob for speed")
    softkeys(p, "Stop", "Pause")
    add(p, "Cleanup")

    p = Panel(); title(p, "Target")
    x, _ = figure(p, "030.00", "gr")
    p.rect(x + FIGURE.width("0"), FIGURE_BASE + 3, FIGURE.width("3") - 1, 2)
    softkeys(p, "Cancel", "Next")
    add(p, "Setting the target")

    p = Panel(); title(p, "Scale", back=True)
    rows(p, [("Reading", "30.02 gr", False), ("Zero", "", False), ("Make", "A&D FX-i", True),
             ("Baud rate", "19200", True)], 1, total=5)
    add(p, "A settings page")

    p = Panel(); title(p, "Wi-Fi", back=True)
    dense(p, ["Joined HomeNet", "IP 192.168.2.168", "opentrickler-9CE0.local", "Signal -59 dBm (good)",
              "Updates from github.com"])
    add(p, "Wi-Fi: a dense card")

    note = [("9.0.1, important", "bold"), "Fixes the scale's rhythm on the A&D FX-i, and the cup's return on a slow "
            "scale.", ("Before you install", "bold"), "Keep the power on: about a minute.", "Your settings are kept.",
            ("Install", "button", True)]
    p = Panel(); title(p, "Update", back=True)
    dense(p, note, at="top")
    add(p, "Update notes: the top")

    p = Panel(); title(p, "Update", back=True)
    dense(p, note, at="end")
    add(p, "Update notes: Install at the end")

    p = Panel(); title(p, "Downloading 9.0.1")
    figure(p, "62", "%"); context(p, "Keep the power on")
    softkeys(p, "Stop", "")
    add(p, "The update downloading")

    p = Panel(); title(p, "Factory reset")
    message(p, "Erase every setting?", "It cannot be undone")
    softkeys(p, "Cancel", "Erase")
    add(p, "Factory reset")
    return out, caps


def main():
    """
    Draw the sheet, and each screen alone.

    Returns:
        The exit code.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="the folder written")
    parser.add_argument("--text", default=DEFAULT_TEXT, help="the text face")
    parser.add_argument("--bold", default=DEFAULT_BOLD, help="the titles' face")
    parser.add_argument("--figure", default=DEFAULT_FIGURE, help="the figures' face")
    args = parser.parse_args()
    use_faces(args.text, args.bold, args.figure)
    panels, captions = screens()
    write_sheet(args.out / "menu_12864.png", panels, captions, CAPTION)
    for idx, panel in enumerate(panels):
        write_sheet(args.out / "screens" / f"{idx:02d}.png", [panel], scale=4, gap=8)
    print(f"{len(panels)} screens: {args.out / 'menu_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
