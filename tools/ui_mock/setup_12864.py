"""
The set-up's screens on the mini 12864, a stage at a time, as designed again (ADR-008, "The set-up, designed again"; `.agents/skills/embedded-ui-design`
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
NETWORK = "Smith home"          # the stand-in's example network, never the owner's
LONG_NETWORK = "Garage extender 2.4G-Plus"
JOINED_ADDRESS = "192.168.1.57"
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


def lines(panel, *texts):
    """
    A dialog's message centred: one or two lines on 29 and 40, three on 26, 37 and 48 (the view's grid). A line wider
    than the panel is said, as the screen's skill asks every text to have a width.
    """
    bases = [29, 40] if len(texts) <= 2 else [26, 37, 48]
    for base, text in zip(bases, texts):
        if menu.TEXT.width(text) > W - 2:
            print(f"too wide ({menu.TEXT.width(text)} px): {text!r}")
        panel.text(menu.TEXT, 0, base, text, align="center")


def screens():
    """
    The set-up's screens, a stage at a time (README, "The set-up, designed again"): each stage as the 12864 shows it
    while the phone shows its own view of the same stage.

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
    add(p, "Never set up: an offer, no pop-up")

    p = Panel()
    version, level = join_code(p)
    column(p, ["Scan this", "with your", "phone's", "camera."])
    back_key(p)
    add(p, f"The code (QR {version}-{level})")

    p = Panel(); title(p, "Set up: Wi-Fi")
    lines(p, "A phone joined.", "Choose your Wi-Fi", "on your phone.")
    menu.softkeys(p, "Back", "Show the code", focus="left")
    add(p, "A phone joined; choosing a network")

    p = Panel(); title(p, "Set up: Wi-Fi")
    lines(p, "Type the password of", NETWORK, "on your phone.")
    menu.softkeys(p, "Back", "Show the code", focus="left")
    add(p, "Its password, typed on the phone")

    p = Panel(); title(p, "Set up: Wi-Fi")
    lines(p, "Joining", NETWORK, "8 s of 30")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "Joining: its seconds count up, to 30")

    p = Panel(); title(p, "Set up: Wi-Fi")
    lines(p, "Joined", NETWORK, "Continue on your phone.")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "Joined: the phone goes on to the scale")

    p = Panel(); title(p, "Set up: Wi-Fi")
    lines(p, "Wrong password for", NETWORK, "Retype it on your phone.")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "Wrong password")

    p = Panel(); title(p, "Set up: Wi-Fi")
    lines(p, "No answer from", NETWORK, "Move it nearer the router.")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "No answer (out of reach, or 5 GHz only)")

    p = Panel(); title(p, "Set up: Scale")
    menu.figure(p, "12.34", "gr")
    menu.context(p, "The scale sends its weight")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "The scale: its weight, live")

    p = Panel(); title(p, "Set up: Scale")
    menu.figure(p, "--", "")
    menu.context(p, "Nothing from the scale")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "The scale: nothing heard in 3 s")

    p = Panel(); title(p, "Set up: Motors")
    lines(p, "Coarse driver: found", "Fine driver: found", "Continue on your phone.")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "The motors")

    p = Panel(); title(p, "Set up: Ready")
    lines(p, "Ready.", "Finish on your phone.")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "Ready")

    p = Panel(); menu.title(p, "Set up: Done")
    lines(p, "Open on " + NETWORK + ":", HOTSPOT + ".local", "or " + JOINED_ADDRESS)
    menu.softkeys(p, "", "OK", focus="right")
    add(p, "Done, joined: the hotspot stopped")

    p = Panel(); title(p, "Set up: Done")
    lines(p, "On its own hotspot.", "Open " + ADDRESS, "from a phone on it.")
    menu.softkeys(p, "", "OK", focus="right")
    add(p, "Done, Wi-Fi skipped: the hotspot stays")

    p = Panel(); title(p, "Set up: Wi-Fi")
    lines(p, "Wrong password for", LONG_NETWORK, "Retype it on your phone.")
    menu.softkeys(p, "Back", "", focus="left")
    add(p, "A long network name: its own line")

    wifi = [("Hotspot", HOTSPOT, "pair"), ("Password", KEY, "pair"), ("IP", ADDRESS, "pair"),
            ("Join by camera", "button", False)]
    p = Panel(); title(p, "Wi-Fi", back=True)
    menu.dense(p, wifi, at="top")
    add(p, "Wi-Fi on its own hotspot: Password")
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
