"""The firmware's own C, built for this PC and run: the parts a unit is not needed for (tests/host).

tests/host is the PC build (ADR-006, gate 4): a CMake project that takes the firmware's modules by their own build
files, with stand-ins for the Pico SDK's and FreeRTOS's headers, and makes each test from them. This configures it
into build_host/, builds it whole (so a module that stopped being free of the chip fails here), runs each test and
ends with VERDICT PASS or FAIL.

    venv\\Scripts\\python tests\\host_test.py

- The compiler: $CC, else `zig cc` (on Windows the Pico extension brings zig into %USERPROFILE%\\.pico-sdk\\zig), else
  cc, gcc or clang.
- CMake and Ninja: those on the path, else the ones toolchain.env names, where setup_toolchain fetched them.
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
HOST = REPO / "tests" / "host"
OUT = REPO / "build_host"
# The versions, from toolchain.env (ADR-011): one NAME=value a line
VERSIONS = dict(re.findall(r"^([A-Z_]+)=(\S+)", (REPO / "toolchain.env").read_text(encoding="utf-8"), re.M))

# The tests that are programs: each a target of tests/host/CMakeLists.txt, run with no argument
TESTS = ("config_migration_test",)


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


def tool(name: str) -> str:
    """
    Find CMake or Ninja.

    Args:
        name: "cmake" or "ninja".

    Returns:
        The program's path: the one on the path, else the version toolchain.env names where setup_toolchain.ps1
        (%USERPROFILE%\\.pico-sdk) or setup_toolchain.sh (.toolchain/ in the repository) fetched it.
    """
    found = shutil.which(name)
    if found:
        return found
    version = VERSIONS[name.upper()]
    inner = "bin/" if name == "cmake" else ""
    for candidate in (Path.home() / ".pico-sdk" / name / f"v{version}" / f"{inner}{name}.exe",
                      REPO / ".toolchain" / f"{name}-{version}" / f"{inner}{name}"):
        if candidate.is_file():
            return str(candidate)
    raise SystemExit(f"VERDICT FAIL {name} {version} not found: run setup_toolchain first")


def host_build(*targets: str) -> str:
    """
    Build the PC build, configuring it first when it has not been.

    Args:
        targets: the targets to build; none for all of it.

    Returns:
        "" when it built, else the build's output.
    """
    cmake = tool("cmake")
    if not (OUT / "build.ninja").is_file():
        # CMake takes a compiler of two words (`zig cc`) only from CC
        env = dict(os.environ, CC=" ".join(part.replace("\\", "/") for part in compiler()))
        made = subprocess.run([cmake, "-S", str(HOST), "-B", str(OUT), "-G", "Ninja",
                               f"-DCMAKE_MAKE_PROGRAM={Path(tool('ninja')).as_posix()}"],
                              capture_output=True, text=True, env=env)
        if made.returncode != 0:
            shutil.rmtree(OUT / "CMakeFiles", ignore_errors=True)       # so the next run configures again
            (OUT / "CMakeCache.txt").unlink(missing_ok=True)
            return made.stdout + made.stderr
    command = [cmake, "--build", str(OUT)] + (["--target", *targets] if targets else [])
    built = subprocess.run(command, capture_output=True, text=True)
    return "" if built.returncode == 0 else built.stdout + built.stderr


def library_path(name: str) -> Path:
    """Where the PC build puts a test's library (a target of tests/host/CMakeLists.txt) for Python to load."""
    return OUT / "bin" / (name + (".dll" if os.name == "nt" else ".so"))


def main() -> int:
    """
    Build the PC build and run each test.

    Returns:
        0 when it built and every test passed, 1 otherwise.
    """
    problem = host_build()
    if problem:
        print(problem)
        print("VERDICT FAIL the PC build did not build")
        return 1
    failed = []
    for name in TESTS:
        run = subprocess.run([str(OUT / "bin" / (name + (".exe" if os.name == "nt" else "")))],
                             capture_output=True, text=True)
        print(run.stdout.rstrip())
        if run.returncode != 0:
            failed.append(name)
    print("VERDICT PASS" if not failed else "VERDICT FAIL " + "; ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
