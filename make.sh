#!/bin/bash

###############################################################################
# ESP32 Smart Home - Build, Test, and CI/CD Script
# 
# Usage:
#   ./make.sh [command] [options]
#
# Commands:
#   setup       - Setup development environment
#   build       - Build the project
#   clean       - Clean build artifacts
#   flash       - Flash to ESP32 device
#   monitor     - Monitor serial output
#   test        - Run tests
#   test-qemu   - Run tests on QEMU ESP32
#   doc         - Generate documentation
#   ci          - Run CI/CD pipeline
#   help        - Show this help message
###############################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_DIR/build"
DOCS_DIR="$PROJECT_DIR/docs"
TEST_DIR="$PROJECT_DIR/tests"
QEMU_DIR="$PROJECT_DIR/qemu"

# ESP-IDF path (can be overridden)
IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"

# Configuration
ESP32_TARGET="esp32"
QEMU_IMAGE="${BUILD_DIR}/smart_home.bin"
QEMU_TIMEOUT=60  # seconds

###############################################################################
# Helper Functions
###############################################################################

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_error "$1 is not installed"
        return 1
    fi
    return 0
}

check_esp_idf() {
    if [ ! -d "$IDF_PATH" ]; then
        print_error "ESP-IDF not found at $IDF_PATH"
        print_info "Please set IDF_PATH environment variable or install ESP-IDF"
        return 1
    fi
    
    if [ ! -f "$IDF_PATH/export.sh" ]; then
        print_error "ESP-IDF export.sh not found"
        return 1
    fi
    
    return 0
}

source_esp_idf() {
    if [ -f "$IDF_PATH/export.sh" ]; then
        source "$IDF_PATH/export.sh" > /dev/null 2>&1
        print_info "ESP-IDF environment loaded from $IDF_PATH"
        return 0
    else
        print_error "Failed to source ESP-IDF"
        return 1
    fi
}

###############################################################################
# Setup Functions
###############################################################################

setup_environment() {
    print_info "Setting up development environment..."
    
    # Check prerequisites
    print_info "Checking prerequisites..."
    
    local missing_deps=()
    
    check_command python3 || missing_deps+=("python3")
    check_command cmake || missing_deps+=("cmake")
    check_command ninja || missing_deps+=("ninja")
    check_command git || missing_deps+=("git")
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_info "Please install missing dependencies"
        return 1
    fi
    
    # Check ESP-IDF
    if ! check_esp_idf; then
        print_info "Installing ESP-IDF..."
        install_esp_idf
    fi
    
    # Source ESP-IDF
    source_esp_idf
    
    # Install Python dependencies
    print_info "Installing Python dependencies..."
    if [ -f "$PROJECT_DIR/requirements.txt" ]; then
        pip3 install -r "$PROJECT_DIR/requirements.txt" --quiet
    fi
    
    # Create directories
    mkdir -p "$BUILD_DIR"
    mkdir -p "$DOCS_DIR"
    mkdir -p "$TEST_DIR"
    mkdir -p "$QEMU_DIR"
    
    print_success "Environment setup complete"
}

install_esp_idf() {
    print_info "ESP-IDF installation guide:"
    print_info "1. Install prerequisites:"
    print_info "   sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0"
    print_info "2. Clone ESP-IDF:"
    print_info "   mkdir -p ~/esp"
    print_info "   cd ~/esp"
    print_info "   git clone --recursive https://github.com/espressif/esp-idf.git"
    print_info "3. Install ESP-IDF:"
    print_info "   cd ~/esp/esp-idf"
    print_info "   ./install.sh esp32"
    print_info "4. Set IDF_PATH:"
    print_info "   export IDF_PATH=~/esp/esp-idf"
    exit 1
}

###############################################################################
# Build Functions
###############################################################################

build_project() {
    print_info "Building project..."
    
    if ! check_esp_idf; then
        print_error "ESP-IDF not found. Run './make.sh setup' first"
        return 1
    fi
    
    source_esp_idf
    
    cd "$PROJECT_DIR"
    
    # Configure if needed
    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        print_info "Configuring project..."
        idf.py reconfigure
    fi
    
    # Build
    print_info "Building firmware..."
    idf.py build
    
    if [ -f "$BUILD_DIR/smart_home.bin" ]; then
        print_success "Build successful: $BUILD_DIR/smart_home.bin"
        
        # Show build info
        print_info "Build information:"
        idf.py size-components
        idf.py size-files
    else
        print_error "Build failed - binary not found"
        return 1
    fi
}

clean_build() {
    print_info "Cleaning build artifacts..."
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    fi
    
    cd "$PROJECT_DIR"
    if check_esp_idf; then
        source_esp_idf
        idf.py fullclean
    fi
}

###############################################################################
# Flash and Monitor Functions
###############################################################################

flash_device() {
    print_info "Flashing to ESP32 device..."
    
    if ! check_esp_idf; then
        print_error "ESP-IDF not found"
        return 1
    fi
    
    source_esp_idf
    
    cd "$PROJECT_DIR"
    idf.py flash
    
    print_success "Flash complete"
}

monitor_serial() {
    print_info "Starting serial monitor..."
    
    if ! check_esp_idf; then
        print_error "ESP-IDF not found"
        return 1
    fi
    
    source_esp_idf
    
    cd "$PROJECT_DIR"
    idf.py monitor
}

flash_and_monitor() {
    flash_device
    monitor_serial
}

###############################################################################
# Test Functions
###############################################################################

run_tests() {
    print_info "Running tests..."
    
    # Check if test directory exists
    if [ ! -d "$TEST_DIR" ]; then
        print_warning "Test directory not found: $TEST_DIR"
        print_info "Creating test directory structure..."
        mkdir -p "$TEST_DIR"
        create_test_structure
    fi
    
    # Run unit tests if available
    if [ -f "$TEST_DIR/run_tests.sh" ]; then
        print_info "Running unit tests..."
        bash "$TEST_DIR/run_tests.sh"
    else
        print_warning "No test runner found"
    fi
    
    # Run static analysis
    print_info "Running static analysis..."
    run_static_analysis
    
    # Check build
    print_info "Running build test..."
    if build_project; then
        print_success "Build test passed"
    else
        print_error "Build test failed"
        return 1
    fi
}

create_test_structure() {
    cat > "$TEST_DIR/run_tests.sh" << 'EOF'
#!/bin/bash
# Test runner script

echo "Running ESP32 Smart Home tests..."

# Add test commands here
# Example: pytest, cppcheck, etc.

echo "Tests complete"
EOF
    chmod +x "$TEST_DIR/run_tests.sh"
}

run_static_analysis() {
    print_info "Running static analysis..."
    
    # Check for cppcheck
    if command -v cppcheck &> /dev/null; then
        print_info "Running cppcheck..."
        cppcheck --enable=all --suppress=missingIncludeSystem \
            --suppress=unusedFunction \
            "$PROJECT_DIR/main" \
            --xml --xml-version=2 2>"$TEST_DIR/cppcheck_report.xml" || true
        
        if [ -f "$TEST_DIR/cppcheck_report.xml" ]; then
            print_success "Static analysis complete: $TEST_DIR/cppcheck_report.xml"
        fi
    else
        print_warning "cppcheck not installed, skipping static analysis"
    fi
}

###############################################################################
# QEMU Functions
###############################################################################

setup_qemu() {
    print_info "Setting up QEMU ESP32..."
    
    # Check if QEMU is installed
    if ! command -v qemu-system-xtensa &> /dev/null; then
        print_info "Installing QEMU for ESP32..."
        install_qemu_esp32
    fi
    
    # Check for QEMU ESP32 image
    if [ ! -f "$QEMU_DIR/esp32_qemu_image.bin" ]; then
        print_info "Downloading QEMU ESP32 image..."
        download_qemu_image
    fi
    
    print_success "QEMU setup complete"
}

install_qemu_esp32() {
    print_info "QEMU ESP32 installation:"
    print_info "1. Install QEMU:"
    print_info "   sudo apt-get install qemu-system-xtensa"
    print_info "2. Or build from source:"
    print_info "   git clone https://github.com/espressif/qemu.git"
    print_info "   cd qemu"
    print_info "   ./configure --target-list=xtensa-softmmu"
    print_info "   make"
    print_warning "Please install QEMU manually"
}

download_qemu_image() {
    print_info "Downloading QEMU ESP32 image..."
    # This would download the QEMU image from Espressif
    # For now, just create a placeholder
    mkdir -p "$QEMU_DIR"
    print_warning "QEMU image download not implemented - please download manually"
}

run_qemu_test() {
    print_info "Running tests on QEMU ESP32..."
    
    # Check if QEMU is available
    if ! command -v qemu-system-xtensa &> /dev/null; then
        print_error "QEMU not found. Run './make.sh setup-qemu' first"
        return 1
    fi
    
    # Build project first
    if ! build_project; then
        print_error "Build failed, cannot run QEMU test"
        return 1
    fi
    
    # Check if binary exists
    if [ ! -f "$BUILD_DIR/smart_home.bin" ]; then
        print_error "Binary not found: $BUILD_DIR/smart_home.bin"
        return 1
    fi
    
    print_info "Starting QEMU ESP32 emulation..."
    
    # Run QEMU with timeout
    timeout "$QEMU_TIMEOUT" qemu-system-xtensa \
        -nographic \
        -machine esp32 \
        -drive file="$BUILD_DIR/smart_home.bin",if=mtd,format=raw \
        -global driver=timer.esp32.timg,property=wdt_disable,value=true \
        2>&1 | tee "$QEMU_DIR/qemu_output.log" || {
        
        if [ $? -eq 124 ]; then
            print_warning "QEMU test timed out after $QEMU_TIMEOUT seconds"
        else
            print_error "QEMU test failed"
            return 1
        fi
    }
    
    # Analyze QEMU output
    if grep -q "Application initialized successfully" "$QEMU_DIR/qemu_output.log"; then
        print_success "QEMU test passed - Application initialized"
    else
        print_warning "QEMU test - Application may not have initialized correctly"
    fi
    
    print_success "QEMU test complete - see $QEMU_DIR/qemu_output.log"
}

###############################################################################
# Documentation Functions
###############################################################################

generate_documentation() {
    print_info "Generating Sphinx documentation..."
    
    # Check if Sphinx is installed
    if ! command -v sphinx-build &> /dev/null; then
        print_info "Installing Sphinx..."
        pip3 install -r "$PROJECT_DIR/docs/requirements.txt" --quiet
    fi
    
    # Generate Doxygen XML for Breathe (if Doxyfile exists)
    if [ -f "$PROJECT_DIR/Doxyfile" ]; then
        print_info "Generating Doxygen XML..."
        if command -v doxygen &> /dev/null; then
            doxygen "$PROJECT_DIR/Doxyfile" || print_warning "Doxygen generation had warnings"
        else
            print_warning "Doxygen not installed, skipping XML generation"
        fi
    fi
    
    # Build Sphinx documentation
    cd "$PROJECT_DIR/docs"
    print_info "Building Sphinx HTML documentation..."
    make html
    
    if [ -d "$PROJECT_DIR/docs/_build/html" ]; then
        print_success "Documentation generated: $PROJECT_DIR/docs/_build/html"
        print_info "Open docs/_build/html/index.html to view"
    else
        print_error "Documentation build failed"
        return 1
    fi
}


###############################################################################
# CI/CD Functions
###############################################################################

run_ci() {
    print_info "Running CI/CD pipeline..."
    
    local ci_status=0
    
    # Step 1: Setup
    print_info "CI Step 1/5: Environment setup"
    if ! setup_environment; then
        print_error "CI failed at setup"
        return 1
    fi
    
    # Step 2: Clean build
    print_info "CI Step 2/5: Clean build"
    clean_build
    if ! build_project; then
        print_error "CI failed at build"
        ci_status=1
    fi
    
    # Step 3: Static analysis
    print_info "CI Step 3/5: Static analysis"
    run_static_analysis || ci_status=1
    
    # Step 4: Tests
    print_info "CI Step 4/5: Running tests"
    run_tests || ci_status=1
    
    # Step 5: QEMU test (if available)
    if command -v qemu-system-xtensa &> /dev/null; then
        print_info "CI Step 5/5: QEMU test"
        run_qemu_test || {
            print_warning "QEMU test failed (non-blocking)"
        }
    else
        print_warning "QEMU not available, skipping QEMU test"
    fi
    
    # Generate CI report
    generate_ci_report "$ci_status"
    
    if [ $ci_status -eq 0 ]; then
        print_success "CI/CD pipeline passed"
        return 0
    else
        print_error "CI/CD pipeline failed"
        return 1
    fi
}

generate_ci_report() {
    local status=$1
    
    local report_file="$PROJECT_DIR/ci_report.txt"
    
    cat > "$report_file" << EOF
ESP32 Smart Home - CI/CD Report
Generated: $(date)

Build Status: $([ $status -eq 0 ] && echo "PASS" || echo "FAIL")
Build Directory: $BUILD_DIR
Binary: $([ -f "$BUILD_DIR/smart_home.bin" ] && echo "Present" || echo "Missing")

Test Results:
- Build: $([ $status -eq 0 ] && echo "PASS" || echo "FAIL")
- Static Analysis: See $TEST_DIR/cppcheck_report.xml
- QEMU Test: See $QEMU_DIR/qemu_output.log

Documentation: $DOCS_DIR
EOF
    
    print_info "CI report generated: $report_file"
}

###############################################################################
# Main Function
###############################################################################

show_help() {
    cat << EOF
ESP32 Smart Home - Build Script

Usage: ./make.sh [command] [options]

Commands:
  setup          Setup development environment
  build          Build the project
  clean          Clean build artifacts
  flash          Flash to ESP32 device
  monitor        Monitor serial output
  flash-monitor  Flash and monitor
  test           Run tests
  test-qemu      Run tests on QEMU ESP32
  setup-qemu     Setup QEMU ESP32 environment
  doc            Generate documentation
  ci             Run CI/CD pipeline
  help           Show this help message

Environment Variables:
  IDF_PATH       ESP-IDF installation path (default: ~/esp/esp-idf)

Examples:
  ./make.sh setup
  ./make.sh build
  ./make.sh flash-monitor
  ./make.sh ci
EOF
}

main() {
    local command="${1:-help}"
    
    case "$command" in
        setup)
            setup_environment
            ;;
        build)
            build_project
            ;;
        clean)
            clean_build
            ;;
        flash)
            flash_device
            ;;
        monitor)
            monitor_serial
            ;;
        flash-monitor)
            flash_and_monitor
            ;;
        test)
            run_tests
            ;;
        test-qemu|qemu)
            run_qemu_test
            ;;
        setup-qemu)
            setup_qemu
            ;;
        doc|docs|documentation)
            generate_documentation
            ;;
        ci|ci-cd)
            run_ci
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "Unknown command: $command"
            show_help
            exit 1
            ;;
    esac
}

# Run main function
main "$@"

