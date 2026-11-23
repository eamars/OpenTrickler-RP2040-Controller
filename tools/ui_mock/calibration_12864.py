"""
ADR-009, gate 4: the calibration's screens on the mini 12864, and the other screens the gate asks, as proposed for the
owner (`.agents/skills/embedded-ui-design`; the grid and faces of `menu_12864.py`). Nothing here is built.

    venv\\Scripts\\python tools\\ui_mock\\calibration_12864.py [--out <folder>]

The flow drawn: a powder's page with its calibration; the cup the user declares (sign-off item 17); the cup step (a
safety step, kept); the job while it measures, rehearses and when the cup is full; its verdict, read before it is
kept; the offer of the PID values and its yes; a stop. Then Settings' advanced page (the auto-tuner's switch, this
unit's data, forget what was learnt), the notice after an update (sign-off item 10), and a throw's new ends.
"""
import argparse
import sys
from pathlib import Path

import menu_12864 as m
from panel import Panel, REPO, write_sheet

DEFAULT_OUT = REPO / "docs" / "developments_plans" / "ADR-009-controller_and_tuning" / "mockups"


def screens():
    out, caps = [], []

    def add(panel, caption):
        out.append(panel)
        caps.append(caption)

    # A powder's page: its calibration a row, its state the value
    p = Panel(); m.title(p, "AR2209", back=True)
    m.rows(p, [("Calibrate", "Not yet", True), ("Name", "AR2209", True), ("Coarse", "", True), ("Fine", "", True)], 0,
           total=6)
    add(p, "A powder: never calibrated")

    p = Panel(); m.title(p, "AR2209", back=True)
    m.rows(p, [("Calibrate", "24 Nov", True), ("Name", "AR2209", True), ("Coarse", "", True), ("Fine", "", True)], 0,
           total=6)
    add(p, "A powder: calibrated (the date)")

    # The calibration's page: the cup the user declares, then Start
    p = Panel(); m.title(p, "Calibrate", back=True)
    m.rows(p, [("Cup holds", "200 gr", False), ("Takes", "about 15 min", False), ("Start", "", False)], 0)
    add(p, "Calibrate: the cup it holds, set")

    p = Panel(); m.title(p, "Calibrate")
    m.message(p, "Put the empty cup", "on the scale")
    m.softkeys(p, "Cancel", "Start")
    add(p, "Before it: the cup step (kept)")

    # The job: the title its step, the figure how far, the line what it does
    p = Panel(); m.title(p, "Measuring")
    m.figure(p, "34", "%"); m.context(p, "Fine stops, 3 of 6")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Calibrating: its stops")

    p = Panel(); m.title(p, "Rehearsing")
    m.figure(p, "81", "%"); m.context(p, "Throw 22 of 40")
    m.softkeys(p, "Stop", "", focus="left")
    add(p, "Calibrating: its rehearsals")

    p = Panel(); m.title(p, "Empty the cup")
    m.figure(p, "196.2", "gr"); m.context(p, "It holds 200: paused")
    m.softkeys(p, "Stop", "Go on")
    add(p, "The cup near full: paused until emptied")

    # Its verdict, read before it is kept: Keep at the end
    verdict = [("Expect", "bold"), ("Within 0.03", "84 %", "pair"), ("A throw", "about 11 s", "pair"),
               ("Holding it back", "bold"), "The fine least 0.08 rev/s is above the creep, 0.04.",
               ("Keep", "button", True)]
    p = Panel(); m.title(p, "Calibrated", back=True)
    m.dense(p, verdict, at="top")
    add(p, "Its verdict: the top")

    p = Panel(); m.title(p, "Calibrated", back=True)
    m.dense(p, verdict, at="end")
    add(p, "Its verdict: Keep at the end")

    offer = [("New PID values", "bold"), ("Coarse kp", "0.021", "pair"), ("Fine kp", "0.86", "pair"),
             ("Fine least", "0.08 rev/s", "pair"), "Your values now are kept until you accept.",
             ("Accept", "button", True)]
    p = Panel(); m.title(p, "Offer", back=True)
    m.dense(p, offer, at="top")
    add(p, "The offer: the profile's values")

    p = Panel(); m.title(p, "Calibrate")
    m.message(p, "Stopped: nothing", "was stored")
    m.softkeys(p, "", "OK")
    add(p, "Stopped")

    # Settings, advanced
    p = Panel(); m.title(p, "Advanced", back=True)
    m.rows(p, [("Auto-tune", "On", True), ("This unit's data", "", True), ("Forget learnt", "", True)], 0)
    add(p, "Settings: Advanced")

    p = Panel(); m.title(p, "This unit", back=True)
    m.dense(p, [("Digit", "0.02 gr", "pair"), ("Interval", "0.048 s", "pair"), ("Lag", "0.9 s", "pair"),
                ("Rest", "1.7 s", "pair"), ("Back up", "button", True)], at="top")
    add(p, "This unit's data (read; changed on the web)")

    p = Panel(); m.title(p, "Forget learnt")
    m.message(p, "AR2209: back to its", "calibration's stop?")
    m.softkeys(p, "Cancel", "Forget", focus="left")
    add(p, "Forget what was learnt, a powder")

    # After an update (sign-off item 10)
    p = Panel(); m.title(p, "New controller")
    m.message(p, "Old tuning may not hold.", "Calibrate each powder.")
    m.softkeys(p, "Later", "Calibrate")
    add(p, "After the update: the notice")

    # A throw's new ends (not a verdict)
    for status, line in (("Cup lifted", "Stopped: both tubes"), ("No powder", "Check the hopper"),
                         ("Limit", "Took too long")):
        p = Panel(); m.title(p, status)
        m.figure(p, "12.40", "gr"); m.context(p, line, bold=True)
        m.softkeys(p, "Stop", "Change")
        add(p, f"A throw ended: {status.lower()}")
    return out, caps


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    m.use_faces(m.DEFAULT_TEXT, m.DEFAULT_BOLD, m.DEFAULT_FIGURE)
    panels, captions = screens()
    write_sheet(args.out / "calibration_12864.png", panels, captions, m.CAPTION)
    print(f"{len(panels)} screens: {args.out / 'calibration_12864.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
