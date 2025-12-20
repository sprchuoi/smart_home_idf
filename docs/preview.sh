#!/bin/bash

###############################################################################
# Documentation Preview Script
# Builds and serves documentation locally for development
###############################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DOCS_DIR="$SCRIPT_DIR"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Building ESP32 Smart Home Documentation${NC}"

# Check dependencies
if ! command -v doxygen &> /dev/null; then
    echo "Warning: doxygen not found. Install with: sudo apt-get install doxygen graphviz"
fi

if ! command -v sphinx-build &> /dev/null; then
    echo "Installing Sphinx dependencies..."
    pip install -r "$DOCS_DIR/requirements.txt"
fi

# Generate Doxygen
if [ -f "$PROJECT_DIR/Doxyfile" ]; then
    echo -e "${BLUE}Generating Doxygen documentation...${NC}"
    cd "$PROJECT_DIR"
    doxygen Doxyfile
    echo -e "${GREEN}✓ Doxygen complete${NC}"
else
    echo "Warning: Doxyfile not found"
fi

# Build Sphinx
echo -e "${BLUE}Building Sphinx documentation...${NC}"
cd "$DOCS_DIR"
make clean
make html
echo -e "${GREEN}✓ Sphinx build complete${NC}"

# Serve documentation
PORT=8000
echo ""
echo -e "${GREEN}Documentation built successfully!${NC}"
echo ""
echo "View documentation at:"
echo "  Sphinx: http://localhost:$PORT/"
echo "  Doxygen: file://$PROJECT_DIR/doxygen/html/index.html"
echo ""
echo "Starting HTTP server on port $PORT..."
echo "Press Ctrl+C to stop"
echo ""

cd "$DOCS_DIR/_build/html"
python3 -m http.server $PORT
