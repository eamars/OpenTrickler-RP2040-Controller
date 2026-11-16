"""
The full install (ADR-004): one .uf2 a person drags onto the unit's USB drive. It writes the whole flash: what starts
it (the Pico 2 W's partition table, the Pico W's bootloader) and slot A's image where they go, and every other page as
erased flash: slot B, the rest of slot A, the Pico 2 W's spare. The unit is left in its factory state, no other image
kept (the owner, ADR-004). Every block carries the file's own count and its place in it.
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
FLASH_SECTOR = 4096


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


def parse_flash(text):
    """
    The flash's range, "address:bytes".

    Args:
        text: the argument as given.

    Returns:
        (address, bytes).
    """
    address, _, size = text.partition(":")
    return int(address, 0), int(size, 0)


def pages(parts, flash):
    """
    Every 256-byte page of the flash, by address: the parts' where they are, erased flash elsewhere. So every 4 KB
    sector is written whole, its 16 pages 16 blocks in a row that begin a 16-block stretch of the file, as in a UF2
    the SDK makes. A sector that straddled two stretches lost the pages written before the second began: the Pico W's
    ROM had erased it again there (ADR-004, its first install by USB).

    Args:
        parts: (path, address) pairs.
        flash: (address, bytes), the flash's range, whole sectors.

    Returns:
        {address: 256 bytes}, refusing parts that overlap, are not page-aligned or lie outside the flash.
    """
    start, size = flash
    if start % FLASH_SECTOR or size % FLASH_SECTOR:
        raise ValueError(f"the flash {start:#x}+{size:#x} is not whole {FLASH_SECTOR}-byte sectors")
    written = {}
    for path, address in parts:
        data = path.read_bytes()
        if address % UF2_PAYLOAD:
            raise ValueError(f"{address:#x} is not on a {UF2_PAYLOAD}-byte page")
        if address < start or address + len(data) > start + size:
            raise ValueError(f"{path.name} at {address:#x} does not lie inside the flash")
        for offset in range(0, len(data), UF2_PAYLOAD):
            if address + offset in written:
                raise ValueError(f"two parts write the page at {address + offset:#x}")
            written[address + offset] = data[offset:offset + UF2_PAYLOAD].ljust(UF2_PAYLOAD, bytes([ERASED]))
    for address in range(start, start + size, UF2_PAYLOAD):
        written.setdefault(address, bytes([ERASED]) * UF2_PAYLOAD)
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
    parser.add_argument("--flash", type=parse_flash, required=True, help="address:bytes, the whole flash")
    parser.add_argument("--part", action="append", type=parse_part, default=[], help="file@address")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if not args.part:
        parser.error("give a --part")
    written = pages(args.part, args.flash)
    args.out.write_bytes(uf2(written, FAMILIES[args.family]))
    print(f"{args.out}: {len(written)} blocks, {', '.join(f'{path.name}@{address:#x}' for path, address in args.part)}"
          f", the rest of {args.flash[0]:#x}+{args.flash[1]:#x} erased")


if __name__ == "__main__":
    sys.exit(main())
