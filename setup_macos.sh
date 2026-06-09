#!/bin/bash
set -e

echo "--- Preparing macOS Build Environment ---"

DEPENDS=(cmake ninja git srecord)

if command -v brew >/dev/null 2>&1; then
    echo "--- Installing dependencies from Homebrew ---"
    brew install "${DEPENDS[@]}"
    brew install --cask gcc-arm-embedded
fi

echo "--- Dependency Setup complete. Rerun make ---"
