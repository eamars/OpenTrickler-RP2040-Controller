"""The firmware's own C, compiled for this PC and run: the parts a unit is not needed for (tests/host).

Each test is a C file in tests/host with the firmware sources it needs, compiled with the host's C compiler: $CC,
else `zig cc` (on Windows the Pico extension brings zig into %USERPROFILE%\\.pico-sdk\\zig), else cc, gcc or clang.
The firmware's headers that need the Pico SDK or FreeRTOS come from tests/host/stubs. Ends with VERDICT PASS or FAIL.

    venv\\Scripts\\python tests\\host_test.py
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
HOST = REPO / "tests" / "host"
OUT = REPO / "build_host"

# Each test: its C file, and the firmware's sources and the host's own it is built with
TESTS = {
    "config_migration_test": ["src/common.c", "tests/host/fake_eeprom.c"],
}


def compiler() -> list:
    """The host's C compiler, as the command that starts it."""
    if os.environ.get("CC"):
        return os.environ["CC"].split()
    zig = shutil.which("zig") or next(iter(sorted(Path.home().glob(".pico-sdk/zig/*/zig.exe"), reverse=True)), None)
    if zig:
        return [str(zig), "cc"]
    for name in ("cc", "gcc", "clang"):
        if shutil.which(name):
            return [name]
    raise SystemExit("VERDICT FAIL no host C compiler: set CC, or install zig, gcc or clang")


def main() -> int:
    """
    Build and run each test.

    Returns:
        0 when every test passed, 1 otherwise.
    """
    cc = compiler()
    OUT.mkdir(exist_ok=True)
    failed = []
    for name, sources in TESTS.items():
        exe = OUT / (name + (".exe" if os.name == "nt" else ""))
        command = cc + ["-std=c11", "-Wall", "-O1", "-I", str(HOST / "stubs"), "-I", str(HOST), "-I", str(REPO / "src"),
                        str(HOST / (name + ".c"))] + [str(REPO / source) for source in sources] + ["-o", str(exe), "-lm"]
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout + build.stderr)
            failed.append(f"{name} did not build")
            continue
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout.rstrip())
        if run.returncode != 0:
            failed.append(name)
    print("VERDICT PASS" if not failed else "VERDICT FAIL " + "; ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
