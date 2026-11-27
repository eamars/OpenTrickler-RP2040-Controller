"""
ADR-014's charge profile on the mini 12864, mocked for the owner before it is built (`.agents/skills/embedded-ui-design`
is the guideline), in the menu's faces, grid and parts (menu_12864.py, parity_12864.py), onto one sheet.

    venv\\Scripts\\python tools\\ui_mock\\charge_profiles_12864.py [--out <folder>]

One charge profile is always chosen, and the Charge page's target and powder are its own. The sheet draws the page,
the choice's own page and the charge's screens in each of the two words the row could have, "Load" and "Charge", for
the owner to choose between (ADR-014, "Settled at the approval", 4), and the charge's own line as it is today beside
one that names the profile.
"""
import argparse
import sys
from pathlib import Path

import menu_12864 as m
import parity_12864 as par
from panel import Panel, REPO, W, write_sheet

DEFAULT_OUT = REPO / "docs" / "developments_plans" / "ADR-014-charge_profiles" / "mockups"

# A name is 15 characters at the most, as a powder's
LONGEST_NAME = "6.5CM 140 ELD-M"


def names(word):
    """
    The eight profiles of a unit that named two of them.

    Args:
        word: the row's word, which the names never set keep ("Load 3").

    Returns:
        The eight names.
    """
    return ["308 match", "223 varmint"] + [f"{word} {idx}" for idx in range(3, 9)]


def row_width(label, value):
    """
    Args:
        label: a row's label.
        value: its value, the row opening something.

    Returns:
        The pixels the row takes of the panel's 128, with 3 between its label and its value.
    """
    return 3 + m.TEXT.width(label) + 3 + m.TEXT.width(value) + 3 + m.TEXT.width(">") + 3


def screens():
    """
    Every screen the charge profile changes, in each word.

    Returns:
        (panels, captions).
    """
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    for word in ("Load", "Charge"):
        p = Panel(); m.title(p, "Charge", back=True)
        m.rows(p, [("Start", "", False), (word, "308 match", True), ("Target", "44.00 gr", True),
                   ("Powder", "AR2208,gr", True)], 0)
        add(p, f'"{word}": the Charge page')

        p = Panel(); m.title(p, "Charge", back=True)
        m.rows(p, [("Start", "", False), (word, LONGEST_NAME, True), ("Target", "41.50 gr", True),
                   ("Powder", "AR2209,gr", True)], 1)
        add(p, f'"{word}": the longest name does not fit')

        p = Panel()
        par.choice_page(p, word, names(word), 0, 0)
        add(p, f'"{word}": its own page')

        p = Panel(); m.title(p, "Change", back=True)
        m.rows(p, [(word, "308 match", True), ("Target", "44.00 gr", True), ("Powder", "AR2208,gr", True)], 0)
        add(p, f'"{word}": Change, between throws')

    # The name as the row itself, with no word before it: the longest name fits whole
    p = Panel(); m.title(p, "Charge", back=True)
    m.rows(p, [("Start", "", False), ("308 match", "", True), ("Target", "44.00 gr", True),
               ("Powder", "AR2208,gr", True)], 0)
    add(p, "The name as the row: the Charge page")

    p = Panel(); m.title(p, "Charge", back=True)
    m.rows(p, [("Start", "", False), (LONGEST_NAME, "", True), ("Target", "41.50 gr", True),
               ("Powder", "AR2209,gr", True)], 1)
    add(p, "The name as the row: the longest name")

    p = Panel(); m.title(p, "Charge", back=True)
    m.rows(p, [("Start", "", False), ("Load 1", "", True), ("Target", "Not set", True),
               ("Powder", "AR2208,gr", True)], 2)
    add(p, "The name as the row: a unit just updated")

    p = Panel(); m.title(p, "Change", back=True)
    m.rows(p, [("308 match", "", True), ("Target", "44.00 gr", True), ("Powder", "AR2208,gr", True)], 0)
    add(p, "The name as the row: Change")

    p = Panel(); m.title(p, "Charging")
    m.figure(p, "31.62", "gr"); m.context(p, "of 44.00, AR2208,gr")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Charging: the line as today")

    p = Panel(); m.title(p, "Charging")
    m.figure(p, "31.62", "gr"); m.context(p, "of 44.00, 308 match")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Charging: the profile named instead")
    return out, caps


def main():
    """
    Draw the sheet, and each screen alone; print the widest rows.

    Returns:
        The exit code.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="the folder written")
    args = parser.parse_args()
    m.use_faces(m.DEFAULT_TEXT, m.DEFAULT_BOLD, m.DEFAULT_FIGURE)
    panels, captions = screens()
    write_sheet(args.out / "charge_profiles_12864.png", panels, captions, m.CAPTION)
    for idx, panel in enumerate(panels):
        write_sheet(args.out / "screens" / f"{idx:02d}.png", [panel], scale=4, gap=8)
    for word in ("Load", "Charge"):
        print(f'"{word}" with the longest name: {row_width(word, LONGEST_NAME)} of {W} px')
    print(f'the charge\'s line with the longest name: {m.TEXT.width(f"of 41.50, {LONGEST_NAME}")} of {W} px')
    print(f"{len(panels)} screens: {args.out / 'charge_profiles_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
