"""
The web pages from their sources (ADR-008): each page's markup, its stylesheet and its script joined into one file,
gzipped, as the unit would serve it, with each one's size printed. The styles are plain CSS of our own (ADR-008,
gate 1: chosen by the owner over Tailwind with daisyUI, whose trimmed CSS was about nine times larger gzipped).

    venv\\Scripts\\python scripts\\web_build.py [--out build_web]

Only the Python standard library is used.
"""
import argparse
import gzip
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SOURCES = REPO / "src" / "web"
# The script's parts, joined in this order: the shared part first, the start last
SCRIPTS = ["common.js", "stream.js", "trickler.js", "form.js", "settings.js", "powders.js", "unit.js", "update.js", "setup.js", "main.js"]


def minify_css(css):
    """
    Plain CSS made smaller as a minifier would: comments and runs of white space out.

    Args:
        css: the stylesheet's text.

    Returns:
        The text, smaller.
    """
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
    css = re.sub(r"\s+", " ", css)
    css = re.sub(r"\s*([{};:,>])\s*", r"\1", css)
    return css.replace(";}", "}").strip()


def join(css, script):
    """
    The portal's page with its stylesheet and script inside it.

    Args:
        css: the stylesheet.
        script: the script.

    Returns:
        The page's text.
    """
    page = (SOURCES / "index.html").read_text(encoding="utf-8")
    return page.replace("/*STYLE*/", css).replace("/*SCRIPT*/", script)


def write(out, name, page):
    """
    A page and its gzipped copy into the output folder, and a line of their sizes.

    Args:
        out: the output folder.
        name: the page's file name.
        page: its text.
    """
    raw = page.encode("utf-8")
    packed = gzip.compress(raw, compresslevel=9, mtime=0)
    (out / name).write_bytes(raw)
    (out / (name + ".gz")).write_bytes(packed)
    print(f"{name:<22} {len(raw):>8,} bytes  {len(packed):>7,} gzipped")


def write_header(path, page):
    """
    The page gzipped, as a C header the firmware serves it from (ADR-008, gate 3): `web_portal_gz`, its length beside
    it. Written only when it changes, so the build compiles it again only then.

    Args:
        path: the header's path.
        page: the page's text.
    """
    packed = gzip.compress(page.encode("utf-8"), compresslevel=9, mtime=0)
    rows = [", ".join(f"0x{byte:02x}" for byte in packed[at:at + 16]) for at in range(0, len(packed), 16)]
    text = ("// Made by scripts/web_build.py from src/web (ADR-008, gate 3): the portal, gzipped. Do not edit\n"
            "#ifndef WEB_PORTAL_GZ_H_\n#define WEB_PORTAL_GZ_H_\n\n#include <stddef.h>\n#include <stdint.h>\n\n"
            f"static const uint8_t web_portal_gz[{len(packed)}] = {{\n    " + ",\n    ".join(rows) + "\n};\n"
            f"static const size_t web_portal_gz_len = {len(packed)};\n\n#endif  // WEB_PORTAL_GZ_H_\n")
    target = Path(path)
    if not target.exists() or target.read_text(encoding="utf-8") != text:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
    print(f"{target.name:<22} {len(packed):>8,} bytes gzipped, into the firmware")


def main():
    """Build the pages into the output folder, and the firmware's header when asked."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default=str(REPO / "build_web"))
    parser.add_argument("--header", help="also the portal gzipped as a C header, for the firmware's build")
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    script = "\n".join((SOURCES / name).read_text(encoding="utf-8") for name in SCRIPTS)
    plain = minify_css((SOURCES / "plain.css").read_text(encoding="utf-8"))
    print(f"{'the script':<22} {len(script.encode()):>8,} bytes  {len(gzip.compress(script.encode(), 9)):>7,} gzipped")
    print(f"{'plain CSS':<22} {len(plain.encode()):>8,} bytes  {len(gzip.compress(plain.encode(), 9)):>7,} gzipped")
    page = join(plain, script)
    write(out, "index.html", page)
    if args.header:
        write_header(args.header, page)


if __name__ == "__main__":
    main()
