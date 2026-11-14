"""PBM reader and PNG writer for the 128x64 display previews (standard library only, no Pillow).

The display preview writes each screen as a plain PBM (P1); build_preview.py and doc_images.py turn
it into a PNG scaled SCALE times with an LCD look. A PNG here is a list of rows of RGB tuples.
"""
import struct
import zlib

SCALE = 4
BACKGROUND = (214, 224, 206)
PIXEL = (26, 34, 26)


def read_pbm(path):
    """Rows of 0/1 pixels of a P1 PBM file"""
    lines = open(path).read().split("\n")
    assert lines[0] == "P1", path
    width, height = (int(v) for v in lines[1].split())
    pixels = [[int(c) for c in line] for line in lines[2:2 + height]]
    assert all(len(row) == width for row in pixels), path
    return pixels


def lcd_rows(pixels):
    """RGB rows of the LCD look, SCALE times the size of the 0/1 pixel rows"""
    rows = []
    for row in pixels:
        rgb_row = []
        for value in row:
            rgb_row += [PIXEL if value else BACKGROUND] * SCALE
        rows += [list(rgb_row) for _ in range(SCALE)]
    return rows


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)


def write_png(path, rows):
    """rows: list of rows, each a list of (r, g, b) tuples"""
    height = len(rows)
    width = len(rows[0])
    raw = bytearray()
    for row in rows:
        raw.append(0)       # filter type none
        for pixel in row:
            raw += bytes(pixel)
    data = (b"\x89PNG\r\n\x1a\n"
            + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + png_chunk(b"IEND", b""))
    open(path, "wb").write(data)
