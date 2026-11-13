#!/usr/bin/env bash
# Configure build environment for macOS/Linux.
# Source this script before running cmake:
#   source ./configure_env.sh

set -euo pipefail

if [[ -n "${BASH_SOURCE:-}" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fi

# Prefer the bundled ARM GNU toolchain (includes newlib).
# Homebrew's arm-none-eabi-gcc alone is incomplete for Pico SDK builds.
TOOLCHAIN_DIR="${SCRIPT_DIR}/.toolchain/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi"
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

export PICO_SDK_PATH="${SCRIPT_DIR}/library/pico-sdk"

echo "PICO_SDK_PATH=${PICO_SDK_PATH}"
echo "PICO_TOOLCHAIN_PATH=${PICO_TOOLCHAIN_PATH:-not set}"
echo "arm-none-eabi-gcc: $(command -v arm-none-eabi-gcc 2>/dev/null || echo 'not found')"
