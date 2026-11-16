"""
A release, built (ADR-004, gate 3): the five builds a release takes, then its folder (scripts/release_manifest.py).
CI runs it on every build and drafts a release from its folder on a version tag; a PC runs it the same way, after
configure_env.ps1 or configure_env.sh.

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
BUILDS = [
    ("pico2_w", "pico2_w", []),
    ("pico_w", "pico_w", ["-DAPP_SLOT=A"]),
    ("pico2_w_update", "pico2_w", ["-DAPP_TRIAL_IMAGE=ON"]),
    ("pico_w_a_update", "pico_w", ["-DAPP_SLOT=A", "-DAPP_TRIAL_IMAGE=ON"]),
    ("pico_w_b_update", "pico_w", ["-DAPP_SLOT=B", "-DAPP_TRIAL_IMAGE=ON"]),
]


def build(build_dir, board, flags, build_type):
    """
    One build, configured afresh so the image's block carries this commit's version (targets/flash_layout.cmake).

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
    parser.add_argument("--note", default="", help="the release's note, as the offer shows it")
    args = parser.parse_args()

    for name, board, flags in BUILDS:
        print(f"== {name}: {board} {' '.join(flags)}", flush=True)
        build(args.builds / name, board, flags, args.build_type)

    version = re.search(r'version_string = "([^"]*)"',
                        (REPO / "src" / "generated" / "version.c").read_text(encoding="utf-8")).group(1)
    b = args.builds
    subprocess.run([sys.executable, str(REPO / "scripts" / "release_manifest.py"), "--out-dir", str(args.out_dir),
                    "--version", version, "--note", args.note,
                    "--full-install", f"pico2_w={b / 'pico2_w' / 'full_install.uf2'}",
                    "--full-install", f"pico_w={b / 'pico_w' / 'full_install.uf2'}",
                    "--image", f"pico2_w={b / 'pico2_w_update' / 'app.bin'}",
                    "--image", f"pico_w_a={b / 'pico_w_a_update' / 'app.bin'}",
                    "--image", f"pico_w_b={b / 'pico_w_b_update' / 'app.bin'}",
                    "--erase", f"pico2_w={b / 'pico2_w' / 'erase.uf2'}",
                    "--erase", f"pico_w={b / 'pico_w' / 'erase.uf2'}"], check=True)


if __name__ == "__main__":
    sys.exit(main())
