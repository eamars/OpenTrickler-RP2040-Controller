"""
A release's files and its manifest (ADR-004): the folder CI drafts a release from, and rc_run.py update reads.

Each file's name starts with the version, as the unit shows it (OpenTrickler-2.0.0-...): each board's full install
(<board>_full_install.uf2, the first install by USB and the way back), each update image (<name>_update.bin, named
as the unit names the image it takes: GET /api/v1/update, `takes`: pico2_w, pico_w_a, pico_w_b). Beside them, under the same names in every release so that
GitHub's releases/latest/download/ always leads to the latest's: manifest.json and SHA256SUMS. The offer (ADR-008,
ADR-010) and, until then, rc_run.py update read the manifest; a unit finds its image by `takes`, not by a file's name.
The release page's text is written by hand (docs/releases/).
"""
import argparse
import hashlib
import json
import shutil
import struct
import sys
from pathlib import Path

from gen_version import block_text

MANIFEST_VERSION = 1
PICOBIN_BLOCK_START = 0xFFFFDED3
FILE_PREFIX = "OpenTrickler"


def named_file(text):
    """
    A file given as "name=path".

    Args:
        text: the argument as given.

    Returns:
        (name, path).
    """
    name, _, path = text.partition("=")
    if not name or not path:
        raise argparse.ArgumentTypeError(f"{text}: give name=path")
    return name, Path(path)


def block_version(data):
    """
    An update image's block version (ADR-004), as the unit gives its own in GET /api/v1/update (`running.block`):
    276.210 is "1.20.210". Read as src/platform/rp2/picobin_image.c reads it: the first block within the first 4 KB, then the
    block loop, the last image definition's version. The unit decides "newer" by this, before it fetches the image.

    Args:
        data: the image's bytes.

    Returns:
        The text, or None when the image has no version in a block.
    """
    first = next((at for at in range(0, min(len(data), 0x1000) - 3, 4)
                  if struct.unpack_from("<I", data, at)[0] == PICOBIN_BLOCK_START), None)
    if first is None:
        return None
    version, at = None, first
    for _ in range(8):
        index = at + 4
        image_def, found = False, None
        while index + 8 <= len(data):
            header = struct.unpack_from("<I", data, index)[0]
            kind = header & 0xFF
            words = (header >> 8) & (0xFFFF if kind & 0x80 else 0xFF)
            if kind == 0xFF:    # the last item: then the link to the next block
                at += struct.unpack_from("<i", data, index + 4)[0]
                break
            if kind == 0x42 and (header >> 16) & 0xF == 1:     # an executable's image type
                image_def = True
            if kind == 0x48:
                low, high = struct.unpack_from("<2H", data, index + 4)
                found = block_text(high, low)
            index += max(words, 1) * 4
        else:
            return None
        if image_def:
            version = found
        if at == first:
            return version
    return None


def entry(source, out_dir, file_name, url_base):
    """
    A file copied into the release, and its entry.

    Args:
        source: the file built.
        out_dir: the release's folder.
        file_name: its name there.
        url_base: where the release's files will be (GitHub's releases/download/<tag>), or None.

    Returns:
        {"file", "size", "sha256"}, and "url", the file's address, when the release has one.
    """
    data = source.read_bytes()
    shutil.copyfile(source, out_dir / file_name)
    item = {"file": file_name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if url_base:
        item["url"] = f"{url_base.rstrip('/')}/{file_name}"
    return item


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--version", required=True, help="the release's version, as the firmware gives it")
    parser.add_argument("--note", default="", help="the release's note, as the offer shows it")
    parser.add_argument("--importance", choices=["normal", "critical"], default="normal")
    parser.add_argument("--url-base", help="where the release's files will be: each entry is given its address")
    parser.add_argument("--image", action="append", type=named_file, default=[], help="name=app.bin, an update image")
    parser.add_argument("--full-install", action="append", type=named_file, default=[],
                        help="board=full_install.uf2")
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    prefix = f"{FILE_PREFIX}-{args.version}-"
    images = {}
    for name, path in args.image:
        images[name] = entry(path, args.out_dir, f"{prefix}{name}_update.bin", args.url_base)
        images[name]["block"] = block_version(path.read_bytes())
        if images[name]["block"] is None:
            sys.exit(f"{path}: no version in its metadata block; an update image needs one")
    manifest = {
        "manifest_version": MANIFEST_VERSION,
        "version": args.version,
        "note": args.note,
        "importance": args.importance,
        "images": images,
        "full_installs": {board: entry(path, args.out_dir, f"{prefix}{board}_full_install.uf2", args.url_base)
                          for board, path in args.full_install},
    }
    # LF on every system: so the sums check with sha256sum, and a PC makes the same files CI does
    (args.out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")

    files = [(kind, item) for kind in ("full_installs", "images") for item in manifest[kind].values()]
    sums = "".join(f"{item['sha256']}  {item['file']}\n" for _, item in files)
    manifest_sha = hashlib.sha256((args.out_dir / "manifest.json").read_bytes()).hexdigest()
    (args.out_dir / "SHA256SUMS").write_text(sums + f"{manifest_sha}  manifest.json\n", encoding="utf-8", newline="\n")
    print(f"{args.out_dir}: {', '.join(item['file'] for _, item in files)}, manifest.json, SHA256SUMS")


if __name__ == "__main__":
    sys.exit(main())
