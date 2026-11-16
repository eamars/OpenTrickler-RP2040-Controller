"""
A release's manifest (ADR-004): manifest.json beside the release's files, which the offer (ADR-008, ADR-010) and,
until then, rc_run.py update read. Each update image is named as the unit names the image it takes (GET
/api/v1/update, `takes`): pico2_w, pico_w_a, pico_w_b. The files are copied beside the manifest, each with its size
and SHA-256.
"""
import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

MANIFEST_VERSION = 1


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


def entry(source, out_dir, file_name):
    """
    A file copied beside the manifest, and its entry.

    Args:
        source: the file built.
        out_dir: the manifest's folder.
        file_name: its name there.

    Returns:
        {"file", "size", "sha256"}.
    """
    data = source.read_bytes()
    shutil.copyfile(source, out_dir / file_name)
    return {"file": file_name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--version", required=True, help="the release's version, as the firmware gives it")
    parser.add_argument("--note", default="", help="the release's note, as the offer shows it")
    parser.add_argument("--importance", choices=["normal", "critical"], default="normal")
    parser.add_argument("--image", action="append", type=named_file, default=[], help="name=app.bin, an update image")
    parser.add_argument("--full-install", action="append", type=named_file, default=[],
                        help="board=full_install.uf2")
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "manifest_version": MANIFEST_VERSION,
        "version": args.version,
        "note": args.note,
        "importance": args.importance,
        "images": {name: entry(path, args.out_dir, f"{name}_update.bin") for name, path in args.image},
        "full_installs": {board: entry(path, args.out_dir, f"{board}_full_install.uf2")
                          for board, path in args.full_install},
    }
    (args.out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"{args.out_dir / 'manifest.json'}: {', '.join(manifest['images']) or 'no images'}")


if __name__ == "__main__":
    sys.exit(main())
