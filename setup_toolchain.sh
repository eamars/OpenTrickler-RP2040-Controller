#!/usr/bin/env bash
# Fetches the toolchain this repository builds with into .toolchain/, on Linux on x86-64, which is what CI runs:
# Arm's compiler, CMake, Ninja and the ready-made pioasm and picotool, the same set setup_toolchain.ps1 fetches
# on Windows. The versions are in toolchain.env (ADR-011). A part already there is left alone.
# macOS is parked (owner, 2026-11-14): its scripts as last used are in git, `git show 2ad9b0a:setup_toolchain.sh`.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/toolchain.env"
ROOT="${SCRIPT_DIR}/.toolchain"
TOOLS_URL="https://github.com/raspberrypi/pico-sdk-tools/releases/download/${PICO_SDK_TOOLS_RELEASE}"

if [[ "$(uname -s)-$(uname -m)" != Linux-x86_64 ]]; then
    echo "This script is for Linux on x86-64; Windows has setup_toolchain.ps1. Found $(uname -s) $(uname -m)." >&2
    exit 1
fi

# fetch <name> <url> <folder under .toolchain> <a file that shows the part is there, under that folder>
fetch() {
    local name="$1" url="$2" dest="${ROOT}/$3" probe="$4" work source
    if [[ -e "${dest}/${probe}" ]]; then
        echo "Have    ${name} in ${dest}"
        return
    fi
    echo "Fetch   ${name}"
    echo "        from ${url}"
    work="$(mktemp -d)"
    curl --fail --location --retry 3 --silent --show-error --output "${work}/archive" "${url}"
    mkdir -p "${work}/unpacked"
    case "${url}" in
        *.zip) unzip -q "${work}/archive" -d "${work}/unpacked" ;;
        *) tar -xf "${work}/archive" -C "${work}/unpacked" ;;
    esac
    # Some archives hold their files directly and some wrap them in one folder: the probe decides
    source="${work}/unpacked"
    if [[ ! -e "${source}/${probe}" ]]; then
        source="$(find "${work}/unpacked" -mindepth 1 -maxdepth 1 -type d | head -1)"
    fi
    if [[ -z "${source}" || ! -e "${source}/${probe}" ]]; then
        echo "${probe} is not in the archive of ${name}" >&2
        rm -rf "${work}"
        exit 1
    fi
    mkdir -p "${dest}"
    cp -R "${source}/." "${dest}/"
    rm -rf "${work}"
    echo "Done    ${name}"
}

mkdir -p "${ROOT}"
fetch "Arm GNU toolchain ${ARM_GCC}" \
    "https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_GCC}/binrel/arm-gnu-toolchain-${ARM_GCC}-x86_64-arm-none-eabi.tar.xz" \
    "arm-gnu-toolchain-${ARM_GCC}" bin/arm-none-eabi-gcc
fetch "CMake ${CMAKE}" \
    "https://github.com/Kitware/CMake/releases/download/v${CMAKE}/cmake-${CMAKE}-linux-x86_64.tar.gz" \
    "cmake-${CMAKE}" bin/cmake
fetch "Ninja ${NINJA}" \
    "https://github.com/ninja-build/ninja/releases/download/v${NINJA}/ninja-linux.zip" \
    "ninja-${NINJA}" ninja
fetch "picotool ${PICO_TOOLS}" "${TOOLS_URL}/picotool-${PICO_TOOLS}-x86_64-lin.tar.gz" \
    "picotool-${PICO_TOOLS}" picotool/picotool
fetch "pioasm ${PICO_TOOLS}" "${TOOLS_URL}/pico-sdk-tools-${PICO_TOOLS}-x86_64-lin.tar.gz" \
    "pico-sdk-tools-${PICO_TOOLS}" pioasm/pioasm

echo "Source ./configure_env.sh before building."
