"""
The menu's screens on the mini 12864, as designed (ADR-010; `.agents/skills/embedded-ui-design` is the guideline
this follows). Draws each screen as the unit would, onto one sheet, so a change of design is seen before it is built.

    venv\\Scripts\\python tools\\ui_mock\\menu_12864.py [--out <folder>]

The design system, in one place:
  type      Nokia Small: bold for titles and emphasis, plain for all text (caps 7 px). Logisoso 22 for the one
            figure a job screen is about, its unit in the text face beside it.
  grid      a title band, its baseline at 9, a rule at 11; content from 13. Menus and lists: rows of 13 px, four
            a screen. Dense pages: lines of 10 px. Softkeys on the bottom row, 53 to 63.
  kinds     pages (lists), cards (read-only, dense), dialogs (a message, two softkeys), jobs (the title is the
            status; one figure; one line under it; two softkeys), notes (dense, their action at the end).
  softkeys  the left is Back (the reset button, or the knob held), plain; the right is a click, in reverse.
"""
import argparse
import sys
from pathlib import Path

from panel import Font, H, Panel, REPO, W, write_sheet

TEXT = Font.u8g2("NokiaSmallPlain_tr")
BOLD = Font.u8g2("NokiaSmallBold_tr")
FIGURE = Font.u8g2("logisoso22_tr")
CAPTION = Font.bdf("helvR14")

TITLE_BASE, RULE_Y = 9, 11
CONTENT_TOP, ROW_H, DENSE_H = 13, 13, 10
SOFTKEY_TOP, SOFTKEY_BASE = 53, 61
FIGURE_BASE, CONTEXT_BASE = 38, 49

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
    A scroll bar at the right edge, on a page that runs on: a dotted track and a thumb as long as the share shown.

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
    for y in range(top, H, 2):
        panel.set(W - 1, y)
    panel.rect(W - 2, at, 2, length)


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
        base = top + 9
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


def dense(panel, lines, first=0, total=None):
    """
    A dense page's lines of 10 px: each a text, (text, "bold"), or (text, "button", focused).

    Args:
        panel: the Panel.
        lines: those shown.
        first: the first line's index in the whole page.
        total: the lines in the whole page.
    """
    total = total or len(lines)
    for idx, line in enumerate(lines):
        y = CONTENT_TOP + idx * DENSE_H
        if y + DENSE_H > H + 1:
            break
        base = y + 8
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
    scrollbar(panel, first, (H - CONTENT_TOP) // DENSE_H, total)


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

    note = [("9.0.1, important", "bold"), "Fixes the scale's rhythm on", "the A&D FX-i, and the cup's",
            "return on a slow scale.", ("Before you install", "bold"), "Keep the power on: about a", "minute.",
            "Your settings are kept.", ("Install", "button", True)]
    p = Panel(); title(p, "Update", back=True)
    dense(p, note[:5], first=0, total=len(note))
    add(p, "Update notes: the top")

    p = Panel(); title(p, "Update", back=True)
    dense(p, note[4:], first=4, total=len(note))
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
    args = parser.parse_args()
    panels, captions = screens()
    write_sheet(args.out / "menu_12864.png", panels, captions, CAPTION)
    for idx, panel in enumerate(panels):
        write_sheet(args.out / "screens" / f"{idx:02d}.png", [panel], scale=4, gap=8)
    print(f"{len(panels)} screens: {args.out / 'menu_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
