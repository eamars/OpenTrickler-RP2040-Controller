"""No number of the charge without its row (ADR-009, gate N).

Every fixed number of the charge is a macro in `src/charge_rules.h`, and each has its row in
`docs/developments_plans/ADR-009-controller_and_tuning/numbers.md`. This check keeps both true:

1. **Every macro of the header is named in numbers.md,** and every `CHARGE_RULE_...` or `CHARGE_PID_RULE_...` name
   numbers.md gives is in the header.
2. **The charge's files hold no number of their own.** Each number written in them, comments and texts left out, is
   0 or 1, or is in `ALLOWED` below with how often it stands there and why it is not a number of the controller
   (a size, a clock, a conversion, a user's default, the simulated scale's own). One more of any, or a new one, fails:
   it is named in the header with its row in numbers.md, or it is added below with its reason.

Usage, with the repository's venv, from this folder:
    python check_numbers.py             the check; exits 1 on a number without a row
    python check_numbers.py --list      every number found in the charge's files, by file, to make ALLOWED from
"""

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HEADER = REPO / "src" / "charge_rules.h"
NUMBERS = REPO / "docs" / "developments_plans" / "ADR-009-controller_and_tuning" / "numbers.md"
FILES = ("charge_control.c", "charge_control.h", "charge_calibration.c", "charge_calibration.h", "charge_mode.cpp",
         "charge_mode.h", "charge_calibration_job.c", "charge_calibration_job.h")
FREE = ("0", "1")           # nothing and all, a flag, the first of a list
NUMBER = re.compile(r"(?<![A-Za-z_0-9.])(0[xX][0-9a-fA-F]+|\d+\.?\d*(?:[eE][-+]?\d+)?)[fFuUlL]*(?![A-Za-z_0-9.])")

# The numbers a file may hold that are not the controller's: {file: {number: (how often, why)}}
ALLOWED = {
    "charge_control.c": {
        "1000": (2, "milliseconds in a second"),
        "1e-06": (4, "F.3: the back-off measured as it is set, its turns and speed equal within a millionth (a float's tie); F.4: a sum of turns at a bound, the same"),
        "2": (11, "two tubes (arrays of 2); a median's middle; the middle of a parcel's stretch (numbers.md D24); half a digit, the band's edge from the tolerance (A7's conversion)"),
        "32": (2, "the most first errors place_stop takes: a size (B11)"),
    },
    "charge_control.h": {
        "2": (13, "two tubes: arrays of 2"),
        "256": (1, "CHARGE_CONTROL_PARCELS_MAX: a size, with its condition (C23)"),
        "3": (1, "CHARGE_CONTROL_MOVES: a size (B11)"),
        "4": (1, "CHARGE_CONTROL_COARSE_STOPS_MAX: a size (B11)"),
        "8": (1, "CHARGE_CONTROL_FINE_STOPS_MAX: a size (B11)"),
    },
    "charge_calibration.c": {
        "80": (1, "F.4: a console line's room (`ccal`'s stored states)"),
        "1000": (1, "F.4: milliseconds in a second (the learnt source's interval)"),
        # The simulated scale's own calibration, used only with `sim on` (section H)
        "0.01": (1, "H: the simulated scale's coarse share"),
        "0.025": (1, "H: its fine speeds"),
        "0.05": (1, "H: its fine speeds"),
        "0.1": (1, "H: its fine speeds"),
        "0.2": (1, "H: its fine speeds"),
        "0.3": (1, "H: its coarse speeds"),
        "0.5": (1, "H: its fine speeds"),
        "0.7": (1, "H: its moves, in digits"),
        "2.5": (1, "H: its moves, in digits"),
        "2": (15, "F.2: half a digit, the band from the tolerance (A7's conversion); the start check's code 2. H: half an interval, half a digit, two intervals, a coarse speed; a median's middle; twice the first "
                  "errors kept (a merge's room); the console's `ccal` fields by their place"),
        "6": (7, "H: a move of 6 digits; the console's `ccal` fields, six to a line"),
        # The console's commands and the stored record
        "3": (3, "the console's `ccal` fields by their place"),
        "4": (12, "the console's commands, by their words' lengths and their fields' places"),
        "5": (14, "the console's commands, by their words' lengths and their fields' places"),
        "7": (6, "the console's commands, by their words' lengths"),
        "10": (2, "a number read in base 10"),
        "8": (2, "the console's `ccal corrupt`: its word's length"),
        "96": (1, "a console line's length"),
        "160": (1, "a console line's length"),
        "200": (1, "a console line's length"),
    },
    "charge_calibration.h": {
        "16": (1, "CHARGE_CALIBRATION_FIRST_ERRORS_MAX: the first errors a record keeps, a size (B11)"),
    },
    "charge_mode.cpp": {
        "128": (1, "F.4: a line of text's room (the learnt source's start, said)"),
        # The user's settings as a new unit has them (section A): settings, not the controller's numbers
        "5": (2, "A6's default, the coarse stop threshold; the target's five digits; a buffer never used (E20, goes at F.1)"),
        "0.03": (1, "A7's default, the fine stop threshold"),
        "0.02": (2, "A8's and A9's defaults, the zero's margins"),
        "0.25": (1, "A11's default, the back-off's turns"),
        "2": (3, "A12's default, the precharge's speed; an event's bit; the start check's code 2 (a top speed of 0)"),
        "1000": (8, "milliseconds in a second; A12's default, the precharge's time; a target's three decimals"),
        "100": (1, "a target's two decimals"),
        "10": (3, "a target's decimal digits"),
        "4": (1, "the last of a target's five digits"),
        "99999": (1, "A19: the most a target's five digits show"),
        "0.001": (1, "A19: a target on its digits' grid"),
        "50": (1, "E21: a fault's acknowledgement looked for (not control)"),
        "30": (1, "the title's length"),
    },
    "charge_calibration_job.c": {
        "2": (8, "half a digit; a median's middle; the trapezoid's half"),
    },
    "charge_calibration_job.h": {
        "1024": (1, "CHARGE_CAL_JOB_READINGS_MAX: a size"),
        "24": (1, "CHARGE_CAL_JOB_RUNS_MAX: a size (F3's most moves of one size)"),
    },
    "charge_mode.h": {
        "2": (3, "an enum's value; the filler's two unused bytes (A18)"),
        "3": (1, "an enum's value"),
        "4": (1, "an enum's value"),
        "8": (2, "the stored record's revision; a weight's text length"),
    },
}


def bare(source: str) -> str:
    """The source with its comments, texts and characters taken out."""
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.DOTALL)
    source = re.sub(r"//[^\n]*", " ", source)
    source = re.sub(r'"(?:\\.|[^"\\\n])*"', '""', source)
    return re.sub(r"'(?:\\.|[^'\\\n])'", "' '", source)


def canonical(token: str) -> str:
    """A number as one spelling: 2, 2.0 and 2.0f are 2; 1e-4 is 0.0001."""
    if token.lower().startswith("0x"):
        return str(int(token, 16))
    value = float(token)
    return str(int(value)) if value == int(value) else repr(value)


def numbers_in(path: Path) -> Counter:
    found = Counter()
    for line in bare(path.read_text(encoding="utf-8")).splitlines():
        if line.lstrip().startswith("#include"):
            continue
        for match in NUMBER.finditer(line):
            value = canonical(match.group(1))
            if value not in FREE:
                found[value] += 1
    return found


def header_macros() -> list:
    return re.findall(r"^#define\s+([A-Z_0-9]+)\b", HEADER.read_text(encoding="utf-8"), flags=re.MULTILINE)


def check() -> list:
    faults = []
    macros = [name for name in header_macros() if name != "CHARGE_RULES_H_"]
    page = NUMBERS.read_text(encoding="utf-8")
    named = set(re.findall(r"`([A-Z][A-Z_0-9]+)`", page))
    for name in macros:
        if name not in named:
            faults.append(f"{HEADER.name}: {name} has no row in numbers.md")
    for name in sorted(named):
        if name.startswith(("CHARGE_RULE_", "CHARGE_PID_RULE_")) and name not in macros:
            faults.append(f"numbers.md names {name}, which {HEADER.name} does not hold")
    for file_name in FILES:
        found = numbers_in(REPO / "src" / file_name)
        allowed = ALLOWED.get(file_name, {})
        for value, count in sorted(found.items()):
            most = allowed.get(value, (0, ""))[0]
            if count > most:
                faults.append(f"src/{file_name}: the number {value} stands {count} times, {most} allowed: name it in "
                              f"{HEADER.name} with its row in numbers.md, or give its reason in ALLOWED")
        for value, (most, _) in sorted(allowed.items()):
            if found.get(value, 0) < most:
                faults.append(f"src/{file_name}: ALLOWED gives {value} {most} times, {found.get(value, 0)} found: "
                              f"lower it, so the list stays what the file holds")
    return faults


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="every number found in the charge's files, by file")
    args = parser.parse_args()
    if args.list:
        for file_name in FILES:
            found = numbers_in(REPO / "src" / file_name)
            print(file_name + ": " + ", ".join(f"{value} x{count}" for value, count in sorted(found.items())))
        return 0
    faults = check()
    for fault in faults:
        print(fault)
    print("VERDICT " + ("PASS" if not faults else f"FAIL {len(faults)} numbers without a row"))
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())
