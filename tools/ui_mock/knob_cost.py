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
# web's data (or by ADR-009's tuning)
MAIN_SETTINGS = 3
ROWS = [
    ("Target, a small change", "S", "digits", "30.00 to 30.10", [0, 1], edit_digits("03000", "03010")),
    ("Target, a new load", "S", "digits", "24.35 to 42.00", [0, 1], edit_digits("02435", "04200")),
    ("Powder (8 profiles)", "S", "choice", "1 to 5", [0, 2], edit_choice(8, 1, 5)),
    ("Start a charge", "S", "action", "", [0, 0], 1 + 1),
    ("Zero, in a charge", "S", "action", "", [], 1),
    ("Stop, in a charge or cleanup", "S", "action", "", [], 1),
    ("Cleanup's speed", "S", "step 1 rev/s", "0 to 3 rev/s", [1, 0, 0], edit_step(0, 3, 1)),
    ("Cleanup's gate, open or closed", "S", "toggle", "", [1, 0, 1], 1),
    ("Cleanup's zero", "S", "action", "", [1, 0, 2], 1),
    ("Scale's make (9)", "W", "choice", "A&D to Sartorius", [MAIN_SETTINGS, 0, 1], edit_choice(9, 0, 7)),
    ("Scale's baud rate (3)", "W", "choice", "19200 to 9600", [MAIN_SETTINGS, 0, 2], edit_choice(3, 2, 1)),
    ("Scale's frame format (2)", "W", "choice", "8N1 to 7N1", [MAIN_SETTINGS, 0, 3], edit_choice(2, 0, 1)),
    ("Decimals (2)", "W", "choice", "2 to 3", [MAIN_SETTINGS, 0, 4], edit_choice(2, 0, 1)),
    ("Invert knob", "W", "toggle", "", [MAIN_SETTINGS, 1, 0], 1),
    ("Invert screen", "W", "toggle", "", [MAIN_SETTINGS, 1, 1], 1),
    ("Rotation (2)", "W", "choice", "", [MAIN_SETTINGS, 1, 2], edit_choice(2, 0, 1)),
    ("Backlight colour (8 named)", "W", "choice", "white to green", [MAIN_SETTINGS, 1, 3], edit_choice(8, 0, 3)),
    ("Servo gate on", "W", "toggle", "", [MAIN_SETTINGS, 2, 0], 1),
    ("Servo gate open, close", "W", "action", "", [MAIN_SETTINGS, 2, 1], 1),
    ("A shutter's end (the servo moves live)", "W", "step 0.001", "0.050 to 0.055", [MAIN_SETTINGS, 2, 3],
     edit_step(0.050, 0.055, 0.001)),
    ("A motor's direction", "W", "toggle", "", [MAIN_SETTINGS, 3, 0], 1),
    ("Update channel (2)", "W", "choice", "", [MAIN_SETTINGS, 4, 0], edit_choice(2, 0, 1)),
    ("Restart", "W", "action", "", [MAIN_SETTINGS, 5], 1),
    ("Factory reset (opens on Cancel)", "W", "action", "", [MAIN_SETTINGS, 6], 1 + 1 + 1),
    ("Wi-Fi on or off", "W", "toggle", "", [2, 0], 1),
    # Tuning and building: costed where they would sit, Settings > their group > the field
    ("Fine stop threshold", "T", "step 0.01 gr", "0.03 to 0.05", [MAIN_SETTINGS, 7, 0, 1],
     edit_step(0.03, 0.05, 0.01)),
    ("Coarse stop threshold", "T", "step 0.1 gr", "5.0 to 3.0", [MAIN_SETTINGS, 7, 0, 0], edit_step(5.0, 3.0, 0.1)),
    ("Back-off turns", "T", "step 0.125", "0.25 to 0.5", [MAIN_SETTINGS, 7, 1, 2], edit_step(0.25, 0.5, 0.125)),
    ("Precharge time", "T", "step 50 ms", "1000 to 1500 ms", [MAIN_SETTINGS, 7, 2, 1], edit_step(1000, 1500, 50)),
    ("A motor's current", "T", "step 50 mA", "400 to 600 mA", [MAIN_SETTINGS, 3, 2], edit_step(400, 600, 50)),
    ("A profile's Kp (digits)", "T", "digits", "0.0250 to 0.0300", [MAIN_SETTINGS, 8, 1, 1, 0],
     edit_digits("00250", "00300")),
    ("A profile's name", "T", "text", "AR2209", [MAIN_SETTINGS, 8, 1, 0], edit_text("AR2209")),
    ("Wi-Fi network, picked from a scan", "W", "choice", "the 2nd found", [MAIN_SETTINGS, 9, 1], 1 + 1 + 1),
    ("Wi-Fi key, 12 characters", "W", "text", "Trickle-2024", [MAIN_SETTINGS, 9, 2], edit_text("Trickle-2024")),
    ("Update source, a URL", "T", "text", "http://192.168.2.13:8080/a/b", [MAIN_SETTINGS, 4, 1],
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
