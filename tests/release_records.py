"""The stored records and their enums against the latest release, the release a unit updates from (ADR-009, 2026-11-26;
ADR-017, gate 1).

The owner: "NEVER INSERT, always append for EEPROM record", and "The upgrade should be always from release to release".
The shared migration (src/services/storage/common.c, load_config) reads an earlier record as the start of today's, so it
is right only where every change since the release was an append; and a stored enum's value read from the release must
still mean what the release meant. Checked here, from the sources (the release's by its tag, today's on disk):
- the records are found by use: every type whose size is handed to load_config, load_config_layout, save_config,
  save_config_now or config_read_stored, each call giving it as `sizeof(<type>)`, and the structs inside them;
- each stored struct's fields at the release (type and name, in order) are the first fields of today's; a record whose
  history had an insertion says how it reads the release's layout instead (PAST_LAYOUTS);
- each stored enum's value names at the release are today's names for the same values, but for KNOWN_OPEN, each with
  the piece that closes it; an entry of KNOWN_OPEN that no longer clashes fails too, so it is taken out;
- every stored field is of a type laid out the same by every compiler the firmware is built with (ADR-006, gate 8):
  FIXED_TYPES, a stored struct, one of LAID_OUT, or an enum that names its integer (C23's
  `typedef enum : uint8_t {...} scale_driver_t;`). A plain enum is not: one byte with the Pico's compiler, which
  shortens an enum to its values, an int's four with one that does not (ESP-IDF's, as far as is known). An enum of a
  release field names `uint8_t`, the one byte it lay in. A value past its integer the compiler refuses itself;
- the portal reads an old backup's scale make as the make the release stored by that number.
And from the compiler's own layout, each ELF given with --elf (CI gives both boards'), read from its DWARF:
- each record's fields at the release lie where they lay then (RELEASE_LAYOUTS, the release's layout read once and kept
  here), but a record of PAST_LAYOUTS;
- a field appended since does not start in the release's struct's tail padding (the filler rule), where it would read
  whatever byte the release left there, but for KNOWN_FILLER, each with what it reads;
- with two ELFs, every record laid out the same in both (ADR-007: the RP2040's compiler against ESP-IDF's).
Ends with VERDICT PASS or FAIL. CI runs it on every build, after the builds (it fetches the tags).

    venv\\Scripts\\python tests\\release_records.py [--elf build_w\\app.elf] [--elf build\\app.elf]
    venv\\Scripts\\python tests\\release_records.py --print-layouts <an ELF or object>   (the table, for a new release)
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RELEASE = "v1.21"

# Where the storage itself lies: its own calls hand sizes on, they store nothing of their own
STORAGE = "src/services/storage/"
# The calls a record's type is handed to, and which of their arguments is its size
STORE_CALLS = {"load_config": 3, "load_config_layout": 3, "save_config": 2, "save_config_now": 2,
               "config_read_stored": 2}
# The types a stored field may have besides the stored structs; the integers among them are what an enum may name
INTEGERS = {"uint8_t", "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t"}
FIXED_TYPES = INTEGERS | {"float", "bool", "char"}
# A stored type that is not a typedef struct, and what lays it out the same everywhere
LAID_OUT = {"rgbw_u32_t": "a union of a uint32_t and its four bytes, packed (neopixel_led.h)"}
# The release's stored enums each lay in one byte, as the Pico's compiler makes an enum whose values fit (gdb's ptype /o
# on both images before gate 8; v1.21's charge record has decimal_places at byte 24, one byte, in host_test.py)
V121_ENUM_INTEGER = "uint8_t"
# A stored enum renamed since the release: {today's name: the release's}
ENUM_RENAMED = {
    "wireless_auth_t": "cyw43_auth_t",      # ours, with cyw43's name until ADR-006's gate 8
}
# Records whose history had an insertion before the rule, and where they read the release's layout field by field
PAST_LAYOUTS = {
    "eeprom_charge_mode_data_t": "src/services/charge/charge_mode_layout.c (v1.19 and v2.0.0 inserted fields; ADR-009's review, B1)",
}
# Stored enum values whose meaning changed since the release, each with what closes it. Empty since main's v1.21 merge
# (#137, 2026-11-26) gave the scale driver's 9 back to FA-E/EN and moved the simulated scale to 100 (ADR-009's review, B2)
KNOWN_OPEN = {}
# Fields appended into the release's tail padding before the rule was checked, each reading what the release left in
# that byte (ADR-006, handed on by ADR-010; ADR-017): {(type, field): what reads it}
KNOWN_FILLER = {
    ("mini_12864_module_config_t", "inverted_display"): "a bool: any byte but 0 reads as inverted",
    ("mini_12864_module_config_t", "charge_view"): "a choice: a byte no choice has runs its first choice, unstored "
                                                    "(settings.c, mend_choices)",
    ("eeprom_wireless_metadata_t", "setup_done"): "a bool: any byte but 0 reads as set up",
}
# The portal's reading of an old file's scale make, and the makes' names
PORTAL = "src/web/api/portal/unit.js"
SCALE_CHOICES = "src/services/scale/scale.c"

# The release's layout, by the Pico's compiler: {type: (sizeof, [(field, offset, bytes), ...])}. Read once from the
# release's own definitions (each stored struct, enum, union and the numbers they use, from its headers at its tag),
# compiled by arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -g, and printed by --print-layouts (ADR-017, gate 1).
# Its charge record is 52 bytes with decimal_places at 24, as host_test.py's v1.21 record has them
RELEASE_LAYOUTS = {
    'eeprom_charge_mode_data_t': (52, [
        ('charge_mode_data_rev', 0, 2),
        ('coarse_stop_threshold', 4, 4),
        ('fine_stop_threshold', 8, 4),
        ('set_point_sd_margin', 12, 4),
        ('set_point_mean_margin', 16, 4),
        ('coarse_stop_gate_ratio', 20, 4),
        ('decimal_places', 24, 1),
        ('precharge_enable', 25, 1),
        ('precharge_time_ms', 28, 4),
        ('precharge_speed_rps', 32, 4),
        ('neopixel_normal_charge_colour', 36, 4),
        ('neopixel_under_charge_colour', 40, 4),
        ('neopixel_over_charge_colour', 44, 4),
        ('neopixel_not_ready_colour', 48, 4),
    ]),
    'eeprom_motor_data_t': (60, [
        ('motor_data_rev', 0, 4),
        ('motor_data', 4, 56),
    ]),
    'eeprom_neopixel_led_metadata_t': (20, [
        ('neopixel_data_rev', 0, 2),
        ('default_led_colours', 4, 12),
        ('pwm_out_led_chain_count', 16, 1),
        ('pwm_out_led_colour_order', 17, 1),
        ('pwm_out_led_is_rgbw', 18, 1),
    ]),
    'eeprom_profile_data_t': (516, [
        ('profile_data_rev', 0, 2),
        ('current_profile_idx', 2, 2),
        ('profiles', 4, 512),
    ]),
    'eeprom_scale_data_t': (6, [
        ('scale_data_rev', 0, 2),
        ('scale_driver', 2, 1),
        ('scale_baudrate', 3, 1),
        ('scale_uart_format', 4, 1),
    ]),
    'eeprom_servo_gate_config_t': (28, [
        ('servo_gate_config_rev', 0, 2),
        ('servo_gate_enable', 2, 1),
        ('shutter0_close_duty_cycle', 4, 4),
        ('shutter0_open_duty_cycle', 8, 4),
        ('shutter1_close_duty_cycle', 12, 4),
        ('shutter1_open_duty_cycle', 16, 4),
        ('shutter_close_speed_pct_s', 20, 4),
        ('shutter_open_speed_pct_s', 24, 4),
    ]),
    'eeprom_wireless_metadata_t': (108, [
        ('wireless_data_rev', 0, 2),
        ('ssid', 2, 32),
        ('pw', 34, 64),
        ('auth', 98, 1),
        ('timeout_ms', 100, 4),
        ('enable', 104, 1),
    ]),
    'mini_12864_module_config_t': (8, [
        ('data_rev', 0, 4),
        ('inverted_encoder_direction', 4, 1),
        ('display_rotation', 5, 1),
    ]),
    'motor_persistent_config_t': (28, [
        ('full_steps_per_rotation', 0, 4),
        ('current_ma', 4, 2),
        ('microsteps', 6, 2),
        ('max_speed_rps', 8, 2),
        ('r_sense', 10, 2),
        ('angular_acceleration', 12, 4),
        ('min_speed_rps', 16, 4),
        ('gear_ratio', 20, 4),
        ('inverted_direction', 24, 1),
        ('inverted_enable', 25, 1),
    ]),
    'neopixel_led_colours_t': (12, [
        ('led1_colour', 0, 4),
        ('led2_colour', 4, 4),
        ('mini12864_backlight_colour', 8, 4),
    ]),
    'profile_t': (64, [
        ('rev', 0, 4),
        ('compatibility', 4, 4),
        ('name', 8, 16),
        ('coarse_kp', 24, 4),
        ('coarse_ki', 28, 4),
        ('coarse_kd', 32, 4),
        ('coarse_min_flow_speed_rps', 36, 4),
        ('coarse_max_flow_speed_rps', 40, 4),
        ('fine_kp', 44, 4),
        ('fine_ki', 48, 4),
        ('fine_kd', 52, 4),
        ('fine_min_flow_speed_rps', 56, 4),
        ('fine_max_flow_speed_rps', 60, 4),
    ]),
}


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
    text = re.sub(r"/\*.*?\*/", lambda found: "\n" * found.group(0).count("\n"), re.sub(r"//[^\n]*", "", text),
                  flags=re.S)
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


def call_arguments(text: str, start: int) -> list:
    """A call's arguments, from the character after its "(", split at its own commas."""
    depth, args, current = 0, [], ""
    for char in text[start:]:
        if char == "(":
            depth += 1
        elif char == ")":
            if depth == 0:
                args.append(current.strip())
                return args
            depth -= 1
        elif char == "," and depth == 0:
            args.append(current.strip())
            current = ""
            continue
        current += char
    return args


def records_by_use(failures: list) -> dict:
    """
    The stored records, found by use: each type whose size a call of STORE_CALLS is given, outside the storage, and the
    structs inside them.

    Args:
        failures: a call whose size is not `sizeof(<type>)` is added to it.

    Returns:
        {type: the file that defines it, relative to the repository}.
    """
    sources = sorted(p for p in (REPO / "src").rglob("*") if p.suffix in (".c", ".h"))
    texts = {p.relative_to(REPO).as_posix(): p.read_text(encoding="utf-8", errors="replace") for p in sources}
    used = set()
    for path, text in texts.items():
        if path.startswith(STORAGE):
            continue
        code = without_comments(text)
        for call in re.finditer(r"\b(" + "|".join(STORE_CALLS) + r")\s*\(", code):
            if code[:call.start()].rstrip().endswith(("bool", "config_found_t", "*")):
                continue        # a declaration, not a call
            args = call_arguments(code, call.end())
            at = STORE_CALLS[call.group(1)]
            size = re.fullmatch(r"sizeof\s*\(\s*(\w+_t)\s*\)", args[at]) if at < len(args) else None
            if size is None:
                line = code[:call.start()].count("\n") + 1
                failures.append(f"{path}:{line}: {call.group(1)} given its size as "
                                f"{args[at] if at < len(args) else '?'}: give it as sizeof(<the record's type>), so "
                                f"this check finds the record")
                continue
            used.add(size.group(1))
    defined = {}
    for path, text in texts.items():
        for name in re.findall(r"\}\s*(\w+_t)\s*;", without_comments(text)):
            if struct_fields(text, name) is not None:
                defined.setdefault(name, path)
    records, todo = {}, sorted(used)
    while todo:
        name = todo.pop()
        if name in records:
            continue
        if name not in defined:
            failures.append(f"{name}: stored, but its typedef struct is not found in src")
            continue
        records[name] = defined[name]
        todo += [decl.split()[0] for decl in struct_fields(texts[defined[name]], name) if decl.split()[0] in defined]
    return records


def elf_layouts(elf_path: Path, names) -> dict:
    """
    Each named type's layout in an ELF's (or an object's) DWARF, as its compiler laid it out.

    Args:
        elf_path: the ELF or object, built with -g.
        names: the types, by their typedef's name.

    Returns:
        {name: (sizeof, [(field, offset, bytes), ...])}, for each name found.
    """
    from elftools.elf.elffile import ELFFile     # pyelftools (requirements.txt): CI installs it for this step

    def target(die):
        return die.get_DIE_from_attribute("DW_AT_type") if "DW_AT_type" in die.attributes else None

    def resolved(die):
        while die is not None and die.tag in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type"):
            die = target(die)
        return die

    def size_of(die):
        die = resolved(die)
        if die is None:
            return 0
        if "DW_AT_byte_size" in die.attributes:
            return die.attributes["DW_AT_byte_size"].value
        if die.tag == "DW_TAG_array_type":
            count = 1
            for child in die.iter_children():
                if child.tag == "DW_TAG_subrange_type":
                    if "DW_AT_count" in child.attributes:
                        count *= child.attributes["DW_AT_count"].value
                    elif "DW_AT_upper_bound" in child.attributes:
                        count *= child.attributes["DW_AT_upper_bound"].value + 1
            return count * size_of(target(die))
        return 0

    found = {}
    wanted = set(names)
    with open(elf_path, "rb") as stream:
        dwarf = ELFFile(stream).get_dwarf_info()
        for unit in dwarf.iter_CUs():
            for die in unit.iter_DIEs():
                if die.tag != "DW_TAG_typedef" or "DW_AT_name" not in die.attributes:
                    continue
                name = die.attributes["DW_AT_name"].value.decode()
                if name not in wanted or name in found:
                    continue
                struct = resolved(die)
                if struct is None or struct.tag not in ("DW_TAG_structure_type", "DW_TAG_union_type"):
                    continue
                fields = []
                for member in struct.iter_children():
                    if member.tag != "DW_TAG_member" or "DW_AT_name" not in member.attributes:
                        continue
                    location = member.attributes.get("DW_AT_data_member_location")
                    offset = location.value if location is not None and isinstance(location.value, int) else 0
                    fields.append((member.attributes["DW_AT_name"].value.decode(), offset, size_of(target(member))))
                found[name] = (size_of(struct), fields)
            if len(found) == len(wanted):
                break
    return found


def layout_checks(elfs: list, records: dict) -> list:
    """
    The records as each ELF's compiler laid them out, against the release's layout, the filler rule, and each other.

    Args:
        elfs: the ELFs, built with -g.
        records: the stored records (records_by_use()).

    Returns:
        The failures, each a line.
    """
    failures = []
    layouts = []
    known_seen = set()
    for elf in elfs:
        layout = elf_layouts(elf, records)
        layouts.append(layout)
        for name in sorted(records):
            if name not in layout:
                failures.append(f"{elf.name}: {name} not in its DWARF")
                continue
            size, fields = layout[name]
            if name not in RELEASE_LAYOUTS:
                print(f"{name:32s} {elf.name}: {size} bytes, new since {RELEASE}")
                continue
            then_size, then_fields = RELEASE_LAYOUTS[name]
            if name in PAST_LAYOUTS:
                # Its earlier layouts are read field by field from where each field lay (its declared map)
                print(f"{name:32s} {elf.name}: {RELEASE}'s {then_size} bytes, {size} now; read field by field")
                continue
            now = {field: (offset, width) for field, offset, width in fields}
            for idx, (field, offset, width) in enumerate(then_fields):
                # The last field may grow: a record's array of elements, each grown at its own end (the element's
                # type is checked on its own)
                last = idx == len(then_fields) - 1
                moved = field not in now or now[field][0] != offset or \
                    (now[field][1] != width if not last else now[field][1] < width)
                if moved:
                    failures.append(f"{elf.name}: {name}.{field} lay at {offset} ({width} bytes) at {RELEASE}, "
                                    f"now {now.get(field, 'gone')}: a field moved")
            then_names = {field for field, _, _ in then_fields}
            filler = [(field, offset) for field, offset, _ in fields
                      if field not in then_names and offset < then_size]
            for field, offset in filler:
                if (name, field) not in KNOWN_FILLER:
                    failures.append(f"{elf.name}: {name}.{field} starts at {offset}, in {RELEASE}'s tail padding "
                                    f"(its struct was {then_size} bytes): it reads whatever byte {RELEASE} left there. "
                                    f"Append it past the padding")
            known = [field for field, _ in filler if (name, field) in KNOWN_FILLER]
            print(f"{name:32s} {elf.name}: {RELEASE}'s {then_size} bytes, {size} now; its fields where they lay"
                  f"{'; in its filler, known: ' + ', '.join(known) if known else ''}")
            known_seen.update((name, field) for field in known)
    for name in sorted(records):
        seen = {repr(layout.get(name)) for layout in layouts}
        if len(seen) > 1:
            failures.append(f"{name}: laid out differently by the ELFs' compilers: "
                            + "; ".join(f"{elf.name} {layout.get(name)}" for elf, layout in zip(elfs, layouts)))
    for key, why in KNOWN_FILLER.items():
        if key not in known_seen:
            failures.append(f"{key[0]}.{key[1]} is in KNOWN_FILLER but not in {RELEASE}'s filler: take it out ({why})")
    return failures


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
    Check the records and their enums against the release, and their layout in each ELF given.

    Returns:
        0 when every check passes, 1 otherwise.
    """
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--elf", type=Path, action="append", default=[], help="an image built with -g, its records' "
                        "layout checked; given twice, the two compared")
    parser.add_argument("--print-layouts", type=Path, help="print the records' layout in this ELF or object as "
                        "RELEASE_LAYOUTS's entries, and stop")
    args = parser.parse_args()

    failures = []
    records = records_by_use(failures)
    if args.print_layouts is not None:
        names = set(records) | set(RELEASE_LAYOUTS)
        for name, (size, fields) in sorted(elf_layouts(args.print_layouts, names).items()):
            print(f"    {name!r}: ({size}, {fields!r}),")
        return 0
    stored_types = set()
    headers = sorted(set(records.values()) | {"src/services/storage/include/common.h"})
    then_enums, now_enums = {}, {}
    for header in headers:
        then_enums.update(enums(source(header, RELEASE)))
        now_enums.update(enums(source(header)))
    v121_name = {then: now for now, then in ENUM_RENAMED.items()}
    for name, path in sorted(records.items()):
        now = struct_fields(source(path), name)
        stored_types.update(decl.split()[0] for decl in now)
        # Each field laid out the same by every compiler
        for decl in now:
            field_type = decl.split()[0]
            if field_type in now_enums and now_enums[field_type][0] not in INTEGERS:
                failures.append(f"{name}: {decl}: an enum its compiler sizes (one byte on the Pico, an int's four "
                                f"where a compiler does not shorten it): write it `typedef enum : uint8_t {{...}} "
                                f"{field_type};`")
            elif field_type not in FIXED_TYPES.union(now_enums, records, LAID_OUT):
                failures.append(f"{name}: {decl}: a {field_type} is laid out as its compiler chooses: a stored field "
                                f"is of a fixed width (FIXED_TYPES)")
        then = struct_fields(source(path, RELEASE), name)
        if then is not None:
            # The release's fields by today's names, each enum of them in the one byte it lay in
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
    # The stored enums: each release value keeps its meaning
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
    if args.elf:
        failures += layout_checks(args.elf, records)
    else:
        print("layouts: no ELF given (--elf): the filler rule and the release's offsets not checked")
    for failure in failures:
        print("FAIL " + failure)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {len(failures)} found")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
