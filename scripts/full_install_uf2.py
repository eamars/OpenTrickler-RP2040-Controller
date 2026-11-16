"""
The full install (ADR-004): one .uf2 a person drags onto the unit's USB drive, with what starts flash (the Pico 2 W's
partition table, the Pico W's bootloader), slot A's image, and the start of slot B written as erased flash, so that
slot A boots whatever slot B held. Every block carries the file's own count and its place in it.
"""
import argparse
import struct
import sys
from pathlib import Path

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_PAYLOAD = 256
UF2_DATA_ROOM = 476
FAMILIES = {"rp2040": 0xE48BFF56, "absolute": 0xE48BFF57}
ERASED = 0xFF


def parse_part(text):
    """
    A part to write, "file@address".

    Args:
        text: the argument as given.

    Returns:
        (path, address).
    """
    path, _, address = text.rpartition("@")
    return Path(path), int(address, 0)


def parse_blank(text):
    """
    A region written as erased flash, "address:bytes".

    Args:
        text: the argument as given.

    Returns:
        (address, bytes).
    """
    address, _, size = text.partition(":")
    return int(address, 0), int(size, 0)


def pages(parts, blanks):
    """
    The 256-byte pages to write, by address. A file's last page is filled out as erased flash.

    Args:
        parts: (path, address) pairs.
        blanks: (address, bytes) pairs.

    Returns:
        {address: 256 bytes}, refusing parts that overlap or are not page-aligned.
    """
    written = {}
    regions = [(address, path.read_bytes()) for path, address in parts]
    regions += [(address, bytes([ERASED]) * size) for address, size in blanks]
    for address, data in regions:
        if address % UF2_PAYLOAD:
            raise ValueError(f"{address:#x} is not on a {UF2_PAYLOAD}-byte page")
        for offset in range(0, len(data), UF2_PAYLOAD):
            page = data[offset:offset + UF2_PAYLOAD].ljust(UF2_PAYLOAD, bytes([ERASED]))
            if address + offset in written:
                raise ValueError(f"two parts write the page at {address + offset:#x}")
            written[address + offset] = page
    return written


def uf2(written, family):
    """
    The .uf2's bytes: one block a page, in address order, numbered across the whole file.

    Args:
        written: {address: 256 bytes}.
        family: the UF2 family ID.

    Returns:
        The file's bytes.
    """
    addresses = sorted(written)
    blocks = []
    for number, address in enumerate(addresses):
        header = struct.pack("<8I", UF2_MAGIC_START0, UF2_MAGIC_START1, UF2_FLAG_FAMILY_ID_PRESENT, address,
                             UF2_PAYLOAD, number, len(addresses), family)
        data = written[address].ljust(UF2_DATA_ROOM, b"\x00")
        blocks.append(header + data + struct.pack("<I", UF2_MAGIC_END))
    return b"".join(blocks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--family", choices=sorted(FAMILIES), required=True)
    parser.add_argument("--part", action="append", type=parse_part, required=True, help="file@address")
    parser.add_argument("--blank", action="append", type=parse_blank, default=[], help="address:bytes")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    written = pages(args.part, args.blank)
    args.out.write_bytes(uf2(written, FAMILIES[args.family]))
    print(f"{args.out}: {len(written)} blocks, {', '.join(f'{path.name}@{address:#x}' for path, address in args.part)}"
          f"{''.join(f', blank {address:#x}+{size:#x}' for address, size in args.blank)}")


if __name__ == "__main__":
    sys.exit(main())
