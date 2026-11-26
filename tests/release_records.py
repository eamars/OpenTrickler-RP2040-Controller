"""The stored records and their enums against v1.21, the release a unit updates from (ADR-009, 2026-11-26).

The owner: "NEVER INSERT, always append for EEPROM record", and compatibility "only back track to v1.21". The shared
migration (src/services/storage/common.c, load_config) reads an earlier record as the start of today's, so it is right only where every
change since v1.21 was an append; and a stored enum's value read from v1.21 must still mean what v1.21 meant. Checked
here, from the sources (v1.21's by its tag, today's on disk):
- each stored struct's fields at v1.21 (type and name, in order) are the first fields of today's; a record whose history
  had an insertion says how it reads v1.21's layout instead (PAST_LAYOUTS);
- each stored enum's value names at v1.21 are today's names for the same values, but for KNOWN_OPEN, each with the piece
  that closes it; an entry of KNOWN_OPEN that no longer clashes fails too, so it is taken out;
- every `eeprom_*_t` struct in src's headers is one of RECORDS, so a new record is not missed.
Ends with VERDICT PASS or FAIL. CI runs it on every build (it fetches the tags).

    venv\\Scripts\\python tests\\release_records.py
"""

import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RELEASE = "v1.21"

# The stored structs, by their header: the records and the structs inside them
RECORDS = {
    "eeprom_charge_mode_data_t": "src/services/charge/include/charge_mode.h",
    "eeprom_scale_data_t": "src/services/scale/include/scale.h",
    "scale_lag_point_t": "src/services/scale/include/scale.h",
    "eeprom_motor_data_t": "src/services/motors/include/motors.h",
    "motor_persistent_config_t": "src/services/motors/include/motors.h",
    "eeprom_neopixel_led_metadata_t": "src/services/leds/include/neopixel_led.h",
    "mini_12864_module_config_t": "src/ui/mini_12864/include/mini_12864_module.h",
    "eeprom_profile_data_t": "src/services/profile/include/profile.h",
    "profile_t": "src/services/profile/include/profile.h",
    "eeprom_servo_gate_config_t": "src/services/servo_gate/include/servo_gate.h",
    "eeprom_wireless_metadata_t": "src/services/wireless/include/wireless.h",
    "eeprom_update_config_t": "src/services/update_fetch/include/update_poll.h",
}
# Records whose history had an insertion before the rule, and where they read v1.21's layout field by field
PAST_LAYOUTS = {
    "eeprom_charge_mode_data_t": "src/services/charge/charge_mode_layout.c (v1.19 and v2.0.0 inserted fields; ADR-009's review, B1)",
}
# Stored enum values whose meaning changed since v1.21, each with what closes it. Empty since main's v1.21 merge
# (#137, 2026-11-26) gave the scale driver's 9 back to FA-E/EN and moved the simulated scale to 100 (ADR-009's review, B2)
KNOWN_OPEN = {}
HEADERS = sorted({path for path in RECORDS.values()} | {"src/services/storage/include/common.h"})


def source(path: str, tag: str = None) -> str:
    """A file's text at a tag, or on disk; "" when it is not there."""
    if tag is None:
        file = REPO / path
        return file.read_text(encoding="utf-8", errors="replace") if file.exists() else ""
    # A release from before ADR-006 has every file in one flat src/
    run = subprocess.run(["git", "-C", str(REPO), "show", f"{tag}:src/{Path(path).name}"], capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    return run.stdout if run.returncode == 0 else ""


def without_comments(text: str) -> str:
    """The text with its comments and its preprocessor lines taken out (a test build's field stays a field)."""
    text = re.sub(r"/\*.*?\*/", "", re.sub(r"//[^\n]*", "", text), flags=re.S)
    return re.sub(r"^[ \t]*#.*$", "", text, flags=re.M)


def struct_fields(text: str, name: str):
    """The declarations of `typedef struct {...} name;`, each "type name", in order; None when there is none."""
    found = re.search(r"typedef\s+struct\s*\w*\s*\{([^{}]*)\}\s*" + re.escape(name) + r"\s*;", without_comments(text),
                      re.S)
    if found is None:
        return None
    return [" ".join(decl.split()) for decl in found.group(1).split(";") if decl.strip()]


def enums(text: str) -> dict:
    """Each `typedef enum {...} name;` as {name: {value: member}}."""
    out = {}
    for body, name in re.findall(r"typedef\s+enum\s*\w*\s*\{([^{}]*)\}\s*(\w+)\s*;", without_comments(text), re.S):
        value, members = -1, {}
        for item in body.split(","):
            item = item.strip()
            parts = re.match(r"(\w+)\s*(?:=\s*(.+))?$", item)
            if not parts:
                continue
            try:
                value = int(parts.group(2).strip(), 0) if parts.group(2) else value + 1
            except ValueError:
                value = None
            if value is not None:
                members[value] = parts.group(1)
        out[name] = members
    return out


def main() -> int:
    """
    Check the records and their enums against v1.21.

    Returns:
        0 when every check passes, 1 otherwise.
    """
    failures = []
    stored_types = set()
    for name, path in RECORDS.items():
        now = struct_fields(source(path), name)
        if now is None:
            failures.append(f"{name}: not found in {path}")
            continue
        stored_types.update(decl.split()[0] for decl in now)
        then = struct_fields(source(path, RELEASE), name)
        if then is None:
            print(f"{name:32s} new since {RELEASE}")
        elif now[:len(then)] == then:
            print(f"{name:32s} {RELEASE}'s {len(then)} fields first, {len(now) - len(then)} appended")
        elif name in PAST_LAYOUTS:
            print(f"{name:32s} not append-only since {RELEASE}: read field by field, {PAST_LAYOUTS[name]}")
        else:
            at = next(i for i, (a, b) in enumerate(zip(then + [None], now + [None])) if a != b)
            failures.append(f"{name}: {RELEASE}'s field {at} ({then[at] if at < len(then) else '-'}) is "
                            f"{now[at] if at < len(now) else 'gone'} now: a field inserted, moved or changed")
    # Every eeprom_*_t struct in src's headers is a record listed here
    for header in sorted((REPO / "src").glob("*/*/**/*.h")):
        for name in re.findall(r"\}\s*(eeprom_\w+_t)\s*;", without_comments(header.read_text(encoding="utf-8",
                                                                                            errors="replace"))):
            if name not in RECORDS:
                failures.append(f"{name} ({header.name}): a stored struct not in RECORDS")
    # The stored enums: each v1.21 value keeps its meaning
    then_enums, now_enums = {}, {}
    for header in HEADERS:
        then_enums.update(enums(source(header, RELEASE)))
        now_enums.update(enums(source(header)))
    clashes = set()
    for type_name in sorted(stored_types & set(then_enums) & set(now_enums)):
        for value, member in then_enums[type_name].items():
            now_member = now_enums[type_name].get(value)
            if now_member != member:
                clashes.add((type_name, value))
                said = f"{type_name} {value}: {RELEASE}'s {member}, now {now_member or 'nothing'}"
                if (type_name, value) in KNOWN_OPEN:
                    print(f"{said}: known, open: {KNOWN_OPEN[(type_name, value)]}")
                else:
                    failures.append(f"{said}: a stored value whose meaning changed")
        if not any(t == type_name for t, _ in clashes):
            print(f"{type_name:32s} every {RELEASE} value means what it meant")
    for key, why in KNOWN_OPEN.items():
        if key not in clashes:
            failures.append(f"{key[0]} {key[1]} no longer clashes: take it out of KNOWN_OPEN ({why})")
    for failure in failures:
        print("FAIL " + failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)} found")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
