#!/usr/bin/env bash
# Puts the toolchain that setup_toolchain.sh fetched on the path, on macOS and Linux. Source it before cmake:
#   source ./configure_env.sh
# The versions are in toolchain.env (ADR-011). It sets no shell options: it runs in the caller's shell.

if [[ -n "${BASH_SOURCE:-}" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fi
source "${SCRIPT_DIR}/toolchain.env"
TOOLCHAIN_ROOT="${SCRIPT_DIR}/.toolchain"

# Arm's compiler, with newlib. Homebrew's arm-none-eabi-gcc alone is incomplete for Pico SDK builds.
TOOLCHAIN_DIR="${TOOLCHAIN_ROOT}/arm-gnu-toolchain-${ARM_GCC}"
if [[ -d "${TOOLCHAIN_DIR}/bin" ]]; then
    export PICO_TOOLCHAIN_PATH="${TOOLCHAIN_DIR}"
    export PATH="${TOOLCHAIN_DIR}/bin:${PATH}"
elif [[ -d "/Applications/ArmGNUToolchain" ]]; then
    # Fallback: brew cask gcc-arm-embedded install location
    ARM_TOOLCHAIN="$(find /Applications/ArmGNUToolchain -maxdepth 3 -type d -name bin | head -1 | xargs dirname)"
    export PICO_TOOLCHAIN_PATH="${ARM_TOOLCHAIN}"
    export PATH="${ARM_TOOLCHAIN}/bin:${PATH}"
else
    echo "Warning: No complete ARM toolchain found."
    echo "Install one of:"
    echo "  1) Run: ./setup_toolchain.sh"
    echo "  2) brew install --cask gcc-arm-embedded"
fi

# On Linux, setup_toolchain.sh also fetches CMake, Ninja and the ready-made pioasm and picotool; the SDK finds
# the last two on the path and does not build them
for TOOL_DIR in "${TOOLCHAIN_ROOT}/cmake-${CMAKE}/bin" "${TOOLCHAIN_ROOT}/ninja-${NINJA}" \
                "${TOOLCHAIN_ROOT}/picotool-${PICO_TOOLS}/picotool" "${TOOLCHAIN_ROOT}/pico-sdk-tools-${PICO_TOOLS}/pioasm"; do
    if [[ -d "${TOOL_DIR}" ]]; then
        export PATH="${TOOL_DIR}:${PATH}"
    fi
done

export PICO_SDK_PATH="${SCRIPT_DIR}/library/pico-sdk"

echo "PICO_SDK_PATH=${PICO_SDK_PATH}"
echo "PICO_TOOLCHAIN_PATH=${PICO_TOOLCHAIN_PATH:-not set}"
echo "arm-none-eabi-gcc: $(command -v arm-none-eabi-gcc 2>/dev/null || echo 'not found')"
