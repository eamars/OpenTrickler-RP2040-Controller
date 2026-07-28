#!/usr/bin/env bash
# Download and extract the ARM GNU toolchain for macOS (Apple Silicon).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN_ROOT="${SCRIPT_DIR}/.toolchain"
ARCHIVE="${TOOLCHAIN_ROOT}/arm-gnu-toolchain.tar.xz"
URL="https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.tar.xz"
EXTRACTED="${TOOLCHAIN_ROOT}/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi"

mkdir -p "${TOOLCHAIN_ROOT}"

if [[ -x "${EXTRACTED}/bin/arm-none-eabi-gcc" ]]; then
    echo "Toolchain already installed at ${EXTRACTED}"
    exit 0
fi

echo "Downloading ARM GNU Toolchain..."
curl -L -o "${ARCHIVE}" "${URL}"
echo "Extracting..."
tar -xf "${ARCHIVE}" -C "${TOOLCHAIN_ROOT}"
echo "Done. Source ./configure_env.sh before building."
