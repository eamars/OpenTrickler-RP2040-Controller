"""
A 128 x 64 one-bit panel drawn with u8g2's own fonts, for mock-ups of the unit's screen (ADR-010; the skill
`.agents/skills/embedded-ui-design`). Every glyph is u8g2's bitmap, so a mock-up is what the unit can draw.

Fonts come from either of u8g2's sources: its BDF files (library/u8g2/tools/font/bdf, `Font.bdf("helvB08")`) or its
compiled catalogue, about 2,000 fonts (library/u8g2/csrc/u8g2_fonts.c, `Font.u8g2("NokiaSmallPlain_tr")`).
Lit pixels are drawn dark on light, as the mini 12864 shows them and rc_run.py's snaps draw them.

A font's glyphs, to check letters that may be confused (u v, I l 1, 0 O):
    venv\\Scripts\\python tools\\ui_mock\\panel.py glyphs NokiaSmallPlain_tr "uvwIl10O"
"""
import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BDF_DIR = REPO / "library" / "u8g2" / "tools" / "font" / "bdf"
U8G2_FONTS_C = REPO / "library" / "u8g2" / "csrc" / "u8g2_fonts.c"
W, H = 128, 64
U8G2_HEADER_SIZE = 23


class _Bits:
    """u8g2's glyph bit stream, read LSB first (u8g2_font.c, u8g2_font_decode_get_unsigned_bits)."""

    def __init__(self, data, at):
        self.data, self.at, self.bit = data, at, 0

    def get(self, count):
        """
        Args:
            count: bits to read.

        Returns:
            Their value, unsigned.
        """
        value = self.data[self.at] >> self.bit
        end = self.bit + count
        if end >= 8:
            self.at += 1
            if self.at < len(self.data):
                value |= self.data[self.at] << (8 - self.bit)
            end -= 8
        self.bit = end
        return value & ((1 << count) - 1)

    def signed(self, count):
        """
        Args:
            count: bits to read.

        Returns:
            Their value, stored biased by half its range.
        """
        return self.get(count) - (1 << (count - 1))


def _c_string_bytes(literal):
    """
    A C string literal's body as bytes: octal escapes, the usual backslash escapes, plain characters.

    Args:
        literal: the text between the quotes.

    Returns:
        The bytes it stands for.
    """
    out = bytearray()
    idx = 0
    while idx < len(literal):
        char = literal[idx]
        if char != "\\":
            out.append(ord(char))
            idx += 1
            continue
        nxt = literal[idx + 1]
        if nxt in "01234567":
            end = idx + 1
            while end < len(literal) and end < idx + 4 and literal[end] in "01234567":
                end += 1
            out.append(int(literal[idx + 1:end], 8))
            idx = end
            continue
        out.append({"n": 10, "t": 9, "r": 13, "\\": 92, '"': 34, "'": 39}.get(nxt, ord(nxt)))
        idx += 2
    return bytes(out)


class Font:
    """A font's ASCII glyphs: code -> {"dx", "w", "h", "xoff", "yoff", "rows"}, rows MSB first, w bits each."""

    _u8g2_source = None

    def __init__(self, name, glyphs):
        """
        Args:
            name: the font's name.
            glyphs: its glyphs, as the class holds them.
        """
        self.name = name
        self.glyphs = glyphs
        caps = [glyphs[ord(c)] for c in "H0" if ord(c) in glyphs]
        self.cap = max(g["h"] + g["yoff"] for g in caps)
        self.descent = max(-g["yoff"] for code, g in glyphs.items() if 32 <= code < 127)

    @classmethod
    def bdf(cls, name):
        """
        A font from u8g2's BDF sources.

        Args:
            name: the file's name without .bdf ("helvB08").

        Returns:
            The Font.
        """
        glyphs = {}
        lines = iter((BDF_DIR / f"{name}.bdf").read_text(encoding="latin-1").splitlines())
        for line in lines:
            if not line.startswith("STARTCHAR"):
                continue
            glyph = {}
            for line in lines:
                words = line.split()
                if not words:
                    continue
                if words[0] == "ENCODING":
                    glyph["code"] = int(words[-1])
                elif words[0] == "DWIDTH":
                    glyph["dx"] = int(words[1])
                elif words[0] == "BBX":
                    glyph["w"], glyph["h"], glyph["xoff"], glyph["yoff"] = (int(word) for word in words[1:5])
                elif words[0] == "BITMAP":
                    rows = []
                    for _ in range(glyph["h"]):
                        hex_row = next(lines).strip()
                        rows.append(int(hex_row, 16) >> (len(hex_row) * 4 - glyph["w"]) if glyph["w"] else 0)
                    glyph["rows"] = rows
                elif words[0] == "ENDCHAR":
                    break
            if "code" in glyph and "rows" in glyph and glyph["code"] < 256:
                glyphs[glyph["code"]] = glyph
        return cls(name, glyphs)

    @classmethod
    def u8g2(cls, name):
        """
        A font from u8g2's compiled catalogue, decoded as u8g2_font.c decodes it: a 23-byte header, then each
        glyph as its code, the bytes to the next glyph, and a bit stream (width, height, x and y offsets, advance,
        then runs of 0s and 1s, each pair repeated while a 1 bit follows). Codes above 255 are not read.

        Args:
            name: its name without "u8g2_font_" ("NokiaSmallPlain_tr", "logisoso22_tr").

        Returns:
            The Font.
        """
        if cls._u8g2_source is None:
            cls._u8g2_source = U8G2_FONTS_C.read_text(encoding="latin-1")
        found = re.search(r"const uint8_t u8g2_font_" + re.escape(name) +
                          r"\[\d+\][^=]*=\s*((?:\"(?:[^\"\\]|\\.)*\"\s*)+);", cls._u8g2_source)
        if found is None:
            raise KeyError(f"u8g2 has no font {name}")
        data = b"".join(_c_string_bytes(part) for part in re.findall(r"\"((?:[^\"\\]|\\.)*)\"", found.group(1)))
        m0, m1, bits_w, bits_h, bits_x, bits_y, bits_dx = data[2:9]
        glyphs = {}
        at = U8G2_HEADER_SIZE
        while at + 1 < len(data) and data[at + 1] != 0:
            code, size = data[at], data[at + 1]
            bits = _Bits(data, at + 2)
            width, height = bits.get(bits_w), bits.get(bits_h)
            xoff, yoff, advance = bits.signed(bits_x), bits.signed(bits_y), bits.signed(bits_dx)
            pixels = []
            while len(pixels) < width * height:
                zeros, ones = bits.get(m0), bits.get(m1)
                while True:
                    pixels.extend([0] * zeros + [1] * ones)
                    if len(pixels) >= width * height or bits.get(1) == 0:
                        break
            rows = [int("".join(str(pixel) for pixel in pixels[r * width:(r + 1) * width]) or "0", 2)
                    for r in range(height)]
            glyphs[code] = {"code": code, "dx": advance, "w": width, "h": height, "xoff": xoff, "yoff": yoff,
                            "rows": rows}
            at += size
        return cls(name, glyphs)

    @classmethod
    def load(cls, name):
        """
        A font by name from either source: a BDF file when there is one by that name, else the catalogue.

        Args:
            name: "helvB08" or "NokiaSmallPlain_tr".

        Returns:
            The Font.
        """
        return cls.bdf(name) if (BDF_DIR / f"{name}.bdf").exists() else cls.u8g2(name)

    def glyph(self, char):
        """
        Args:
            char: a character.

        Returns:
            Its glyph, or the space's when the font has none (the menu shows ASCII only).
        """
        return self.glyphs.get(ord(char)) or self.glyphs[ord(" ")]

    def width(self, text):
        """
        Args:
            text: a line of text.

        Returns:
            Its advance in pixels.
        """
        return sum(self.glyph(char)["dx"] for char in text)


class Panel:
    """The 128 x 64 panel's pixels, True for lit."""

    def __init__(self):
        self.px = [[False] * W for _ in range(H)]

    def set(self, x, y, lit=True):
        """A pixel, ignored outside the panel."""
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = lit

    def rect(self, x, y, w, h, lit=True):
        """A filled rectangle."""
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.set(xx, yy, lit)

    def frame(self, x, y, w, h):
        """A rectangle's outline."""
        for xx in range(x, x + w):
            self.set(xx, y)
            self.set(xx, y + h - 1)
        for yy in range(y, y + h):
            self.set(x, yy)
            self.set(x + w - 1, yy)

    def hline(self, x, y, w):
        """A horizontal line."""
        for xx in range(x, x + w):
            self.set(xx, y)

    def text(self, font, x, base, text, lit=True, align="left", right=W):
        """
        A line of text, its baseline at `base`.

        Args:
            font: a Font.
            x: its left edge (align "left").
            base: the baseline's row.
            text: the text.
            lit: False to draw it unlit, on a lit box.
            align: "left", "right" (its right edge at `right`) or "center" (on the panel).
            right: the right edge for align "right".

        Returns:
            Its width in pixels.
        """
        width = font.width(text)
        if align == "right":
            x = right - width
        elif align == "center":
            x = (W - width) // 2
        for char in text:
            glyph = font.glyph(char)
            top = base - (glyph["h"] + glyph["yoff"]) + 1
            for r, row in enumerate(glyph["rows"]):
                for c in range(glyph["w"]):
                    if (row >> (glyph["w"] - 1 - c)) & 1:
                        self.set(x + glyph["xoff"] + c, top + r, lit)
            x += glyph["dx"]
        return width

    def icon(self, x, y, art, lit=True):
        """An icon drawn as text rows, '#' a lit pixel, its top left at (x, y)."""
        for r, row in enumerate(art):
            for c, char in enumerate(row):
                if char == "#":
                    self.set(x + c, y + r, lit)


def write_sheet(path, panels, captions=None, caption_font=None, scale=3, gap=20, columns=4):
    """
    Panels side by side on a sheet, each with a caption under it, as a greyscale PNG: the panel's glass light grey,
    lit pixels near black.

    Args:
        path: the PNG written.
        panels: the Panels.
        captions: a line under each, or None.
        caption_font: the Font of the captions.
        scale: screen pixels to sheet pixels.
        gap: the space around each panel, in sheet pixels.
        columns: panels in a row.
    """
    cols = min(columns, len(panels))
    rows = (len(panels) + cols - 1) // cols
    cap_h = 24 if captions else 0
    sheet_w = cols * W * scale + (cols + 1) * gap
    sheet_h = rows * (H * scale + cap_h) + (rows + 1) * gap
    img = [[255] * sheet_w for _ in range(sheet_h)]
    for idx, panel in enumerate(panels):
        ox = gap + (idx % cols) * (W * scale + gap)
        oy = gap + (idx // cols) * (H * scale + cap_h + gap)
        for y in range(H * scale):
            for x in range(W * scale):
                img[oy + y][ox + x] = 30 if panel.px[y // scale][x // scale] else 222
        for x in range(-2, W * scale + 2):
            img[oy - 2][ox + x] = img[oy + H * scale + 1][ox + x] = 120
        for y in range(-2, H * scale + 2):
            img[oy + y][ox - 2] = img[oy + y][ox + W * scale + 1] = 120
        if captions and caption_font and captions[idx]:
            x = ox
            for char in captions[idx]:
                glyph = caption_font.glyph(char)
                top = oy + H * scale + 6 + 14 - (glyph["h"] + glyph["yoff"])
                for r, row in enumerate(glyph["rows"]):
                    for c in range(glyph["w"]):
                        if (row >> (glyph["w"] - 1 - c)) & 1 and x + glyph["xoff"] + c < ox + W * scale:
                            img[top + r][x + glyph["xoff"] + c] = 60
                x += glyph["dx"]
    raw = bytearray()
    for row in img:
        raw.append(0)
        raw.extend(row)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    header = struct.pack(">IIBBBBB", sheet_w, sheet_h, 8, 0, 0, 0, 0)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
                           + chunk(b"IEND", b""))


def print_glyphs(font, chars):
    """
    Glyphs side by side as text, on one baseline, to check letters that may be confused.

    Args:
        font: a Font.
        chars: the characters.
    """
    top_row = max(font.glyph(c)["h"] + font.glyph(c)["yoff"] for c in chars)
    bottom_row = min(font.glyph(c)["yoff"] for c in chars)
    print(f"{font.name}: cap {font.cap}, descent {font.descent}")
    for level in range(top_row, bottom_row, -1):
        line = []
        for char in chars:
            glyph = font.glyph(char)
            r = glyph["h"] + glyph["yoff"] - level
            row = glyph["rows"][r] if 0 <= r < glyph["h"] else 0
            line.append("".join("#" if (row >> (glyph["w"] - 1 - c)) & 1 else "." for c in range(glyph["w"])))
        print("  ".join(line))


def read_snap(path, scale=4):
    """
    A snap of the unit's screen (rc_run.py's PNG: one bit a pixel, each screen pixel `scale` by `scale`, lit dark)
    as a Panel.

    Args:
        path: the PNG.
        scale: its pixels to a screen pixel.

    Returns:
        The Panel.
    """
    data = Path(path).read_bytes()
    at, idat, width = 8, b"", 0
    while at < len(data):
        length, tag = struct.unpack(">I4s", data[at:at + 8])
        if tag == b"IHDR":
            width = struct.unpack(">I", data[at + 8:at + 12])[0]
        elif tag == b"IDAT":
            idat += data[at + 8:at + 8 + length]
        at += 12 + length
    raw = zlib.decompress(idat)
    stride = 1 + (width + 7) // 8
    panel = Panel()
    for y in range(H):
        line = raw[y * scale * stride + 1:(y * scale + 1) * stride]
        for x in range(W):
            bit = (line[(x * scale) // 8] >> (7 - (x * scale) % 8)) & 1
            panel.px[y][x] = bit == 0
    return panel


def main():
    """
    The command line: `glyphs <font> <characters>` (a u8g2 catalogue name or a BDF name), or `sheet <out.png>
    <snap.png>...`, the unit's snaps on one sheet, each captioned with its file's name, to set beside the mock-ups.

    Returns:
        The exit code.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    glyphs = sub.add_parser("glyphs", help="print a font's glyphs")
    glyphs.add_argument("font", help="u8g2 catalogue name (NokiaSmallPlain_tr) or BDF name (helvB08)")
    glyphs.add_argument("chars")
    sheet = sub.add_parser("sheet", help="the unit's snaps on one sheet")
    sheet.add_argument("out", type=Path)
    sheet.add_argument("snaps", type=Path, nargs="+")
    args = parser.parse_args()
    if args.command == "glyphs":
        print_glyphs(Font.load(args.font), args.chars)
        return 0
    panels = [read_snap(path) for path in args.snaps]
    write_sheet(args.out, panels, [path.stem for path in args.snaps], Font.bdf("helvR14"))
    print(args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
