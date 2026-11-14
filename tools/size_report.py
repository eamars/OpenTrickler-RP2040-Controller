"""Print what a firmware image is made of, from the linker's map file.

Flash is summed by part (the radio, lwIP, the pages, the application and so on). RAM is what the image takes at
link time: .data and .bss. Run it after a build; CI runs it for both boards (ADR-001, step 2).

Usage:
    python tools/size_report.py build/app.elf.map --bin build/app.bin
    python tools/size_report.py build/app.elf.map --bin build/app.bin --markdown --title "Pico 2 W"

With --bin, the flash total is compared with the image file and the script exits 1 when they differ by more than
BIN_TOLERANCE, which would mean the report no longer counts the image correctly.
"""

import argparse
import pathlib
import re
import sys
from collections import defaultdict

FLASH_START, FLASH_END = 0x10000000, 0x11000000
RAM_START, RAM_END = 0x20000000, 0x20100000
BIN_TOLERANCE = 0.02
LARGEST_OBJECTS_SHOWN = 12

# RAM output sections whose initial contents are stored in flash
RAM_SECTIONS_LOADED_FROM_FLASH = (".data", ".ram_vector_table")
# What the image takes of RAM at link time
RAM_SECTIONS_COUNTED = (".data", ".bss", ".ram_vector_table", ".uninitialized_data", ".tdata", ".tbss")

OTHER_PART = "Other"
RUNTIME_PART = "C and C++ runtime"

# The map names an object built from this repository by its path from the repository's root, behind the build
# system's own prefix ("CMakeFiles/app.dir/src/motors.c.obj", "library/u8g2/libu8g2.a(mui.c.obj)"). Anything with
# an absolute path comes from outside the tree, which is the compiler's own libraries. So a part is told by where
# its source sits in the repository, never by where the repository or the toolchain is installed.
TARGET_OBJECT_PREFIX = re.compile(r"^CMakeFiles/[^/]+\.dir/")
OUTSIDE_THE_TREE = re.compile(r"^([A-Za-z]:)?/")

# Matched against the path from the repository's root. First match wins.
PARTS = (
    ("Radio: cyw43 driver and its firmware", re.compile(r"cyw43")),
    ("lwIP", re.compile(r"^library/pico-sdk/lib/lwip/|pico_lwip")),
    ("TinyUSB", re.compile(r"^library/pico-sdk/lib/tinyusb/")),
    ("u8g2 and MUI", re.compile(r"u8g2")),
    ("FreeRTOS", re.compile(r"^library/FreeRTOS-Kernel/")),
    ("Trinamic", re.compile(r"^library/Trinamic-library/")),
    ("Pico SDK", re.compile(r"pico-sdk/")),
    ("Web pages", re.compile(r"^src/rest_endpoints")),
    ("HTTP server", re.compile(r"^src/http_rest")),
    ("Application", re.compile(r"^src/")),
)

OUTPUT_SECTION_LINE = re.compile(r"^(\.[\w.]+)(?:\s+0x([0-9a-f]+)\s+0x([0-9a-f]+))?")
INPUT_SECTION_LINE = re.compile(r"^\s+(\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
CONTINUED_INPUT_LINE = re.compile(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
MEMORY_REGION_LINE = re.compile(r"^(\w+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\w+\s*$")


def part_of(object_path):
    """Name the part of the image an object file belongs to.

    Args:
        object_path: the object's path as printed in the map, for example
            "CMakeFiles/app.dir/src/motors.c.obj" or "library/u8g2/libu8g2.a(u8g2_fonts.c.obj)".

    Returns:
        RUNTIME_PART for an object from outside the repository, the part's name from PARTS for one inside it, or
        OTHER_PART when no pattern matches.
    """
    path = object_path.replace("\\", "/")
    if OUTSIDE_THE_TREE.match(path):
        return RUNTIME_PART
    path = TARGET_OBJECT_PREFIX.sub("", path)
    for name, pattern in PARTS:
        if pattern.search(path):
            return name
    return OTHER_PART


def object_name(object_path):
    """Shorten an object's path to its file name, keeping the archive member form "lib.a(member.o)".

    Args:
        object_path: the object's path as printed in the map.

    Returns:
        The last path component.
    """
    return object_path.replace("\\", "/").split("/")[-1]


def read_map(map_path):
    """Read a GNU ld map file and add up what the image takes of flash and RAM.

    Args:
        map_path: path of the map file (pathlib.Path).

    Returns:
        A dict with:
            "flash_by_part": {part name: bytes},
            "flash_by_object": {object file name: bytes},
            "ram_by_section": {output section name: bytes}, for the sections in RAM_SECTIONS_COUNTED,
            "regions": {memory region name: length in bytes}, from the map's memory configuration.
    """
    text = map_path.read_text(errors="replace")

    regions = {}
    configuration = text[text.find("Memory Configuration"):text.find("Linker script and memory map")]
    for line in configuration.splitlines():
        match = MEMORY_REGION_LINE.match(line)
        if match:
            regions[match.group(1)] = int(match.group(3), 16)

    flash_by_part = defaultdict(int)
    flash_by_object = defaultdict(int)
    ram_by_section = defaultdict(int)
    output_section = ""
    # ld puts a long output section name on a line of its own, with its address and size on the next line
    output_section_awaiting_size = False
    # ld does the same for a long input section name
    input_name_pending = False

    for line in text[text.find("Linker script and memory map"):].splitlines():
        match = OUTPUT_SECTION_LINE.match(line)
        if match:
            output_section = match.group(1)
            input_name_pending = False
            output_section_awaiting_size = match.group(2) is None
            if match.group(2) is not None:
                count_output_section(ram_by_section, output_section, int(match.group(2), 16), int(match.group(3), 16))
            continue

        match = INPUT_SECTION_LINE.match(line)
        if match:
            address, size, path = int(match.group(2), 16), int(match.group(3), 16), match.group(4)
            input_name_pending = False
            output_section_awaiting_size = False
        else:
            match = CONTINUED_INPUT_LINE.match(line)
            if match is None:
                stripped = line.strip()
                input_name_pending = stripped.startswith(".") and " " not in stripped
                continue
            address, size, path = int(match.group(1), 16), int(match.group(2), 16), match.group(3)
            if output_section_awaiting_size and ".o" not in path:
                # The address and size of the output section itself
                count_output_section(ram_by_section, output_section, address, size)
                output_section_awaiting_size = False
                continue
            if not input_name_pending:
                continue
            input_name_pending = False

        if size == 0 or ".o" not in path:
            continue
        in_flash = FLASH_START <= address < FLASH_END
        loaded_from_flash = RAM_START <= address < RAM_END and output_section in RAM_SECTIONS_LOADED_FROM_FLASH
        if in_flash or loaded_from_flash:
            flash_by_part[part_of(path)] += size
            flash_by_object[object_name(path)] += size

    return {
        "flash_by_part": dict(flash_by_part),
        "flash_by_object": dict(flash_by_object),
        "ram_by_section": dict(ram_by_section),
        "regions": regions,
    }


def count_output_section(ram_by_section, name, address, size):
    """Record an output section's size when it is one of the RAM sections the report counts.

    Args:
        ram_by_section: dict to add to, {output section name: bytes}.
        name: the output section's name, for example ".bss".
        address: where the linker placed it.
        size: its size in bytes.
    """
    if name in RAM_SECTIONS_COUNTED and RAM_START <= address < RAM_END:
        ram_by_section[name] += size


def kb(size):
    """Format a size in bytes as KB with one decimal."""
    return f"{size / 1024:.1f}"


def render(sizes, title, bin_size, markdown):
    """Lay the report out as text.

    Args:
        sizes: the dict read_map() returns.
        title: heading for the report, usually the board's name.
        bin_size: size of the image file in bytes, or None when it was not given.
        markdown: True for Markdown tables (the CI job summary), False for plain columns.

    Returns:
        The report as one string.
    """
    flash_total = sum(sizes["flash_by_part"].values())
    ram_total = sum(sizes["ram_by_section"].values())
    flash_region = sizes["regions"].get("FLASH")
    ram_region = sizes["regions"].get("RAM")

    flash_rows = [(name, kb(size), f"{100 * size / flash_total:.1f}%")
                  for name, size in sorted(sizes["flash_by_part"].items(), key=lambda item: -item[1])]
    flash_rows.append(("Total", kb(flash_total), "100%"))
    object_rows = [(name, kb(size))
                   for name, size in sorted(sizes["flash_by_object"].items(), key=lambda item: -item[1])]
    object_rows = object_rows[:LARGEST_OBJECTS_SHOWN]
    ram_rows = [(name, kb(size)) for name, size in sorted(sizes["ram_by_section"].items(), key=lambda item: -item[1])]
    ram_rows.append(("Total", kb(ram_total)))

    notes = []
    if flash_region:
        notes.append(f"Flash: {kb(flash_total)} KB of {kb(flash_region)} KB ({100 * flash_total / flash_region:.0f}%).")
    if ram_region:
        notes.append(f"RAM at link time: {kb(ram_total)} KB of {kb(ram_region)} KB ({100 * ram_total / ram_region:.0f}%).")
    if bin_size is not None:
        difference = (flash_total - bin_size) / bin_size
        notes.append(f"Image file: {kb(bin_size)} KB; the flash total is {100 * difference:+.1f}% from it.")

    tables = (
        ("Flash by part", ("Part", "KB", "Share"), flash_rows),
        ("Largest objects in flash", ("Object", "KB"), object_rows),
        ("RAM at link time", ("Section", "KB"), ram_rows),
    )
    lines = []
    if markdown:
        lines.append(f"### {title}")
        lines.extend(["", *("- " + note for note in notes)])
        for heading, header, rows in tables:
            lines.extend(["", f"**{heading}**", "", "| " + " | ".join(header) + " |",
                          "|---|" + "---:|" * (len(header) - 1)])
            lines.extend("| " + " | ".join(row) + " |" for row in rows)
        lines.append("")
    else:
        lines.append(title)
        lines.extend(notes)
        for heading, header, rows in tables:
            lines.extend(["", heading])
            width = max(len(row[0]) for row in rows)
            lines.extend(f"  {row[0]:<{width}}  " + "  ".join(f"{cell:>8}" for cell in row[1:]) for row in rows)
    return "\n".join(lines)


def main():
    """Read the command line, print the report, and return the process exit code (0, or 1 when --bin disagrees)."""
    parser = argparse.ArgumentParser(description="Print a firmware image's flash by part and its RAM at link time.")
    parser.add_argument("map", type=pathlib.Path, help="the linker's map file, for example build/app.elf.map")
    parser.add_argument("--bin", type=pathlib.Path, default=None, help="the image file, to check the flash total")
    parser.add_argument("--title", default=None, help="heading for the report (default: the map file's path)")
    parser.add_argument("--markdown", action="store_true", help="Markdown tables, for the CI job summary")
    arguments = parser.parse_args()

    sizes = read_map(arguments.map)
    bin_size = arguments.bin.stat().st_size if arguments.bin else None
    print(render(sizes, arguments.title or str(arguments.map), bin_size, arguments.markdown))

    if bin_size is None:
        return 0
    flash_total = sum(sizes["flash_by_part"].values())
    if abs(flash_total - bin_size) / bin_size > BIN_TOLERANCE:
        print(f"The flash total is more than {100 * BIN_TOLERANCE:.0f}% from the image file: the report is wrong.",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
