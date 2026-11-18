"""
The web pages from their sources (ADR-008): each page's markup, its stylesheet and its script joined into one file,
gzipped, as the unit would serve it. For gate 1's styles' proposal it builds the charge's page twice, once with our
own plain CSS and once with Tailwind and daisyUI trimmed by Tailwind's standalone CLI, and prints each one's size.

    venv\\Scripts\\python scripts\\web_build.py [--out build_web] [--tailwind <cli>] [--daisyui <folder>]

Without --tailwind, only the plain CSS page is built. Only the Python standard library is used.
"""
import argparse
import gzip
import re
import shutil
import subprocess
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
    The charge's page with its stylesheet and script inside it.

    Args:
        css: the stylesheet.
        script: the script.

    Returns:
        The page's text.
    """
    page = (SOURCES / "charge.html").read_text(encoding="utf-8")
    return page.replace("/*STYLE*/", css).replace("/*SCRIPT*/", script)


def tailwind_css(cli, daisyui, work):
    """
    The daisyUI stylesheet, trimmed to the page by Tailwind's CLI.

    Args:
        cli: Tailwind's standalone CLI.
        daisyui: the folder holding daisyui.mjs and daisyui-theme.mjs.
        work: a folder to build in, emptied first.

    Returns:
        The minified stylesheet.
    """
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    for name in ("daisy.css", "charge.html", "app.js"):
        shutil.copy(SOURCES / name, work / name)
    for name in ("daisyui.mjs", "daisyui-theme.mjs"):
        shutil.copy(Path(daisyui) / name, work / name)
    out = work / "out.css"
    subprocess.run([str(cli), "-i", str(work / "daisy.css"), "-o", str(out), "--minify"], cwd=work, check=True,
                   capture_output=True)
    return out.read_text(encoding="utf-8")


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
    parser.add_argument("--tailwind", help="Tailwind's standalone CLI")
    parser.add_argument("--daisyui", help="the folder holding daisyUI's plugin files")
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    script = (SOURCES / "app.js").read_text(encoding="utf-8")
    plain = minify_css((SOURCES / "plain.css").read_text(encoding="utf-8"))
    print(f"{'the script':<22} {len(script.encode()):>8,} bytes  {len(gzip.compress(script.encode(), 9)):>7,} gzipped")
    print(f"{'plain CSS':<22} {len(plain.encode()):>8,} bytes  {len(gzip.compress(plain.encode(), 9)):>7,} gzipped")
    write(out, "charge_plain.html", join(plain, script))
    if args.tailwind:
        daisy = tailwind_css(args.tailwind, args.daisyui, out / "tailwind_work")
        print(f"{'daisyUI CSS':<22} {len(daisy.encode()):>8,} bytes  {len(gzip.compress(daisy.encode(), 9)):>7,} gzipped")
        write(out, "charge_daisy.html", join(daisy, script))


if __name__ == "__main__":
    main()
