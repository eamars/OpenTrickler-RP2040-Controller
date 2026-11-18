"""
The set-up's screens on the mini 12864, as proposed (ADR-008, "Commissioning"; `.agents/skills/embedded-ui-design`
is the guideline), drawn in the menu's grid and faces (menu_12864.py) onto one sheet, for the owner to settle before
gate 4 builds them.

    venv\\Scripts\\python tools\\ui_mock\\setup_12864.py [--out <folder>]

The hotspot's key is made from the chip's unique id (the README's proposal): eight characters from an alphabet with no
i, l, o, 0 or 1, which helvR08 draws alike or nearly. The code a phone's camera reads joins the hotspot (the phones'
own Wi-Fi format, `WIFI:T:WPA;S:<name>;P:<key>;;`): QR version 3 at level L, 29 modules a side, 2 px a module, so
58 of the panel's 64 rows, its quiet zone 3 px. Drawn as a phone reads a code, dark modules on a light ground: on
the negative glass the ground is lit and the modules are not.
"""
import argparse
import sys
from pathlib import Path

import segno

import menu_12864 as menu
from panel import H, Panel, REPO, W, write_sheet

DEFAULT_OUT = REPO / "docs" / "developments_plans" / "ADR-008-web_ui" / "mockups"
HOTSPOT = "opentrickler-9CE0"
KEY = "k7m2x9pq"        # an example: the unit's own is made from its chip
ADDRESS = "192.168.4.1"
MODULE = 2              # pixels a module
CODE_SIDE = 64          # the code's lit square, its quiet zone inside it
# The title's hotspot mark (scripts/bdf2lvgl.py, ICONS U+E001), shown while the unit is its own access point
HOTSPOT_MARK = [" #         # ", "#  #     #  #", "# #  ###  # #", "# #  ###  # #", "# #  ###  # #",
                "#  #     #  #", " #         # "]
COLUMN_X = CODE_SIDE + 4    # the text beside the code


def title(panel, text, back=False):
    """The menu's title band, its mark the hotspot's: the unit is its own access point while it is set up."""
    panel.text(menu.BOLD, 1, menu.TITLE_BASE, ("< " if back else "") + text)
    panel.icon(W - 14, 2, HOTSPOT_MARK)
    panel.hline(0, menu.RULE_Y, W)


def join_code(panel):
    """
    The code that joins the hotspot, at the panel's left: its quiet zone lit, its dark modules not.

    Args:
        panel: the Panel.

    Returns:
        The code's version and error level, as a phone's reader needs them.
    """
    code = segno.make(f"WIFI:T:WPA;S:{HOTSPOT};P:{KEY};;", error="l", micro=False, boost_error=False)
    side = code.symbol_size(border=0)[0] * MODULE
    quiet = (CODE_SIDE - side) // 2
    panel.rect(0, 0, CODE_SIDE, H)
    for row, bits in enumerate(code.matrix):
        for col, dark in enumerate(bits):
            if dark:
                panel.rect(quiet + col * MODULE, quiet + row * MODULE, MODULE, MODULE, lit=False)
    return code.version, code.error


def column(panel, lines):
    """Lines of text beside the code, one each dense line, together centred in the room above the softkey."""
    height = len(lines) * menu.DENSE_H
    top = (menu.SOFTKEY_TOP - height) // 2
    for idx, line in enumerate(lines):
        panel.text(menu.TEXT, COLUMN_X, top + menu.DENSE_BASE + idx * menu.DENSE_H, line)


def back_key(panel):
    """
    The screen's one softkey, Back, under the text beside the code and focused: the code takes the left, where Back
    stands elsewhere, so here it stands at the right, the only thing a click does.
    """
    width = menu.TEXT.width("Back") + 6
    x = W - width
    panel.rect(x, menu.SOFTKEY_TOP, width, H - menu.SOFTKEY_TOP)
    panel.text(menu.TEXT, x + 3, menu.SOFTKEY_BASE, "Back", lit=False)


def screens():
    """
    The set-up's screens.

    Returns:
        (panels, captions).
    """
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    p = Panel(); title(p, "OpenTrickler")
    menu.rows(p, [("Set up this unit", "", True), ("Charge", "", True), ("Purge", "", False), ("Wi-Fi", "", True)],
              0, total=5)
    add(p, "Main menu, never set up: an offer, no pop-up")

    p = Panel()
    version, level = join_code(p)
    column(p, ["Scan this", "with your", "phone's", "camera."])
    back_key(p)
    add(p, f"Set up: the code joins the hotspot (QR {version}-{level})")

    p = Panel(); title(p, "Set up")
    p.text(menu.TEXT, 0, 26, "A phone joined.", align="center")
    p.text(menu.TEXT, 0, 37, "Go on with it there,", align="center")
    p.text(menu.TEXT, 0, 48, f"or open {ADDRESS}", align="center")
    menu.softkeys(p, "Back", "Show the code", focus="left")
    add(p, "Set up: a phone joined the hotspot")

    wifi = [("Hotspot", HOTSPOT, "pair"), ("Key", KEY, "pair"), ("IP", ADDRESS, "pair"),
            ("Join by camera", "button", False)]
    p = Panel(); title(p, "Wi-Fi", back=True)
    menu.dense(p, wifi, at="top")
    add(p, "Wi-Fi on its own hotspot: the top")

    wifi[-1] = ("Join by camera", "button", True)
    p = Panel(); title(p, "Wi-Fi", back=True)
    menu.dense(p, wifi, at="end")
    add(p, "Wi-Fi on its own hotspot: its action")
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
    menu.use_faces(menu.DEFAULT_TEXT, menu.DEFAULT_BOLD, menu.DEFAULT_FIGURE)
    panels, captions = screens()
    write_sheet(args.out / "setup_12864.png", panels, captions, menu.CAPTION, columns=3)
    for idx, panel in enumerate(panels):
        write_sheet(args.out / "screens" / f"setup_{idx:02d}.png", [panel], scale=4, gap=8)
    print(f"{len(panels)} screens: {args.out / 'setup_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
