"""
A release, built (ADR-004, gate 3): the five builds a release takes, then its folder (scripts/release_manifest.py).
CI runs it on every build and drafts a release from its folder on a version tag; a PC runs it the same way, after
configure_env.ps1 or configure_env.sh. With --tag it makes a tag's release, as CI does, and refuses one whose firmware
does not report the tag's version or whose notes are missing (docs/releases/README.md).

The builds, each in build_release/<name>:
  pico2_w, pico_w             the full installs (the Pico W's slot A), not marked: they boot at once
  pico2_w_update              the Pico 2 W's update image: one binary for either slot
  pico_w_a_update, _b_update  the Pico W's, one linked for each slot
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RELEASE_NOTES = REPO / "docs" / "releases"
NOTE_MAX_BYTES = 1023   # the offer's note, as the Open Weapon Mount Computer's
BUILDS = [
    ("pico2_w", "pico2_w", []),
    ("pico_w", "pico_w", ["-DAPP_SLOT=A"]),
    ("pico2_w_update", "pico2_w", ["-DAPP_TRIAL_IMAGE=ON"]),
    ("pico_w_a_update", "pico_w", ["-DAPP_SLOT=A", "-DAPP_TRIAL_IMAGE=ON"]),
    ("pico_w_b_update", "pico_w", ["-DAPP_SLOT=B", "-DAPP_TRIAL_IMAGE=ON"]),
]


def build(build_dir, board, flags, build_type):
    """
    One build, configured afresh so the image's block carries this commit's version (src/boards/common/flash_layout.cmake).

    Args:
        build_dir: its folder.
        board: PICO_BOARD.
        flags: CMake definitions beyond the board and the build type.
        build_type: CMAKE_BUILD_TYPE.
    """
    subprocess.run(["cmake", "-B", str(build_dir), "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={build_type}",
                    f"-DPICO_BOARD={board}", *flags], cwd=REPO, check=True)
    subprocess.run(["cmake", "--build", str(build_dir)], cwd=REPO, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out-dir", type=Path, default=REPO / "release", help="the release's folder")
    parser.add_argument("--builds", type=Path, default=REPO / "build_release", help="where the builds go")
    parser.add_argument("--build-type", default="Debug")
    parser.add_argument("--tag", help="a release's tag, vX.Y.Z: its notes and version are checked, its note taken "
                                      "from docs/releases/<tag>_note.txt, and its files given their addresses")
    parser.add_argument("--repository", default="eamars/OpenTrickler-RP2040-Controller",
                        help="the GitHub repository a tag's release is in (owner/name)")
    args = parser.parse_args()

    note, url_base = None, None
    if args.tag:
        for needed in (RELEASE_NOTES / f"{args.tag}.md", RELEASE_NOTES / f"{args.tag}_note.txt"):
            if not needed.exists():
                sys.exit(f"{needed.relative_to(REPO)} is missing: a release's notes are written before its tag")
        tagged = subprocess.run([sys.executable, str(REPO / "scripts" / "gen_version.py"), "--version-string"],
                                cwd=REPO, check=True, capture_output=True, text=True).stdout.strip()
        if f"v{tagged}" != args.tag:
            sys.exit(f"the firmware reports {tagged}, not the tag {args.tag}")
        note = (RELEASE_NOTES / f"{args.tag}_note.txt").read_text(encoding="utf-8").strip()
        if len(note.encode("utf-8")) > NOTE_MAX_BYTES:
            sys.exit(f"{args.tag}_note.txt is longer than {NOTE_MAX_BYTES} bytes")
        url_base = f"https://github.com/{args.repository}/releases/download/{args.tag}"

    for name, board, flags in BUILDS:
        print(f"== {name}: {board} {' '.join(flags)}", flush=True)
        build(args.builds / name, board, flags, args.build_type)

    version = re.search(r'version_string = "([^"]*)"',
                        (REPO / "src" / "generated" / "version.c").read_text(encoding="utf-8")).group(1)
    if args.tag and f"v{version}" != args.tag:
        sys.exit(f"the firmware built reports {version}, not the tag {args.tag}")
    b = args.builds
    subprocess.run([sys.executable, str(REPO / "scripts" / "release_manifest.py"), "--out-dir", str(args.out_dir),
                    "--version", version, "--note", note if note is not None else f"Build {version}.",
                    *(["--url-base", url_base] if url_base else []),
                    "--full-install", f"pico2_w={b / 'pico2_w' / 'full_install.uf2'}",
                    "--full-install", f"pico_w={b / 'pico_w' / 'full_install.uf2'}",
                    "--image", f"pico2_w={b / 'pico2_w_update' / 'app.bin'}",
                    "--image", f"pico_w_a={b / 'pico_w_a_update' / 'app.bin'}",
                    "--image", f"pico_w_b={b / 'pico_w_b_update' / 'app.bin'}"], check=True)


if __name__ == "__main__":
    sys.exit(main())
