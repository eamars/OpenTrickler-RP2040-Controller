"""
Appends an OTA metadata footer to a built app.bin, producing app_ota.bin.

The footer layout mirrors src/firmware_footer.h's firmware_update_footer_t
exactly (packed, little-endian, 128 bytes total) -- the two must be kept in
sync by hand, since this script has no way to parse the C header.

Usage

    python append_firmware_footer.py --input app.bin --output app_ota.bin --board pico2_w

Dependencies: none beyond the standard library.
"""

import argparse
import logging
import re
import struct
import subprocess
import sys
import zlib


GIT_VERSION_COMMAND = ["git", "describe", "--tags", "--long", "--dirty", "--always"]

GIT_VERSION_PATTERN_REGEX = r'v(?P<major>\d+)\.(?P<minor>\d+)-(?P<patch>\d+)-g(?P<short_hash>[a-f0-9]+)(?P<dirty>-dirty)?'

FIRMWARE_FOOTER_MAGIC = 0x4B52544F
FIRMWARE_FOOTER_FORMAT_VERSION = 1
FIRMWARE_FOOTER_VERSION_STRLEN = 32
FIRMWARE_FOOTER_HASH_STRLEN = 16
FIRMWARE_FOOTER_RESERVED_LEN = 60
FIRMWARE_FOOTER_SIZE = 128

BOARD_TYPE_BY_PICO_BOARD = {
    "pico_w": 1,     # FIRMWARE_BOARD_TYPE_PICO_W
    "pico2_w": 2,    # FIRMWARE_BOARD_TYPE_PICO2_W
}

# Struct format for everything except the trailing footer_crc32, which is
# computed over these bytes and appended separately.
FOOTER_BODY_STRUCT_FORMAT = (
    "<"    # little-endian, standard sizes, no implicit padding
    "I"    # magic
    "H"    # footer_format_version
    "H"    # board_type
    "I"    # payload_size
    "I"    # payload_crc32
    f"{FIRMWARE_FOOTER_VERSION_STRLEN}s"  # version_string
    f"{FIRMWARE_FOOTER_HASH_STRLEN}s"     # vcs_hash
    f"{FIRMWARE_FOOTER_RESERVED_LEN}s"    # reserved
)


def get_git_version_and_hash():
    """Derive the firmware version string and short VCS hash from git.

    Mirrors scripts/gen_version.py's own git-describe parsing so the footer's
    embedded version matches what version_string/vcs_hash report at runtime.

    Returns:
        A (version_string, hash_string) tuple. version_string is "unknown"
        and hash_string is "" if the working tree isn't a recognizable
        annotated-tag checkout.
    """
    output = subprocess.check_output(GIT_VERSION_COMMAND, text=True)
    match = re.match(GIT_VERSION_PATTERN_REGEX, output)

    if not match:
        return "unknown", ""

    groupdict = match.groupdict()
    dirty_string = groupdict["dirty"] or ""
    version_string = f"{groupdict['major']}.{groupdict['minor']}.{groupdict['patch']}{dirty_string}"
    return version_string, groupdict["short_hash"]


def build_footer(payload: bytes, board_type: int, version_string: str, vcs_hash: str) -> bytes:
    """Build the 128-byte OTA footer for a given firmware payload.

    Args:
        payload: The raw app.bin bytes the footer will describe.
        board_type: firmware_board_type_t value (see BOARD_TYPE_BY_PICO_BOARD).
        version_string: Human-readable version, truncated/NUL-padded to fit.
        vcs_hash: Short git hash, truncated/NUL-padded to fit.

    Returns:
        The 128-byte packed footer, including its own trailing CRC32.
    """
    payload_crc32 = zlib.crc32(payload) & 0xFFFFFFFF

    body = struct.pack(
        FOOTER_BODY_STRUCT_FORMAT,
        FIRMWARE_FOOTER_MAGIC,
        FIRMWARE_FOOTER_FORMAT_VERSION,
        board_type,
        len(payload),
        payload_crc32,
        version_string.encode("ascii", errors="replace"),
        vcs_hash.encode("ascii", errors="replace"),
        b"\x00" * FIRMWARE_FOOTER_RESERVED_LEN,
    )

    footer_crc32 = zlib.crc32(body) & 0xFFFFFFFF
    footer = body + struct.pack("<I", footer_crc32)

    assert len(footer) == FIRMWARE_FOOTER_SIZE, f"footer size {len(footer)} != {FIRMWARE_FOOTER_SIZE}"
    return footer


def main(input_path: str, output_path: str, board: str):
    """Read app.bin, append its OTA footer, and write app_ota.bin.

    Args:
        input_path: Path to the plain, footer-less app.bin produced by
            pico_add_extra_outputs.
        output_path: Path to write the footer-appended OTA image to.
        board: PICO_BOARD value ("pico_w" or "pico2_w") the image was built for.
    """
    if board not in BOARD_TYPE_BY_PICO_BOARD:
        raise ValueError(f"Unknown PICO_BOARD '{board}', expected one of {sorted(BOARD_TYPE_BY_PICO_BOARD)}")

    with open(input_path, "rb") as fp:
        payload = fp.read()

    version_string, vcs_hash = get_git_version_and_hash()
    logging.debug(f"version_string={version_string} vcs_hash={vcs_hash}")

    footer = build_footer(payload, BOARD_TYPE_BY_PICO_BOARD[board], version_string, vcs_hash)

    with open(output_path, "wb") as fp:
        fp.write(payload)
        fp.write(footer)

    logging.debug(f"Wrote {output_path}: payload={len(payload)}B footer={len(footer)}B")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--input", help="Path to the input app.bin", required=True)
    parser.add_argument("--output", help="Path to write the footer-appended app_ota.bin to", required=True)
    parser.add_argument("--board", help="PICO_BOARD value the image was built for", required=True)
    parser.add_argument("-v", "--verbose", action="count", default=0)

    args = parser.parse_args()

    logging_levels = {0: logging.ERROR,
                       1: logging.DEBUG,
                       2: logging.INFO,
                       3: logging.WARNING,
                       4: logging.ERROR,
                       5: logging.CRITICAL}

    logging.basicConfig(stream=sys.stdout, level=logging_levels[args.verbose])

    main(args.input, args.output, args.board)
