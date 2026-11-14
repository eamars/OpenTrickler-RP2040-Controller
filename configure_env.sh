#!/usr/bin/env bash
# Puts the toolchain that setup_toolchain.sh fetched on the path, on Linux, which is what CI runs. Source it
# before cmake:
#   source ./configure_env.sh
# The versions are in toolchain.env (ADR-011). It sets no shell options: it runs in the caller's shell.
# macOS is parked (owner, 2026-11-14): its scripts as last used are in git, `git show 2ad9b0a:configure_env.sh`.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/toolchain.env"
TOOLCHAIN_ROOT="${SCRIPT_DIR}/.toolchain"

# Arm's compiler, CMake, Ninja, and the ready-made picotool and pioasm, which the SDK finds on the path and does
# not build
TOOLCHAIN_DIR="${TOOLCHAIN_ROOT}/arm-gnu-toolchain-${ARM_GCC}"
if [[ -d "${TOOLCHAIN_DIR}/bin" ]]; then
    export PICO_TOOLCHAIN_PATH="${TOOLCHAIN_DIR}"
else
    echo "Warning: ${TOOLCHAIN_DIR} is missing. Run ./setup_toolchain.sh first."
fi
for TOOL_DIR in "${TOOLCHAIN_DIR}/bin" "${TOOLCHAIN_ROOT}/cmake-${CMAKE}/bin" "${TOOLCHAIN_ROOT}/ninja-${NINJA}" \
                "${TOOLCHAIN_ROOT}/picotool-${PICO_TOOLS}/picotool" "${TOOLCHAIN_ROOT}/pico-sdk-tools-${PICO_TOOLS}/pioasm"; do
    if [[ -d "${TOOL_DIR}" ]]; then
        export PATH="${TOOL_DIR}:${PATH}"
    fi
done

export PICO_SDK_PATH="${SCRIPT_DIR}/library/pico-sdk"

echo "PICO_SDK_PATH=${PICO_SDK_PATH}"
echo "PICO_TOOLCHAIN_PATH=${PICO_TOOLCHAIN_PATH:-not set}"
echo "arm-none-eabi-gcc: $(command -v arm-none-eabi-gcc 2>/dev/null || echo 'not found')"
