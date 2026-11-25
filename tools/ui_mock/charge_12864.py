"""
ADR-009 step 2, gate 3: the charge's turn from one throw to the next on the mini 12864, mocked for the owner before
it is built (`.agents/skills/embedded-ui-design` is the guideline), in the menu's faces and parts (menu_12864.py),
onto one sheet.

The owner, 2026-11-25 at the bench: "Only the part that needs human to put back the cup should have the
confirmation." Today the next throw pours by itself once the cup is back and the pan reads steady at zero, so a bump
read as the cup's return could start a pour. The new step: once the cup is back and steady at zero, the charge waits
for the person's click (Ready, Start focused); a reading that leaves zero before it goes back to Zeroing. The first
throw of a charge needs none: its Start was the press. The figures are a 30 gr charge of AR2209.

    venv\\Scripts\\python tools\\ui_mock\\charge_12864.py [--out <folder>]
"""
import argparse
import sys
from pathlib import Path

import menu_12864 as m
from panel import Panel, REPO, write_sheet

DEFAULT_OUT = REPO / "docs" / "developments_plans" / "ADR-009-controller_and_tuning" / "mockups"
CHARGE = "of 30.00 gr, AR2209"


def screens():
    """
    The charge's screens from one throw's end to the next one's start, in the order a person meets them.

    Returns:
        (panels, captions).
    """
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    p = Panel(); m.title(p, "Charging")
    m.figure(p, "28.71", "gr"); m.context(p, CHARGE)
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Charging, as today")

    p = Panel(); m.title(p, "Lift the cup")
    m.figure(p, "30.01", "gr"); m.context(p, "Good", bold=True)
    m.softkeys(p, "Stop", "Change", focus="left")
    add(p, "The verdict; lift the cup, as today")

    p = Panel(); m.title(p, "Return the cup")
    m.figure(p, "30.01", "gr"); m.context(p, "Good", bold=True)
    m.softkeys(p, "Stop", "Change", focus="left")
    add(p, "Emptied, the cup to return, as today")

    p = Panel(); m.title(p, "Zeroing")
    m.figure(p, "0.00", "gr"); m.context(p, CHARGE)
    m.softkeys(p, "Stop", "Zero")
    add(p, "Cup back: waits for a steady zero, as today")

    p = Panel(); m.title(p, "Ready")
    m.figure(p, "0.00", "gr"); m.context(p, CHARGE)
    m.softkeys(p, "Stop", "Start")
    add(p, "NEW: steady at zero, waits for the click")

    p = Panel(); m.title(p, "Charging")
    m.figure(p, "0.41", "gr"); m.context(p, CHARGE)
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Clicked: the next throw pours")
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
    write_sheet(args.out / "charge_12864.png", panels, captions, m.CAPTION)
    print(f"{len(panels)} screens: {args.out / 'charge_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
