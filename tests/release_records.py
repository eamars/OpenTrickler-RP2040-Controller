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
  FIXED_TYPES, a stored struct of RECORDS, one of LAID_OUT, or an enum that names its integer (C23's
  `typedef enum : uint8_t {...} scale_driver_t;`). A plain enum is not: one byte with the Pico's compiler, which
  shortens an enum to its values, an int's four with one that does not (ESP-IDF's, as far as is known). An enum of a
  v1.21 field names `uint8_t`, the one byte it lay in. A value past its integer the compiler refuses itself.
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
# The types a stored field may have besides the structs of RECORDS; the integers among them are what an enum may name
INTEGERS = {"uint8_t", "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t"}
FIXED_TYPES = INTEGERS | {"float", "bool", "char"}
# A stored type that is not a typedef struct, and what lays it out the same everywhere
LAID_OUT = {"rgbw_u32_t": "a union of a uint32_t and its four bytes, packed (neopixel_led.h)"}
# v1.21's stored enums each lay in one byte, as the Pico's compiler makes an enum whose values fit (gdb's ptype /o on
# both images before gate 8; v1.21's charge record has decimal_places at byte 24, one byte, in host_test.py)
V121_ENUM_INTEGER = "uint8_t"
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
# The portal's reading of an old file's scale make, and the makes' names
PORTAL = "src/web/api/portal/unit.js"
SCALE_CHOICES = "src/services/scale/scale.c"


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
    """Each `typedef enum [: integer] {...} name;` as {name: (its integer or None, {value: member})}."""
    out = {}
    for integer, body, name in re.findall(r"typedef\s+enum\s*\w*\s*(?::\s*(\w+)\s*)?\{([^{}]*)\}\s*(\w+)\s*;",
                                          without_comments(text), re.S):
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
        out[name] = (integer or None, members)
    return out


def portal_scale_makes(then_enums: dict) -> list:
    """
    A backup from the release's web page holds the scale's make as its stored number (the old code s0), which the
    portal reads by its own table: each number of the release must read as the make it was. It read v1.21's 9,
    FA-E/EN, as the simulated scale until 2026-11-28.

    Args:
        then_enums: the release's stored enums, as enums() gives them.

    Returns:
        The failures, each a line.
    """
    found = re.search(r's0:\s*\["driver",\s*\[([^\]]*)\]\]', source(PORTAL))
    if found is None:
        return [f"{PORTAL}: the old scale makes' table (s0) not found"]
    table = re.findall(r'"(\w+)"', found.group(1))
    text = dict(re.findall(r'\.held = (SCALE_DRIVER_\w+), \.text = "(\w+)"', source(SCALE_CHOICES)))
    failures = []
    for value, member in sorted(then_enums["scale_driver_t"][1].items()):
        read = table[value] if value < len(table) else None
        if read != text.get(member):
            failures.append(f"{PORTAL}: an old file's scale make {value} ({RELEASE}'s {member}) is read as "
                            f"{read or 'nothing'}, not {text.get(member) or 'a make this firmware has'}")
    if not failures:
        print(f"{'the portal, s0':32s} every {RELEASE} scale make of an old file read as the make it was")
    return failures


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
    v121_name = {then: now for now, then in ENUM_RENAMED.items()}
    for name, path in RECORDS.items():
        now = struct_fields(source(path), name)
        if now is None:
            failures.append(f"{name}: not found in {path}")
            continue
        stored_types.update(decl.split()[0] for decl in now)
        # Each field laid out the same by every compiler
        for decl in now:
            field_type = decl.split()[0]
            if field_type in now_enums and now_enums[field_type][0] not in INTEGERS:
                failures.append(f"{name}: {decl}: an enum its compiler sizes (one byte on the Pico, an int's four "
                                f"where a compiler does not shorten it): write it `typedef enum : uint8_t {{...}} "
                                f"{field_type};`")
            elif field_type not in FIXED_TYPES.union(now_enums, RECORDS, LAID_OUT):
                failures.append(f"{name}: {decl}: a {field_type} is laid out as its compiler chooses: a stored field "
                                f"is of a fixed width (FIXED_TYPES)")
        then = struct_fields(source(path, RELEASE), name)
        if then is not None:
            # v1.21's fields by today's names, each enum of them in the one byte it lay in
            then = [" ".join([v121_name.get(decl.split()[0], decl.split()[0])] + decl.split()[1:]) for decl in then]
            for decl in then:
                field_type = decl.split()[0]
                if field_type in now_enums and now_enums[field_type][0] != V121_ENUM_INTEGER:
                    failures.append(f"{name}: {decl}: {RELEASE}'s enum lay in a {V121_ENUM_INTEGER}, today's names "
                                    f"{now_enums[field_type][0] or 'none'}")
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
    for now_name in ENUM_RENAMED:
        if now_name not in now_enums:
            failures.append(f"{now_name}: in ENUM_RENAMED, but no such enum now")
    clashes = set()
    for type_name in sorted(stored_types & set(now_enums)):
        then_name = ENUM_RENAMED.get(type_name, type_name)
        if then_name not in then_enums:
            print(f"{type_name:32s} new since {RELEASE}, held in a {now_enums[type_name][0]}")
            continue
        for value, member in then_enums[then_name][1].items():
            now_member = now_enums[type_name][1].get(value)
            if now_member != member:
                clashes.add((type_name, value))
                said = f"{type_name} {value}: {RELEASE}'s {member}, now {now_member or 'nothing'}"
                if (type_name, value) in KNOWN_OPEN:
                    print(f"{said}: known, open: {KNOWN_OPEN[(type_name, value)]}")
                else:
                    failures.append(f"{said}: a stored value whose meaning changed")
        if not any(t == type_name for t, _ in clashes):
            renamed = f" ({RELEASE}'s {then_name})" if then_name != type_name else ""
            print(f"{type_name:32s} every {RELEASE} value means what it meant{renamed}, held in a "
                  f"{now_enums[type_name][0]}")
    for key, why in KNOWN_OPEN.items():
        if key not in clashes:
            failures.append(f"{key[0]} {key[1]} no longer clashes: take it out of KNOWN_OPEN ({why})")
    failures += portal_scale_makes(then_enums)
    for failure in failures:
        print("FAIL " + failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)} found")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
