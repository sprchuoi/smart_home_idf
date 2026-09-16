#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Load ESP-IDF environment
if [ -z "$IDF_PATH" ]; then
  echo "ESP-IDF environment not set. Please source the export.sh script."
  exit 1
fi

# Define the ELF file to debug
ELF_FILE="build/smart_home.elf"

if [ ! -f "$ELF_FILE" ]; then
  echo "ELF file not found: $ELF_FILE"
  echo "Make sure the project is built before debugging."
  exit 1
fi

# Resolve GDB for the *configured* target rather than assuming classic ESP32.
# This script previously hardcoded xtensa-esp32-elf-gdb, which cannot debug an
# ESP32-S3 -- it would either fail to find the binary or silently attach with
# the wrong register and memory layout.
TARGET="$(sed -n 's/^CONFIG_IDF_TARGET="\(.*\)"/\1/p' sdkconfig | head -1)"
if [ -z "$TARGET" ]; then
  echo "Cannot determine the target from sdkconfig. Run 'idf.py build' first."
  exit 1
fi

GDB="xtensa-${TARGET}-elf-gdb"
if ! command -v "$GDB" > /dev/null 2>&1; then
  echo "$GDB is not on PATH."
  echo "Source \$IDF_PATH/export.sh, or just use 'idf.py gdb'."
  exit 1
fi

echo "Target:  $TARGET"
echo "Debugger: $GDB"
echo "Expecting a GDB stub on :3333 -- run 'idf.py openocd' in another terminal."
echo

"$GDB" "$ELF_FILE" \
  -ex "target remote :3333" \
  -ex "monitor reset halt" \
  -ex "flushregs" \
  -ex "thb app_main" \
  -ex "continue"
