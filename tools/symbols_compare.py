"""Compare two firmware images by their symbols: each function and object by name and size.

The check a move of files, or a change in how they are linked, is judged by (ADR-006). Our own code is built at -O0,
so a file that only moved, or is now built as a library, gives every function the size it had. A symbol gone, new or
of another size is a change to explain: a handler the linker dropped shows here, and in no run of the unit's menu.

Addresses are not compared: another link order moves everything.

Usage:
    python tools/symbols_compare.py build_adr006/before_w/app.elf build_w/app.elf
    python tools/symbols_compare.py before.elf after.elf --expect "^version_"

A name matched by --expect may differ (the version's text grows with the commit count); it is listed, not judged.
Ends with VERDICT PASS or VERDICT FAIL and exits 0 or 1.
"""

import argparse
import pathlib
import re
import shutil
import subprocess
import sys
from collections import Counter

REPO = pathlib.Path(__file__).resolve().parent.parent
PICO_SDK_ROOT = pathlib.Path.home() / ".pico-sdk"
# The versions, from toolchain.env (ADR-011): one NAME=value a line
VERSIONS = dict(re.findall(r"^([A-Z_]+)=(\S+)", (REPO / "toolchain.env").read_text(encoding="utf-8"), re.M))
# The VS Code extension names the compiler's folder 15_2_Rel1 for Arm's release 15.2.rel1
TOOLCHAIN_BIN = PICO_SDK_ROOT / "toolchain" / VERSIONS["ARM_GCC"].replace(".", "_").replace("rel", "Rel") / "bin"

# nm -S: address, size, type, name. A symbol with no size (a label, a linker script's name) has three fields
NM_LINE = re.compile(r"^[0-9a-f]+ ([0-9a-f]+) (\S) (.+)$")
# Arm's mapping symbols mark code and data inside a section; they are not ours
MAPPING_SYMBOL = re.compile(r"^\$[atdx](\.|$)")
DIFFERENCES_SHOWN = 40


def nm_tool():
    """
    Find the compiler's nm.

    Returns:
        Its path: the one toolchain.env names, else the one on PATH.
    """
    candidate = TOOLCHAIN_BIN / "arm-none-eabi-nm.exe"
    if candidate.exists():
        return str(candidate)
    found = shutil.which("arm-none-eabi-nm")
    if found is None:
        raise FileNotFoundError(f"arm-none-eabi-nm is not in {TOOLCHAIN_BIN} nor on PATH")
    return found


def read_symbols(elf):
    """
    Read an image's sized symbols.

    Args:
        elf: the image's ELF file.

    Returns:
        A Counter of (name, type, size): a count, since two files may each have a static function of one name.
    """
    listing = subprocess.run([nm_tool(), "-S", "--defined-only", str(elf)], check=True, capture_output=True,
                             text=True).stdout
    symbols = Counter()
    for line in listing.splitlines():
        match = NM_LINE.match(line)
        if match is None:
            continue
        size, kind, name = int(match.group(1), 16), match.group(2), match.group(3)
        if MAPPING_SYMBOL.match(name):
            continue
        # A local and a global of one kind are the same thing to this check: t and T are both code
        symbols[(name, kind.lower(), size)] += 1
    return symbols


def describe(entry, count):
    """
    One symbol as a line of the report.

    Args:
        entry: (name, type, size).
        count: how many of it.

    Returns:
        The line.
    """
    name, kind, size = entry
    times = f" x{count}" if count > 1 else ""
    return f"{kind} {size:6d}  {name}{times}"


def main():
    """
    Compare the two images named on the command line and print the verdict.

    Returns:
        0 when every symbol is the same but for those expected to differ, else 1.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("before", type=pathlib.Path, help="the image before the change (an ELF)")
    parser.add_argument("after", type=pathlib.Path, help="the image after it")
    parser.add_argument("--expect", action="append", default=[],
                        help="a regular expression: a name it matches may differ, and is only listed")
    args = parser.parse_args()

    before, after = read_symbols(args.before), read_symbols(args.after)
    expected = [re.compile(pattern) for pattern in args.expect]
    gone, new = before - after, after - before

    judged, listed = [], []
    for label, difference in (("gone", gone), ("new", new)):
        for entry, count in sorted(difference.items()):
            line = f"{label:4s} {describe(entry, count)}"
            if any(pattern.search(entry[0]) for pattern in expected):
                listed.append(line)
            else:
                judged.append(line)

    print(f"before: {sum(before.values())} sized symbols, {args.before}")
    print(f"after:  {sum(after.values())} sized symbols, {args.after}")
    if listed:
        print(f"expected to differ ({len(listed)}):")
        for line in listed:
            print(f"  {line}")
    if judged:
        print(f"differences ({len(judged)}):")
        for line in judged[:DIFFERENCES_SHOWN]:
            print(f"  {line}")
        if len(judged) > DIFFERENCES_SHOWN:
            print(f"  ... and {len(judged) - DIFFERENCES_SHOWN} more")
        print(f"VERDICT FAIL {len(judged)} symbols gone, new or of another size")
        return 1
    print("VERDICT PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
