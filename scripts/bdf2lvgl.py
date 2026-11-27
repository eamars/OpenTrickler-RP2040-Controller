"""BDF bitmap fonts into LVGL fonts (ADR-010, gate 4): the menu's fonts for the mini 12864, made at build time.

The screen's faces are Helvetica 8, regular and bold, and Helvetica 18 bold for a job's figure, read from the BDF
files in fonts/ (their notice is in fonts/README.md). Each becomes an LVGL font in its built-in format
(lv_font_fmt_txt): one bit a pixel, printable ASCII only (U+0020 to U+007E), the bits of each glyph packed row after
row with no padding, as LVGL reads them. Its line is as tall as those characters need, not the whole BDF's.

A font given with ":icons" also carries the menu's own icons, drawn below as pixels, at the code points ICONS gives
(a second range, in Unicode's private use): the title's marks (menu_view_12864.c).

A picture given with ":art" (a text file, '#' a lit pixel, '.' not) becomes a font of its 16 x 16 tiles, 'A' the top
left, in reading order: a label of "ABC\nDEF\nGHI" draws a 48 x 48 picture, each tile drawn on its own, so LVGL's
working buffer for a glyph stays at 256 bytes (the hidden hello's figure, fonts/claude_chan.txt).

Usage (the view's build file, src/ui/view_12864/CMakeLists.txt, runs it):
    python scripts/bdf2lvgl.py --out src/generated/menu_fonts.c helvR08.bdf:menu_font_regular:icons ...
"""
import argparse
import sys
from pathlib import Path

FIRST = 0x20
LAST = 0x7E
ART_TILE = 16
ART_FIRST = ord("A")

# The menu's icons (ADR-010, gate 5; the owner chose them from renders, 2026-11-17), each standing on the baseline.
# U+E000: joined to a network as a station, signal bars always full (no strength: clearest on the small screen).
# U+E001: the unit's own access point, a hotspot. Unicode's private use: LVGL draws nothing below U+0020.
# U+E002: a choice page's current option (the owner chose it over a radio dot and over bold, 2026-11-17).
# U+E003: the remote mark, a phone: the unit is driven from elsewhere (the owner chose it over a monitor, a pointer
# and a link, 2026-11-18; the simulated scale's mark stays the word SIM, which only a developer sees)
ICONS = {
    0xE000: ["         ##",
           "         ##",
           "      ## ##",
           "      ## ##",
           "   ## ## ##",
           "   ## ## ##",
           "## ## ## ##",
           "## ## ## ##"],
    0xE001: [" #         # ",
           "#  #     #  #",
           "# #  ###  # #",
           "# #  ###  # #",
           "# #  ###  # #",
           "#  #     #  #",
           " #         # "],
    0xE002: ["      #",
           "     ##",
           "#   ## ",
           "## ##  ",
           " ###   ",
           "  #    "],
    0xE003: ["#####",
           "#   #",
           "#   #",
           "#   #",
           "#   #",
           "#####",
           "# # #",
           "#####"],
}


def read_bdf(path: Path) -> dict:
    """Read a BDF font's printable ASCII glyphs.

    Args:
        path: the BDF file.

    Returns:
        {code: {"dx", "w", "h", "xoff", "yoff", "rows"}}, rows being each row's bits as an int, MSB first, w bits.
    """
    glyphs = {}
    lines = iter(path.read_text(encoding="latin-1").splitlines())
    for line in lines:
        if not line.startswith("STARTCHAR"):
            continue
        glyph = {"code": None}
        for line in lines:
            words = line.split()
            if not words:
                continue
            if words[0] == "ENCODING":
                glyph["code"] = int(words[1])
            elif words[0] == "DWIDTH":
                glyph["dx"] = int(words[1])
            elif words[0] == "BBX":
                glyph["w"], glyph["h"], glyph["xoff"], glyph["yoff"] = (int(word) for word in words[1:5])
            elif words[0] == "BITMAP":
                rows = []
                for row in range(glyph["h"]):
                    hex_row = next(lines).strip()
                    bits = len(hex_row) * 4
                    rows.append(int(hex_row, 16) >> (bits - glyph["w"]) if glyph["w"] else 0)
                glyph["rows"] = rows
            elif words[0] == "ENDCHAR":
                break
        if glyph["code"] is not None and FIRST <= glyph["code"] <= LAST:
            glyphs[glyph["code"]] = glyph
    missing = [chr(code) for code in range(FIRST, LAST + 1) if code not in glyphs]
    if missing:
        raise ValueError(f"{path} has no glyph for {''.join(missing)!r}")
    return glyphs


def pack(glyph: dict) -> bytes:
    """A glyph's bits, row after row with no padding, MSB first, the last byte padded with zeros.

    Args:
        glyph: one glyph from read_bdf.

    Returns:
        Its bitmap, as LVGL's plain one-bit format reads it.
    """
    bits = []
    for row in glyph["rows"]:
        bits.extend((row >> (glyph["w"] - 1 - col)) & 1 for col in range(glyph["w"]))
    bits.extend([0] * (-len(bits) % 8))
    return bytes(int("".join(map(str, bits[idx:idx + 8])), 2) for idx in range(0, len(bits), 8))


def icon_glyph(art: list) -> dict:
    """An icon drawn as text ('#' a lit pixel) as a glyph, standing on the baseline, a pixel after it.

    Args:
        art: its rows, top first, all one width.

    Returns:
        A glyph as read_bdf gives one.
    """
    width = len(art[0])
    return {"dx": width + 1, "w": width, "h": len(art), "xoff": 0, "yoff": 0,
            "rows": [int("".join("1" if c == "#" else "0" for c in row), 2) for row in art]}


def read_art(path: Path) -> dict:
    """A picture's 16 x 16 tiles as glyphs, from 'A' on in reading order.

    Args:
        path: the picture as text, '#' a lit pixel, '.' not; its sides multiples of 16.

    Returns:
        {code: glyph}, as read_bdf gives them.
    """
    rows = [line.strip() for line in path.read_text(encoding="ascii").splitlines() if line.strip()]
    width = len(rows[0])
    if any(len(row) != width for row in rows) or width % ART_TILE or len(rows) % ART_TILE:
        raise ValueError(f"{path}: every row {width} wide, the sides multiples of {ART_TILE}")
    if any(c not in "#." for row in rows for c in row):
        raise ValueError(f"{path}: only '#' and '.'")
    glyphs = {}
    code = ART_FIRST
    for top in range(0, len(rows), ART_TILE):
        for left in range(0, width, ART_TILE):
            tile = [row[left:left + ART_TILE] for row in rows[top:top + ART_TILE]]
            glyphs[code] = {"dx": ART_TILE, "w": ART_TILE, "h": ART_TILE, "xoff": 0, "yoff": 0,
                            "rows": [int("".join("1" if c == "#" else "0" for c in row), 2) for row in tile]}
            code += 1
    return glyphs


def font_c(glyphs: dict, name: str, source: str, icons: bool, first: int = FIRST, last: int = LAST) -> str:
    """One font as C: its bitmaps, its glyphs' descriptions, its character ranges and its lv_font_t.

    Args:
        glyphs: the font's glyphs from read_bdf or read_art.
        name: the C name of its lv_font_t.
        source: the BDF file's name, for the comment.
        icons: whether it carries the menu's icons too.
        first: its first code.
        last: its last code.

    Returns:
        The C text.
    """
    ascent = max(glyph["yoff"] + glyph["h"] for glyph in glyphs.values())
    descent = max(-glyph["yoff"] for glyph in glyphs.values())
    codes = list(range(first, last + 1))
    if icons:
        glyphs = dict(glyphs)
        for code, art in ICONS.items():
            glyphs[code] = icon_glyph(art)
        codes += sorted(ICONS)
    bitmap = bytearray()
    descriptions = ["    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},"]
    for code in codes:
        glyph = glyphs[code]
        descriptions.append(
            f"    {{.bitmap_index = {len(bitmap)}, .adv_w = {glyph['dx'] * 16}, .box_w = {glyph['w']}, "
            f".box_h = {glyph['h']}, .ofs_x = {glyph['xoff']}, .ofs_y = {glyph['yoff']}}},")
        bitmap += pack(glyph)
    bitmap_lines = [", ".join(f"0x{byte:02x}" for byte in bitmap[idx:idx + 16]) + ","
                    for idx in range(0, len(bitmap), 16)]
    icon_cmap = ""
    if icons:
        icon_cmap = (f"\n    {{.range_start = {min(ICONS)}, .range_length = {len(ICONS)}, .glyph_id_start = "
                     f"{last - first + 2}, .unicode_list = NULL,\n     .glyph_id_ofs_list = NULL, .list_length = 0, "
                     f".type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}},")
    return f"""
// {name}: {source}, U+{first:04X} to U+{last:04X}, one bit a pixel, a line of {ascent + descent} pixels
static const uint8_t {name}_bitmap[] = {{
    {chr(10).join("    " + line for line in bitmap_lines).strip()}
}};

static const lv_font_fmt_txt_glyph_dsc_t {name}_glyphs[] = {{
{chr(10).join(descriptions)}
}};

static const lv_font_fmt_txt_cmap_t {name}_cmaps[] = {{
    {{.range_start = {first}, .range_length = {last - first + 1}, .glyph_id_start = 1, .unicode_list = NULL,
     .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}},{icon_cmap}
}};

static const lv_font_fmt_txt_dsc_t {name}_dsc = {{
    .glyph_bitmap = {name}_bitmap,
    .glyph_dsc = {name}_glyphs,
    .cmaps = {name}_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = {2 if icons else 1},
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
}};

const lv_font_t {name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {ascent + descent},
    .base_line = {descent},
    .cap_height = 0,
    .x_height = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = 0,
    .underline_thickness = 0,
    .static_bitmap = 1,
    .dsc = &{name}_dsc,
    .fallback = NULL,
    .user_data = NULL,
}};
"""


def main() -> int:
    """Write the C file of every font given.

    Returns:
        The process's exit code.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, required=True, help="the C file written")
    parser.add_argument("fonts", nargs="+",
                        help="<bdf path>:<C name>, <bdf path>:<C name>:icons, or <picture path>:<C name>:art")
    args = parser.parse_args()

    parts = ["// Made by scripts/bdf2lvgl.py from the BDF fonts in fonts/ (ADR-010, gate 4): do not edit\n"
             '#include "lvgl/lvgl.h"\n']
    for spec in args.fonts:
        if spec.endswith(":art"):
            path_text, _, name = spec.removesuffix(":art").rpartition(":")
            path = Path(path_text)
            tiles = read_art(path)
            parts.append(font_c(tiles, name, path.name, False, ART_FIRST, ART_FIRST + len(tiles) - 1))
            continue
        icons = spec.endswith(":icons")
        path_text, _, name = spec.removesuffix(":icons").rpartition(":")
        path = Path(path_text)
        parts.append(font_c(read_bdf(path), name, path.name, icons))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("".join(parts), encoding="ascii", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
