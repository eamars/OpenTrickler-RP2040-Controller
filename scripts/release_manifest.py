"""
A release's files and its manifest (ADR-004): the folder CI drafts a release from, and rc_run.py update reads.

Beside manifest.json: each board's full install (<board>_full_install.uf2, the first install by USB and the way
back), each update image (<name>_update.bin, named as the unit names the image it takes: GET /api/v1/update, `takes`:
pico2_w, pico_w_a, pico_w_b), each board's erase (<board>_erase.uf2, for a release from before ADR-004), SHA256SUMS,
and RELEASE_NOTES.md, the draft release's text. The offer (ADR-008, ADR-010) and, until then, rc_run.py update read
the manifest.
"""
import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

MANIFEST_VERSION = 1
NOTES = """OpenTrickler firmware {version}

{note}

**A first install, or from v1.20 and before:** hold BOOTSEL, plug the unit in by USB, and drag the board's `*_full_install.uf2` onto the drive (manuals/firmware_update_via_usb.md). Settings stay in the EEPROM.

**An update over Wi-Fi** is through the REST API for now, for debugging (`/api/v1/update`, the update image the unit names); it is offered on the web page and the screen once those come (ADR-008, ADR-010).

**Going back to an older release** is a full erase: manuals/firmware_downgrade.md.

| File | What | SHA-256 |
|---|---|---|
{rows}
"""


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
    A file copied into the release, and its entry.

    Args:
        source: the file built.
        out_dir: the release's folder.
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
    parser.add_argument("--erase", action="append", type=named_file, default=[], help="board=erase.uf2")
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
        "erases": {board: entry(path, args.out_dir, f"{board}_erase.uf2") for board, path in args.erase},
    }
    # LF on every system: so the sums check with sha256sum, and a PC makes the same files CI does
    (args.out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")

    files = [(kind, item) for kind in ("full_installs", "images", "erases") for item in manifest[kind].values()]
    sums = "".join(f"{item['sha256']}  {item['file']}\n" for _, item in files)
    manifest_sha = hashlib.sha256((args.out_dir / "manifest.json").read_bytes()).hexdigest()
    (args.out_dir / "SHA256SUMS").write_text(sums + f"{manifest_sha}  manifest.json\n", encoding="utf-8", newline="\n")
    what = {"full_installs": "the full install, by USB", "images": "the update image, over Wi-Fi",
            "erases": "the erase, by USB, before a release from v1.20 and before"}
    rows = "\n".join(f"| `{item['file']}` | {what[kind]} | `{item['sha256']}` |" for kind, item in files)
    (args.out_dir / "RELEASE_NOTES.md").write_text(
        NOTES.format(version=args.version, note=args.note or "(the release's note)", rows=rows), encoding="utf-8",
        newline="\n")
    print(f"{args.out_dir}: {', '.join(item['file'] for _, item in files)}, manifest.json, SHA256SUMS")


if __name__ == "__main__":
    sys.exit(main())
