"""The stored records and their enums against v1.21, the release a unit updates from (ADR-009, 2026-11-26).

The owner: "NEVER INSERT, always append for EEPROM record", and compatibility "only back track to v1.21". The shared
migration (src/services/storage/common.c, load_config) reads an earlier record as the start of today's, so it is right only where every
change since v1.21 was an append; and a stored enum's value read from v1.21 must still mean what v1.21 meant. Checked
here, from the sources (v1.21's by its tag, today's on disk):
- each stored struct's fields at v1.21 (type and name, in order) are the first fields of today's; a record whose history
  had an insertion says how it reads v1.21's layout instead (PAST_LAYOUTS);
- each stored enum's value names at v1.21 are today's names for the same values, but for KNOWN_OPEN, each with the piece
  that closes it; an entry of KNOWN_OPEN that no longer clashes fails too, so it is taken out;
- every `eeprom_*_t` struct in src's headers is one of RECORDS, so a new record is not missed;
- every stored field is of a type laid out the same by every compiler the firmware is built with (ADR-006, gate 8):
  FIXED_TYPES, a stored struct of RECORDS, or one of LAID_OUT. An enum is not: one byte with the Pico's compiler,
  four with ESP-IDF's. A stored enum is held in an integer whose comment names it first
  (`uint8_t scale_driver;  // scale_driver_t`), and its values are checked by that name, each fitting the integer.
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
    "neopixel_led_colours_t": "src/services/leds/include/neopixel_led.h",
}
# The types a stored field may have besides the structs of RECORDS, with the values each holds
FIXED_TYPES = {"uint8_t": (0, 0xFF), "uint16_t": (0, 0xFFFF), "uint32_t": (0, 0xFFFFFFFF), "int8_t": (-0x80, 0x7F),
               "int16_t": (-0x8000, 0x7FFF), "int32_t": (-0x80000000, 0x7FFFFFFF), "float": None, "bool": None,
               "char": None}
# A stored type that is not a typedef struct, and what lays it out the same everywhere
LAID_OUT = {"rgbw_u32_t": "a union of a uint32_t and its four bytes, packed (neopixel_led.h)"}
# v1.21's enum fields as they lay: each one byte, as the Pico's compiler makes an enum whose values fit (gdb's ptype /o
# on both images before gate 8; v1.21's charge record has decimal_places at byte 24, one byte, in host_test.py)
V121_ENUM_HELD = "uint8_t"
# A stored enum renamed since v1.21: {today's name: v1.21's}
ENUM_RENAMED = {
    "wireless_auth_t": "cyw43_auth_t",      # ours, with cyw43's name until ADR-006's gate 8
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


def held_enums(text: str, name: str, known: set) -> dict:
    """The fields of `typedef struct {...} name;` whose comment names an enum of `known` first: {field: enum}."""
    found = re.search(r"typedef\s+struct\s*\w*\s*\{([^{}]*)\}\s*" + re.escape(name) + r"\s*;", text, re.S)
    out = {}
    for field, enum in re.findall(r"^\s*\w+\s+(\w+)\s*(?:\[[^\]]*\])?\s*;\s*//\s*(\w+)", found.group(1) if found else "",
                                  re.M):
        if enum in known:
            out[field] = enum
    return out


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
    then_enums, now_enums = {}, {}
    for header in HEADERS:
        then_enums.update(enums(source(header, RELEASE)))
        now_enums.update(enums(source(header)))
    held = {}       # {enum: [(record.field, the integer holding it)]}
    for name, path in RECORDS.items():
        now = struct_fields(source(path), name)
        if now is None:
            failures.append(f"{name}: not found in {path}")
            continue
        stored_types.update(decl.split()[0] for decl in now)
        # Each field laid out the same by every compiler
        for decl in now:
            field_type = decl.split()[0]
            if field_type not in FIXED_TYPES and field_type not in RECORDS and field_type not in LAID_OUT:
                failures.append(f"{name}: {decl}: a {field_type} is laid out as its compiler chooses"
                                f"{' (an enum: one byte on the Pico, four under ESP-IDF)' if field_type in now_enums else ''}"
                                f"; store a fixed-width integer, its comment naming the enum it holds")
        types = {decl.split()[1].split("[")[0]: decl.split()[0] for decl in now if len(decl.split()) > 1}
        held_here = held_enums(source(path), name, set(now_enums))
        for field, enum in held_here.items():
            held.setdefault(enum, []).append((f"{name}.{field}", types.get(field)))
        then = struct_fields(source(path, RELEASE), name)
        if then is not None:
            # A v1.21 enum field still names its enum, so its values are still checked
            for decl in then:
                then_type, field = decl.split()[0], decl.split()[1].split("[")[0]
                enum = held_here.get(field)
                if then_type in then_enums and field in types and ENUM_RENAMED.get(enum, enum) != then_type:
                    failures.append(f"{name}.{field}: {RELEASE}'s {then_type}, now a {types[field]} whose comment "
                                    f"names {enum or 'no enum'}")
            # v1.21's enum fields as the integer they lay in
            then = [" ".join([V121_ENUM_HELD] + decl.split()[1:]) if decl.split()[0] in then_enums else decl
                    for decl in then]
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
    # The stored enums: each value fits the integer that holds it, and each v1.21 value keeps its meaning
    for now_name in ENUM_RENAMED:
        if now_name not in now_enums:
            failures.append(f"{now_name}: in ENUM_RENAMED, but no such enum now")
    clashes = set()
    for type_name in sorted(set(held) | (stored_types & set(now_enums))):
        for where, integer in held.get(type_name, []):
            limits = FIXED_TYPES.get(integer)
            past = [value for value in now_enums[type_name] if limits is None or not limits[0] <= value <= limits[1]]
            if past:
                failures.append(f"{where}: a {integer} holding {type_name}, which has {past[0]}, past what it holds")
        then_name = ENUM_RENAMED.get(type_name, type_name)
        if then_name not in then_enums:
            print(f"{type_name:32s} new since {RELEASE}, held in {', '.join(w for w, _ in held.get(type_name, []))}")
            continue
        for value, member in then_enums[then_name].items():
            now_member = now_enums[type_name].get(value)
            if now_member != member:
                clashes.add((type_name, value))
                said = f"{type_name} {value}: {RELEASE}'s {member}, now {now_member or 'nothing'}"
                if (type_name, value) in KNOWN_OPEN:
                    print(f"{said}: known, open: {KNOWN_OPEN[(type_name, value)]}")
                else:
                    failures.append(f"{said}: a stored value whose meaning changed")
        if not any(t == type_name for t, _ in clashes):
            renamed = f" ({RELEASE}'s {then_name})" if then_name != type_name else ""
            print(f"{type_name:32s} every {RELEASE} value means what it meant{renamed}, held in "
                  f"{', '.join(w for w, _ in held.get(type_name, [])) or 'nothing'}")
    for key, why in KNOWN_OPEN.items():
        if key not in clashes:
            failures.append(f"{key[0]} {key[1]} no longer clashes: take it out of KNOWN_OPEN ({why})")
    for failure in failures:
        print("FAIL " + failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)} found")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
