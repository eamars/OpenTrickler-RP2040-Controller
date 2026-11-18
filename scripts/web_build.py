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
    The Trickler's page with its stylesheet and script inside it.

    Args:
        css: the stylesheet.
        script: the script.

    Returns:
        The page's text.
    """
    page = (SOURCES / "trickler.html").read_text(encoding="utf-8")
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


def main():
    """Build the pages into the output folder."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default=str(REPO / "build_web"))
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    script = (SOURCES / "app.js").read_text(encoding="utf-8")
    plain = minify_css((SOURCES / "plain.css").read_text(encoding="utf-8"))
    print(f"{'the script':<22} {len(script.encode()):>8,} bytes  {len(gzip.compress(script.encode(), 9)):>7,} gzipped")
    print(f"{'plain CSS':<22} {len(plain.encode()):>8,} bytes  {len(gzip.compress(plain.encode(), 9)):>7,} gzipped")
    write(out, "trickler.html", join(plain, script))


if __name__ == "__main__":
    main()
