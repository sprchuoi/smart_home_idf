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

# Launch GDB for ESP32
xtensa-esp32-elf-gdb "$ELF_FILE" \
  -ex "target remote :3333" \
  -ex "monitor reset halt" \
  -ex "flushregs" \
  -ex "thb app_main" \
  -ex "continue"