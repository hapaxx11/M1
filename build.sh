#!/bin/sh
set -e

BUILD_DIR="build"
OUTPUT_DIR="artifacts"
FW_NAME="M1_SiN360_v0.9.0.3"
mkdir -p $BUILD_DIR $OUTPUT_DIR

echo "Configuring CMake..."
cmake -G Ninja -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "Compiling..."
cmake --build $BUILD_DIR --parallel $(nproc)

# Find the raw bin (without CRC)
RAW_BIN=$(ls $BUILD_DIR/MonstaTek*.bin 2>/dev/null | grep -v wCRC | head -1)
if [ -z "$RAW_BIN" ]; then
    echo "Error: No raw .bin found in $BUILD_DIR"
    exit 1
fi

echo "Appending STM32 CRC32..."
python3 append_crc.py "$RAW_BIN" "$OUTPUT_DIR/${FW_NAME}.bin"

# Copy ELF for debugging
cp $BUILD_DIR/*.elf "$OUTPUT_DIR/${FW_NAME}.elf" 2>/dev/null || true

# Verify
ELF_FILE="$OUTPUT_DIR/${FW_NAME}.elf"
if [ ! -f "$ELF_FILE" ]; then
    ELF_FILE=$(ls $OUTPUT_DIR/*.elf 2>/dev/null | head -1)
fi
if [ -z "$ELF_FILE" ]; then
    echo "Error: No ELF file found"
    exit 1
fi

ARCH=$(arm-none-eabi-objdump -f "$ELF_FILE" 2>/dev/null | grep "architecture" | head -1)
if echo "$ARCH" | grep -q "armv8-m\|armv7-m"; then
    echo "Build verification: Valid ARM firmware"
    arm-none-eabi-size "$ELF_FILE"
else
    echo "Error: Unexpected architecture in ELF"
    exit 1
fi

echo ""
echo "=== SD Card Update File ==="
ls -la "$OUTPUT_DIR/${FW_NAME}.bin"
echo ""
echo "Copy ${FW_NAME}.bin to your M1 SD card and update via Settings > Firmware Update"
echo "Success!"