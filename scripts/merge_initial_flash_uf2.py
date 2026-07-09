"""
Merges the bootloader and app .bin outputs into a single UF2 image for the
one-time manual BOOTSEL reflash needed to adopt the bootloader/app/staging
flash split (see bootloader/CMakeLists.txt and the root CMakeLists.txt).
Every OTA update after this initial flash goes through the normal
upload/stage/install REST flow instead.

The UF2 family ID and flags are read from an existing, already-built
reference .uf2 for the same board (pico_add_extra_outputs already produces
one per target) rather than hardcoded, since RP2040 and RP2350 use different
family IDs and this avoids getting either wrong.

Usage

    python merge_initial_flash_uf2.py \
        --bootloader-bin bootloader/bootloader.bin \
        --app-bin app.bin \
        --app-offset 0x10010000 \
        --reference-uf2 bootloader/bootloader.uf2 \
        --output initial_flash.uf2

Dependencies: none beyond the standard library.
"""

import argparse
import struct
import sys


UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_BLOCK_SIZE = 512
UF2_DATA_SIZE = 256
UF2_HEADER = "<IIIIIIII"
BOOTLOADER_ORIGIN = 0x10000000


def read_family_id_and_flags(reference_uf2_path: str) -> tuple:
    """Extract the UF2 flags and family ID from an existing .uf2's first block.

    Args:
        reference_uf2_path: Path to any already-built .uf2 for the target
            board (family ID/flags are the same across all targets built for
            the same board -- see pico_add_extra_outputs).

    Returns:
        A (flags, family_id) tuple as found in the reference file's first
        block header.
    """
    with open(reference_uf2_path, "rb") as fp:
        first_block = fp.read(UF2_BLOCK_SIZE)

    (magic_start0, magic_start1, flags, _target_addr, _payload_size,
     _block_no, _num_blocks, family_id) = struct.unpack(UF2_HEADER, first_block[:32])

    if magic_start0 != UF2_MAGIC_START0 or magic_start1 != UF2_MAGIC_START1:
        raise ValueError(f"{reference_uf2_path} does not look like a UF2 file")
    if not (flags & UF2_FLAG_FAMILY_ID_PRESENT):
        raise ValueError(f"{reference_uf2_path}'s first block has no family ID present")

    return flags, family_id


def build_uf2_blocks(data: bytes, target_addr: int, flags: int, family_id: int,
                      block_no: int, num_blocks: int) -> bytes:
    """Chunk `data` into UF2_DATA_SIZE-byte UF2 blocks starting at target_addr.

    Args:
        data: Raw binary payload to encode.
        target_addr: Flash address the first byte of `data` should be
            written to.
        flags: UF2 flags to stamp into every block (see UF2_FLAG_FAMILY_ID_PRESENT).
        family_id: UF2 family ID to stamp into every block.
        block_no: Global block index to start counting from (spans across
            both the bootloader's and the app's blocks in the merged file).
        num_blocks: Total block count across the whole merged file.

    Returns:
        The concatenated raw bytes of all UF2 blocks for this payload.
    """
    output = bytearray()

    for offset in range(0, len(data), UF2_DATA_SIZE):
        chunk = data[offset:offset + UF2_DATA_SIZE]
        padded_chunk = chunk.ljust(UF2_DATA_SIZE, b"\x00")

        header = struct.pack(
            UF2_HEADER,
            UF2_MAGIC_START0, UF2_MAGIC_START1, flags,
            target_addr + offset, UF2_DATA_SIZE, block_no, num_blocks, family_id,
        )
        padding = b"\x00" * (UF2_BLOCK_SIZE - len(header) - UF2_DATA_SIZE - 4)
        footer = struct.pack("<I", UF2_MAGIC_END)

        output += header + padded_chunk + padding + footer
        block_no += 1

    return bytes(output)


def main(bootloader_bin_path: str, app_bin_path: str, app_offset: int,
         reference_uf2_path: str, output_path: str):
    """Build a merged UF2 covering both the bootloader and app flash regions.

    Args:
        bootloader_bin_path: Path to the built bootloader.bin.
        app_bin_path: Path to the built app.bin (plain, footer-less --
            the OTA footer is only relevant to app_ota.bin, uploaded later
            through the web UI, not to this one-time direct flash).
        app_offset: Flash address the app region starts at (must match
            FIRMWARE_UPDATE_APP_REGION_OFFSET for the board being built).
        reference_uf2_path: Any already-built .uf2 for the same board, used
            to source the correct UF2 family ID/flags.
        output_path: Path to write the merged initial_flash.uf2 to.
    """
    with open(bootloader_bin_path, "rb") as fp:
        bootloader_data = fp.read()
    with open(app_bin_path, "rb") as fp:
        app_data = fp.read()

    flags, family_id = read_family_id_and_flags(reference_uf2_path)

    total_blocks = (
        (len(bootloader_data) + UF2_DATA_SIZE - 1) // UF2_DATA_SIZE
        + (len(app_data) + UF2_DATA_SIZE - 1) // UF2_DATA_SIZE
    )

    bootloader_blocks = build_uf2_blocks(
        bootloader_data, BOOTLOADER_ORIGIN, flags, family_id,
        block_no=0, num_blocks=total_blocks,
    )
    bootloader_block_count = len(bootloader_blocks) // UF2_BLOCK_SIZE

    app_blocks = build_uf2_blocks(
        app_data, app_offset, flags, family_id,
        block_no=bootloader_block_count, num_blocks=total_blocks,
    )

    with open(output_path, "wb") as fp:
        fp.write(bootloader_blocks)
        fp.write(app_blocks)

    print(f"Wrote {output_path}: {total_blocks} blocks "
          f"(bootloader @ 0x{BOOTLOADER_ORIGIN:08X}, app @ 0x{app_offset:08X})")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--bootloader-bin", required=True, help="Path to the built bootloader.bin")
    parser.add_argument("--app-bin", required=True, help="Path to the built app.bin (plain, footer-less)")
    parser.add_argument("--app-offset", required=True, type=lambda s: int(s, 0),
                         help="Flash address the app region starts at, e.g. 0x10010000")
    parser.add_argument("--reference-uf2", required=True,
                         help="Any already-built .uf2 for the same board, to source the UF2 family ID/flags")
    parser.add_argument("--output", required=True, help="Path to write the merged UF2 to")

    args = parser.parse_args()

    if args.app_offset <= BOOTLOADER_ORIGIN:
        print(f"error: --app-offset (0x{args.app_offset:08X}) must be greater than "
              f"the bootloader's own origin (0x{BOOTLOADER_ORIGIN:08X})", file=sys.stderr)
        sys.exit(1)

    main(args.bootloader_bin, args.app_bin, args.app_offset, args.reference_uf2, args.output)
