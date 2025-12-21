#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Prevent recursive sourcing
if [ -n "$ESP_IDF_ENV_LOADED" ]; then
  echo "ESP-IDF environment already loaded."
  return 0
fi
export ESP_IDF_ENV_LOADED=1

# Set the ESP-IDF path
export IDF_PATH="$(cd "$(dirname "${(%):-%N}")" && pwd)"

# Add ESP-IDF tools to PATH
if [ -d "$IDF_PATH/tools" ]; then
  export PATH="$IDF_PATH/tools:$PATH"
fi

# Source additional environment setup if needed
if [ -f "$IDF_PATH/export.sh" ]; then
  source "$IDF_PATH/export.sh"
else
  echo "[WARNING] ESP-IDF export.sh not found. Ensure the ESP-IDF environment is properly installed."
fi

# Print confirmation
echo "ESP-IDF environment set up. IDF_PATH=$IDF_PATH"