#!/usr/bin/env python3
"""Copies only the fonts the display screens use out of the 36 MB u8g2_fonts.c."""
import sys

FONTS = ["u8g2_font_helvB08_tr", "u8g2_font_profont11_tf", "u8g2_font_profont22_tf", "u8g2_font_helvR08_tr"]

src_path, out_path = sys.argv[1], sys.argv[2]
text = open(src_path, encoding="latin-1").read()
out = ['#include "u8g2.h"', ""]
for name in FONTS:
    start = text.index("const uint8_t %s[" % name)
    # The font ends with '";' at the end of a line (a data line may start with a ';' byte: '  ";...')
    end = text.index('";\n', start) + 2
    out.append(text[start:end])
    out.append("")
open(out_path, "w", encoding="latin-1").write("\n".join(out))
print("%s: %d fonts" % (out_path, len(FONTS)))
