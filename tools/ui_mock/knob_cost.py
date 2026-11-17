"""
What a change costs a person at the knob (ADR-010's survey of what the screen offers): each detent turned and each
click is one action, counted from the main menu at rest, so a setting too costly to change is seen before it is
built. Prints the table menu_design.md carries.

    venv\\Scripts\\python tools\\ui_mock\\knob_cost.py

The editing methods, all in place in their row (no editor screen):
  toggle   a click flips it.
  action   a click does it.
  choice   a click, the turn through the choices in place (wrapping, so the shorter way), a click.
  step     a click, one step a detent, a click; the step is the field's screen step.
  digits   the target only: the turn selects a digit, a click sets it (the turn then sets its value, wrapping), a
           click ends; then the turn to Set and a click.
  text     a character at a time: a click on "+", the turn through the character set (wrapping), a click.
"""
import math
import string

CHARSET = string.ascii_lowercase + string.ascii_uppercase + string.digits + " -_.:/@#!"


def reach(path):
    """Actions to reach a row: at each page the turns to the row, and a click to open it but at the last."""
    return sum(path) + len(path) - 1


def edit_choice(count, frm, to):
    d = abs(to - frm)
    return 2 + min(d, count - d)


def edit_step(frm, to, step):
    return 2 + math.ceil(round(abs(to - frm) / step, 6))


def edit_digits(frm, to):
    """The target's digits: frm and to as digit strings of the same length."""
    actions, at = 0, 0
    for idx, (a, b) in enumerate(zip(frm, to)):
        if a == b:
            continue
        d = abs(int(a) - int(b))
        actions += abs(idx - at) + 1 + min(d, 10 - d) + 1
        at = idx
    set_at = len(frm) + 1          # past the digits: Cancel, then Set
    return actions + (set_at - at) + 1


def edit_text(text):
    actions, at = 0, 0
    for char in text:
        to = CHARSET.index(char)
        d = abs(to - at)
        actions += 1 + min(d, len(CHARSET) - d) + 1
        at = 0                      # each new character starts at 'a'
    return actions + 2              # the turn to Set, a click


# (what, need, method, the change, path to its row, edit cost)
# need: S a session at the bench; W making the unit work without the web; T tuning or building, set once with the
# web's data (or by ADR-009's tuning). The paths follow the mock's tree (parity_12864.py), each page's rows in order:
MAIN = {"Charge": 0, "Cleanup": 1, "Wi-Fi": 2, "Settings": 3}
CHARGE = {"Start": 0, "Target": 1, "Powder": 2, "Powder details": 3}
CLEANUP = {"Speed": 0, "Gate": 1, "Stop": 2, "Zero": 3}         # behind the cup's check, its Start a click
SETTINGS = {"Scale": 0, "Charge": 1, "Display": 2, "Servo gate": 3, "Motors": 4, "Firmware": 5, "Restart": 6,
            "Factory reset": 7}
SCALE = {"Zero": 0, "Make": 1, "Baud rate": 2, "Frame format": 3, "Decimals": 4}      # its Reading is read only
CHARGE_SETTINGS = {"Fine stop": 0, "Coarse stop": 1}
DISPLAY = {"Invert knob": 0, "Invert screen": 1, "Rotation": 2, "Backlight": 3, "Charge view": 4}
GATE = {"Servo gate": 0, "Shutters": 1, "Shutter 0 closed": 2}
MOTORS = {"Coarse direction": 0, "Fine direction": 1}
FIRMWARE = {"Channel": 0, "Check for update": 1}                # its facts are read only
WIFI_OFF = 2                                                    # Wi-Fi's notes: the turns to its button at the end
S = MAIN["Settings"]


def settings(page, row=None):
    return [S, SETTINGS[page]] + ([] if row is None else [row])


ROWS = [
    ("Target, a small change", "S", "digits", "30.00 to 30.10", [MAIN["Charge"], CHARGE["Target"]],
     edit_digits("03000", "03010")),
    ("Target, a new load", "S", "digits", "24.35 to 42.00", [MAIN["Charge"], CHARGE["Target"]],
     edit_digits("02435", "04200")),
    ("Powder (8 profiles)", "S", "choice", "1 to 5", [MAIN["Charge"], CHARGE["Powder"]], edit_choice(8, 1, 5)),
    ("Powder details, to read", "S", "page", "", [MAIN["Charge"], CHARGE["Powder details"]], 1),
    ("Start a charge", "S", "action", "", [MAIN["Charge"], CHARGE["Start"]], 1 + 1),
    ("Zero, in a charge", "S", "action", "", [], 1),
    ("Stop, in a charge or cleanup (the reset button)", "S", "action", "", [], 1),
    ("Cleanup's speed", "S", "step 1 rev/s", "0 to 3 rev/s", [MAIN["Cleanup"], 0, CLEANUP["Speed"]],
     edit_step(0, 3, 1)),
    ("Cleanup's gate, open or closed", "S", "toggle", "", [MAIN["Cleanup"], 0, CLEANUP["Gate"]], 1),
    ("Cleanup's zero", "S", "action", "", [MAIN["Cleanup"], 0, CLEANUP["Zero"]], 1),
    ("Cleanup's Stop, by the knob", "S", "action", "", [MAIN["Cleanup"], 0, CLEANUP["Stop"]], 1),
    ("Scale's make (9)", "W", "choice", "A&D to Sartorius", settings("Scale", SCALE["Make"]), edit_choice(9, 0, 7)),
    ("Scale's baud rate (3)", "W", "choice", "19200 to 9600", settings("Scale", SCALE["Baud rate"]),
     edit_choice(3, 2, 1)),
    ("Scale's frame format (2)", "W", "choice", "8N1 to 7N1", settings("Scale", SCALE["Frame format"]),
     edit_choice(2, 0, 1)),
    ("Decimals (2)", "W", "choice", "2 to 3", settings("Scale", SCALE["Decimals"]), edit_choice(2, 0, 1)),
    ("Fine stop threshold", "S", "step 0.01 gr", "0.03 to 0.05", settings("Charge", CHARGE_SETTINGS["Fine stop"]),
     edit_step(0.03, 0.05, 0.01)),
    ("Coarse stop threshold", "S", "step 0.1 gr", "5.0 to 3.0", settings("Charge", CHARGE_SETTINGS["Coarse stop"]),
     edit_step(5.0, 3.0, 0.1)),
    ("Invert knob", "W", "toggle", "", settings("Display", DISPLAY["Invert knob"]), 1),
    ("Invert screen", "W", "toggle", "", settings("Display", DISPLAY["Invert screen"]), 1),
    ("Rotation (2)", "W", "choice", "", settings("Display", DISPLAY["Rotation"]), edit_choice(2, 0, 1)),
    ("Backlight colour (8 named)", "W", "choice", "white to green", settings("Display", DISPLAY["Backlight"]),
     edit_choice(8, 0, 4)),
    ("Charge view (2): weight or chart", "S", "choice", "", settings("Display", DISPLAY["Charge view"]),
     edit_choice(2, 0, 1)),
    ("Servo gate on", "W", "toggle", "", settings("Servo gate", GATE["Servo gate"]), 1),
    ("Servo gate open, close", "W", "toggle", "", settings("Servo gate", GATE["Shutters"]), 1),
    ("A shutter's end (the servo moves live)", "W", "step 0.001", "0.050 to 0.055",
     settings("Servo gate", GATE["Shutter 0 closed"]), edit_step(0.050, 0.055, 0.001)),
    ("A motor's direction", "W", "toggle", "", settings("Motors", MOTORS["Fine direction"]), 1),
    ("Update channel (2)", "W", "choice", "", settings("Firmware", FIRMWARE["Channel"]), edit_choice(2, 0, 1)),
    ("Restart", "W", "action", "", [S, SETTINGS["Restart"]], 1),
    ("Factory reset (opens on Cancel)", "W", "action", "", [S, SETTINGS["Factory reset"]], 1 + 1 + 1),
    ("Wi-Fi on or off", "W", "action", "", [MAIN["Wi-Fi"], WIFI_OFF], 1),
    # Not on the screen: costed where they would sit, Settings > a page of their own > the field
    ("Back-off turns", "T", "step 0.125", "0.25 to 0.5", [S, 8, 2], edit_step(0.25, 0.5, 0.125)),
    ("Precharge time", "T", "step 50 ms", "1000 to 1500 ms", [S, 8, 4], edit_step(1000, 1500, 50)),
    ("A motor's current", "T", "step 50 mA", "400 to 600 mA", settings("Motors", 2), edit_step(400, 600, 50)),
    ("A profile's Kp (digits)", "T", "digits", "0.0250 to 0.0300", [S, 8, 1, 0], edit_digits("00250", "00300")),
    ("A profile's name", "T", "text", "AR2209", [S, 8, 1], edit_text("AR2209")),
    ("Wi-Fi network, picked from a scan", "W", "choice", "the 2nd found", [S, 9, 1], 1 + 1 + 1),
    ("Wi-Fi key, 12 characters", "W", "text", "Trickle-2024", [S, 9, 2], edit_text("Trickle-2024")),
    ("Update source, a URL", "T", "text", "http://192.168.2.13:8080/a/b", settings("Firmware", 2),
     edit_text("http://192.168.2.13:8080/a/b")),
]


def main():
    print("| What | Need | Method | The change | To reach | To change | In all |")
    print("|---|---|---|---|---|---|---|")
    for what, need, method, change, path, cost in ROWS:
        to_reach = reach(path) if path else 0
        print(f"| {what} | {need} | {method} | {change} | {to_reach} | {cost} | {to_reach + cost} |")


if __name__ == "__main__":
    main()
