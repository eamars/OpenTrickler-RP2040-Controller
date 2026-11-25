"""
ADR-009 step 1's calibration on the mini 12864, mocked for the owner before gate 4 builds it
(`.agents/skills/embedded-ui-design` is the guideline), in the menu's faces, grid and parts (menu_12864.py,
parity_12864.py), onto one sheet.

    venv\\Scripts\\python tools\\ui_mock\\calibration_12864.py [--out <folder>]

The calibration has three parts, each run alone (the cup, the scale, a powder), each a row of its own page. Its
screens are of the seven kinds the skill allows: lists, dialogs, jobs, notes and one control (the cup filled by the
tubes' speeds set in place, as the purge's). The figures are the bench's of 2026-11-25 (ADR-009's progress).
"""
import argparse
import sys
from pathlib import Path

import menu_12864 as m
import parity_12864 as par
from panel import Panel, REPO, W, write_sheet

# A powder's result as the firmware draws it (menu_nav.c): the 12864's notes give a label and a value on one line,
# so each tube is a heading over its Minimum and Was
POWDER_RESULT = [("Fine tube", "bold"), ("Minimum", "0.02 rev/s", "pair"), ("Was", "0.08 rev/s", "pair"),
                 ("Coarse tube", "bold"), ("Minimum", "0.16 rev/s", "pair"), ("Was", "0.1 rev/s", "pair"),
                 ("Took", "6 min, 210 gr", "pair"), ("Save", "button", True)]

DEFAULT_OUT = REPO / "docs" / "developments_plans" / "ADR-009-controller_and_tuning" / "mockups"


def fill_control(panel, weight, flow, fine, coarse, focus, editing=False):
    """
    The cup filled by the user: the purge's control, its line the cup's weight and flow, the tubes' speeds as rows
    set in place, Cancel and Full as its softkeys. The turn goes the title, Fine tube, Coarse tube, Cancel, Full.

    Args:
        panel: the Panel.
        weight, flow: the line to read.
        fine, coarse: the tubes' speeds.
        focus: "title", "fine", "coarse", "cancel" or "full".
        editing: whether the focused speed is being set.
    """
    m.title(panel, "Fill the cup", back=True)
    if focus == "title":
        par.title_focused(panel, "Fill the cup")
    panel.text(m.TEXT, 3, m.CONTENT_TOP + m.DENSE_BASE, weight)
    panel.text(m.TEXT, 0, m.CONTENT_TOP + m.DENSE_BASE, flow, align="right", right=W - 3)
    order = {"fine": 0, "coarse": 1}
    par.rows(panel, [("Fine tube", fine, False), ("Coarse tube", coarse, False)], order.get(focus), editing=editing,
             top=m.CONTENT_TOP + m.DENSE_H + 1, shown=2)
    m.softkeys(panel, "Cancel", "Full", focus={"cancel": "left", "full": "right"}.get(focus))


def screens():
    """
    Every screen of the calibration, in the order a person meets them.

    Returns:
        (panels, captions).
    """
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    # --- Where it is
    p = Panel(); m.title(p, "OpenTrickler")
    m.rows(p, [("Calibrate this unit", "", True), ("Charge", "", True), ("Purge", "", False), ("Wi-Fi", "", True)], 0,
           total=5)
    add(p, "Main menu: offered until calibrated")

    p = Panel(); m.title(p, "Settings", back=True)
    m.rows(p, [("Calibration", "", True), ("Scale", "", True), ("Charge", "", True), ("Display", "", True)], 0,
           total=9)
    add(p, "Settings: Calibration, kept for good")

    p = Panel(); m.title(p, "Calibration", back=True)
    m.rows(p, [("Cup", "150 gr", True), ("Scale", "Not done", True), ("Powder", "AR2209", True)], 1)
    add(p, "Calibration: three parts, each alone")

    # --- The cup
    p = Panel(); m.title(p, "Cup")
    m.message(p, "Put the empty cup", "on the scale")
    m.softkeys(p, "Cancel", "Next")
    add(p, "Cup: the empty cup on the scale")

    p = Panel(); m.title(p, "Zeroing")
    m.figure(p, "0.00", "gr")
    m.softkeys(p, "Cancel", "", focus="left")
    add(p, "Every part: zeroed with the empty cup")

    p = Panel(); m.title(p, "Lift the cup")
    m.figure(p, "0.00", "gr"); m.context(p, "Its weight is read")
    m.softkeys(p, "Cancel", "", focus="left")
    add(p, "Cup: lifted, the reading falls by its weight")

    p = Panel(); m.title(p, "Put the cup back")
    m.figure(p, "-77.30", "gr"); m.context(p, "The cup weighs 77.30 gr")
    m.softkeys(p, "Cancel", "", focus="left")
    add(p, "Cup: its weight found, back on the pan")

    p = Panel(); fill_control(p, "0.00 gr", "0.00 gr/s", "0.0 rev/s", "0.0 rev/s", "coarse")
    add(p, "Fill: the purge's control")

    p = Panel(); fill_control(p, "112.40 gr", "9.60 gr/s", "0.0 rev/s", "1.0 rev/s", "coarse", editing=True)
    add(p, "Fill: the coarse tube set live")

    p = Panel(); fill_control(p, "150.20 gr", "0.00 gr/s", "0.0 rev/s", "0.0 rev/s", "full")
    add(p, "Fill the cup: as full as wanted, Full")

    p = Panel(); m.title(p, "Cup")
    m.message(p, "It holds 150.20 gr", "Keep it as its capacity?")
    m.softkeys(p, "Cancel", "Keep")
    add(p, "Cup: the capacity confirmed")

    p = Panel(); m.title(p, "Empty the cup")
    m.message(p, "Then put it back", "on the scale")
    m.softkeys(p, "", "Done")
    add(p, "Cup: emptied before anything else pours")

    # --- The scale
    p = Panel(); m.title(p, "Scale")
    m.message(p, "Put the empty cup", "on the scale")
    m.softkeys(p, "Cancel", "Start")
    add(p, "Scale: the cup's step before it pours")

    p = Panel(); m.title(p, "Calibrating")
    m.figure(p, "35", "%"); m.context(p, "The scale's lag")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Scale: running, its step named")

    p = Panel(); m.title(p, "Scale", back=True)
    m.dense(p, [("Settle time", "0.27 s", "pair"), ("Lag, slow flow", "1.2 s", "pair"), ("Lag, fast flow", "0.6 s", "pair"),
                ("Took", "4.6 min, 62 gr", "pair"), ("Keep", "button", True)], at="top")
    add(p, "Scale: what it found, Keep at the end")

    # --- A powder
    p = Panel(); m.title(p, "AR2209")
    for base, text in ((26, "AR2209 in the hopper"), (37, "Put the empty cup"), (48, "on the scale")):
        p.text(m.TEXT, 0, base, text, align="center")       # a dialog's three lines (menu_view_12864.c)
    m.softkeys(p, "Cancel", "Start")
    add(p, "Powder: the one in the hopper, the cup first")

    p = Panel(); m.title(p, "Calibrating")
    m.figure(p, "62", "%"); m.context(p, "Fine tube 0.42 rev/s")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Powder: running, the speed it is at")

    p = Panel(); m.title(p, "Empty the cup")
    m.message(p, "It holds 148 of 150 gr", "Then go on")
    m.softkeys(p, "Stop", "Go on")
    add(p, "Powder: paused before the cup is full")

    p = Panel(); m.title(p, "AR2209", back=True)
    m.dense(p, POWDER_RESULT, at="top")
    add(p, "Powder: found, beside the profile's")

    p = Panel(); m.title(p, "AR2209", back=True)
    m.dense(p, POWDER_RESULT, at="end")
    add(p, "Powder: Save at the end")

    p = Panel(); m.title(p, "AR2209")
    m.message(p, "Fine tube: no powder", "Its minimum is kept")
    m.softkeys(p, "", "OK")
    add(p, "Powder: a step that failed, nothing retried")
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
    m.use_faces(m.DEFAULT_TEXT, m.DEFAULT_BOLD, m.DEFAULT_FIGURE)
    panels, captions = screens()
    write_sheet(args.out / "calibration_12864.png", panels, captions, m.CAPTION)
    for idx, panel in enumerate(panels):
        write_sheet(args.out / "calibration_screens" / f"{idx:02d}.png", [panel], scale=4, gap=8)
    print(f"{len(panels)} screens: {args.out / 'calibration_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
