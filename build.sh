#!/bin/bash

# ================================================================
# ESP32 LoRa Mesh Server - Build and Flash Script
# Using arduino-cli
# ================================================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ================================================================
# Parse .env file for compile-time defines
# ================================================================
BUILD_DEFINES=()
if [ -f ".env" ]; then
    echo -e "${BLUE}[ENV] Loading .env file...${NC}"
    trim() {
        local s="$1"
        s="${s#"${s%%[![:space:]]*}"}"
        s="${s%"${s##*[![:space:]]}"}"
        printf '%s' "$s"
    }
    while IFS='=' read -r key value; do
        # Skip comments and empty lines
        [[ -z "$key" || "$key" =~ ^[[:space:]]*# ]] && continue
        # Trim whitespace (preserve internal spaces and # in values)
        key="$(trim "$key")"
        value="$(trim "$value")"
        [ -z "$key" ] && continue

        # String values need escaped quotes for C compiler
        case "$key" in
            WIFI_SSID|WIFI_PASS|SERVER_IP|DEVICE_NAME|ENCRYPTION_PASSPHRASE|TCP_SHARED_SECRET)
                # Escape for C string literals and avoid spaces in compiler args
                escaped_value="${value//\\/\\\\}"
                escaped_value="${escaped_value//\"/\\\"}"
                # Use octal escapes to avoid C hex-escape bleed (e.g. \x20B)
                escaped_value="${escaped_value// /\\040}"
                escaped_value="${escaped_value//	/\\011}"
                BUILD_DEFINES+=("-D${key}=\"${escaped_value}\"")
                ;;
            SERVER_PORT|NODE_ID|LORA_POWER|LORA_SF|LORA_CR|LORA_HOPS|TCP_ENABLED)
                BUILD_DEFINES+=("-D${key}=${value}")
                ;;
            LORA_FREQ|LORA_BW)
                BUILD_DEFINES+=("-D${key}=${value}")
                ;;
        esac
        case "$key" in
            WIFI_PASS|ENCRYPTION_PASSPHRASE)
                echo -e "  ${GREEN}${key}${NC} = ********"
                ;;
            *)
                echo -e "  ${GREEN}${key}${NC} = ${value}"
                ;;
        esac
    done < .env
    echo ""
else
    echo -e "${YELLOW}[ENV] No .env file found (using firmware defaults)${NC}"
    echo ""
fi

# Configuration
SKETCH_DIR="$(pwd)"
SKETCH_NAME="$(basename "$SKETCH_DIR")"
SKETCH="$SKETCH_DIR"  # full path to sketch (used by arduino-cli upload)
FQBN="esp32:esp32:esp32"
PORT="/dev/ttyUSB0"  # Default port, auto-detect if not found
BAUD="115200"

# Options
FLASH_ALL=false  # If true, flash all detected serial ports
CUSTOM_PORT=""  # User-specified single port

# Usage
usage() {
    echo "Usage: $0 [-a] [-p PORT]"
    echo "  -a        Flash firmware to ALL connected ESP devices (all /dev/ttyUSB* /dev/ttyACM*)"
    echo "  -p PORT   Specify a single port to use (overrides auto-detect)"
    echo "  -h        Show this help"
}

# Parse arguments
while getopts ":ap:h" opt; do
  case ${opt} in
    a )
      FLASH_ALL=true
      ;;
    p )
      CUSTOM_PORT="$OPTARG"
      ;;
    h )
      usage
      exit 0
      ;;
    \? )
      echo "Invalid Option: -$OPTARG" 1>&2
      usage
      exit 1
      ;;
  esac
done
shift $((OPTIND -1))

# If a custom port was specified, use it
if [ -n "$CUSTOM_PORT" ]; then
    PORT="$CUSTOM_PORT"
fi

# Required libraries (install by name; versions omitted for best-compatibility)
LIBRARIES=(
    "RadioLib"
    "Adafruit SSD1306"
    "Adafruit GFX Library"
)

echo -e "${BLUE}================================================================"
echo -e "  ESP32 LoRa Mesh Server - Build Script"
echo -e "================================================================${NC}"
echo ""

# ================================================================
# Check if arduino-cli is installed
# ================================================================
echo -e "${BLUE}[1/7] Checking arduino-cli installation...${NC}"
if ! command -v arduino-cli &> /dev/null; then
    echo -e "${RED}[ERROR] arduino-cli not found!${NC}"
    echo ""
    echo "Please install arduino-cli:"
    echo "  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    echo "  sudo mv bin/arduino-cli /usr/local/bin/"
    echo ""
    echo "Or via package manager:"
    echo "  sudo apt install arduino-cli     # Debian/Ubuntu"
    echo "  brew install arduino-cli         # macOS"
    exit 1
fi
echo -e "${GREEN}✓ arduino-cli found: $(arduino-cli version)${NC}"
echo ""

# ================================================================
# Update core index
# ================================================================
echo -e "${BLUE}[2/7] Updating package index...${NC}"
arduino-cli core update-index
echo -e "${GREEN}✓ Package index updated${NC}"
echo ""

# ================================================================
# Install ESP32 core if needed
# ================================================================
echo -e "${BLUE}[3/7] Checking ESP32 core installation...${NC}"
if ! arduino-cli core list | grep -q "esp32:esp32"; then
    echo -e "${YELLOW}ESP32 core not found, installing...${NC}"
    arduino-cli config init 2>/dev/null || true
    arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
    echo -e "${GREEN}✓ ESP32 core installed${NC}"
else
    echo -e "${GREEN}✓ ESP32 core already installed${NC}"
fi
echo ""

# ================================================================
# Install required libraries
# ================================================================
echo -e "${BLUE}[4/7] Installing required libraries...${NC}"
for lib in "${LIBRARIES[@]}"; do
    lib_name=$(echo "$lib" | cut -d'/' -f2 | cut -d'@' -f1)
    echo -e "  → Installing ${lib_name}..."
    arduino-cli lib install "$lib" || echo -e "${YELLOW}  (already installed or error)${NC}"
done
echo -e "${GREEN}✓ Libraries installed${NC}"
echo ""

# ================================================================
# Auto-detect USB port(s)
# ================================================================
echo -e "${BLUE}[5/7] Detecting ESP32 board(s)...${NC}"

# If -a (FLASH_ALL) requested, collect all candidate ports
if [ "$FLASH_ALL" = true ]; then
    PORTS=()
    for p in /dev/ttyUSB* /dev/ttyACM* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART*; do
        if [ -e "$p" ]; then
            PORTS+=("$p")
        fi
    done
    if [ ${#PORTS[@]} -eq 0 ]; then
        echo -e "${RED}[ERROR] No serial ports found to flash!${NC}"
        ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  (none found)"
        exit 1
    fi
    echo -e "${GREEN}✓ Found ${#PORTS[@]} serial ports to flash:${NC}"
    for p in "${PORTS[@]}"; do echo "  - $p"; done
else
    # Single-port mode
    if [ -n "$CUSTOM_PORT" ]; then
        PORT="$CUSTOM_PORT"
    fi

    # Try to auto-detect port if default doesn't exist
    if [ ! -e "$PORT" ]; then
        echo -e "${YELLOW}Default port $PORT not found, trying to auto-detect...${NC}"
        for test_port in /dev/ttyUSB* /dev/ttyACM* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART*; do
            if [ -e "$test_port" ]; then
                PORT="$test_port"
                echo -e "${GREEN}✓ Found ESP32 at $PORT${NC}"
                break
            fi
        done

        if [ ! -e "$PORT" ]; then
            echo -e "${RED}[ERROR] No ESP32 board found!${NC}"
            echo "Available serial ports:"
            ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  (none found)"
            exit 1
        fi
    else
        echo -e "${GREEN}✓ ESP32 found at $PORT${NC}"
    fi
fi

echo ""

# ================================================================
# Compile sketch
# ================================================================
echo -e "${BLUE}[6/7] Compiling sketch...${NC}"
echo -e "  Board: $FQBN"
echo -e "  Sketch: $SKETCH_NAME"
echo ""

# Some libraries still key off the legacy ESP32 macro.
if [[ "$FQBN" == esp32:* ]]; then
    BUILD_DEFINES+=("-DESP32")
fi

# Compile once and put build artifacts into ./build
echo -e "${BLUE}Compiling once into ./build (reused for uploads)...${NC}"
COMPILE_ARGS=(arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR" --output-dir build --warnings all)
if [ ${#BUILD_DEFINES[@]} -gt 0 ]; then
    BUILD_DEFINES_STR="${BUILD_DEFINES[*]}"
    COMPILE_ARGS+=(--build-property "build.extra_flags=$BUILD_DEFINES_STR")
    redact_define() {
        case "$1" in
            -DWIFI_SSID=*)
                # Show only a short prefix/suffix of SSID
                local v="${1#-DWIFI_SSID=}"
                v="${v#\"}"
                v="${v%\"}"
                local len=${#v}
                local head=4
                local tail=3
                if [ $len -le $((head+tail)) ]; then
                    echo "-DWIFI_SSID=\"${v}\""
                else
                    local prefix="${v:0:head}"
                    local suffix="${v:len-tail:tail}"
                    echo "-DWIFI_SSID=\"${prefix}…${suffix}\""
                fi
                ;;
            -DWIFI_PASS=*|-DENCRYPTION_PASSPHRASE=*|-DTCP_SHARED_SECRET=*)
                echo "${1%%=*}=********"
                ;;
            *)
                echo "$1"
                ;;
        esac
    }
    DISPLAY_DEFINES=()
    for def in "${BUILD_DEFINES[@]}"; do
        DISPLAY_DEFINES+=("$(redact_define "$def")")
    done
    echo -e "${BLUE}Build defines: ${NC}${DISPLAY_DEFINES[*]}"
    echo ""
fi
COMPILE_OUTPUT=$("${COMPILE_ARGS[@]}" 2>&1) || {
    echo ""
    echo -e "${RED}[ERROR] Compilation failed!${NC}"
    echo ""
    echo "$COMPILE_OUTPUT"
    echo ""
    echo "Common issues:"
    echo "  1. Missing libraries - check library names"
    echo "  2. Syntax errors in sketch"
    echo "  3. Wrong board selected"
    exit 1
}

echo ""
echo -e "${GREEN}✓ Compilation successful!${NC}"

# Show sketch size lines from the compiler output
echo ""
echo -e "${BLUE}Sketch size:${NC}"
echo "$COMPILE_OUTPUT" | grep -E "Sketch uses|Global variables" || true

echo ""

# ================================================================
# Upload to ESP32 (single or multiple ports)
# ================================================================
echo -e "${BLUE}[7/7] Uploading to ESP32...${NC}"

if [ "$FLASH_ALL" = true ]; then
    echo -e "${BLUE}Flashing all detected ports...${NC}"
    successes=0
    failures=0
    for p in "${PORTS[@]}"; do
        echo -e "\n${BLUE}Uploading to $p ...${NC}"
        # Reuse compiled build in ./build to avoid recompiling for each port
        if arduino-cli upload --fqbn "$FQBN" --port "$p" --input-dir build "$SKETCH_DIR"; then
            echo -e "${GREEN}✓ Upload to $p successful${NC}"
            successes=$((successes+1))
        else
            echo -e "${RED}✗ Upload to $p FAILED${NC}"
            failures=$((failures+1))
        fi
    done
    echo ""
    echo -e "${GREEN}Flash summary:${NC} ${successes} succeeded, ${failures} failed"
    if [ $failures -gt 0 ]; then
        echo -e "${YELLOW}Some uploads failed. You may retry specific ports with -p /dev/ttyUSBX${NC}"
    fi
    exit 0
else
    echo -e "  Port: $PORT"
    echo -e "  Baud: $BAUD"
    echo ""

    # Use compiled build in ./build to avoid recompilation
    if arduino-cli upload --fqbn "$FQBN" --port "$PORT" --input-dir build "$SKETCH_DIR"; then
        echo ""
        echo -e "${GREEN}================================================================"
        echo -e "  ✓ Upload successful!"
        echo -e "================================================================${NC}"
        echo ""
        echo -e "${BLUE}To monitor serial output, run:${NC}"
        echo -e "  arduino-cli monitor -p $PORT -c baudrate=$BAUD"
        echo ""
        echo -e "${BLUE}Or use screen:${NC}"
        echo -e "  screen $PORT $BAUD"
        echo ""
        echo -e "${BLUE}Or use minicom:${NC}"
        echo -e "  minicom -D $PORT -b $BAUD"
        echo ""
        echo -e "${YELLOW}Type 'HELP' in serial monitor for available commands.${NC}"
        echo ""
        
        # Ask if user wants to open serial monitor
        read -p "Open serial monitor now? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${BLUE}Opening serial monitor... (Ctrl+C to exit)${NC}"
            sleep 2
            arduino-cli monitorv2 -p "$PORT" -c baudrate="$BAUD"
        fi
    else
        echo ""
        echo -e "${RED}[ERROR] Upload failed!${NC}"
        echo ""
        echo "Common issues:"
        echo "  1. Wrong port selected"
        echo "  2. Board not in bootloader mode"
        echo "  3. Another program using the serial port"
        echo "  4. Permission denied - try: sudo chmod 666 $PORT"
        echo ""
        echo "Try manually:"
        echo "  1. Hold BOOT button on ESP32"
        echo "  2. Press EN/RST button"
        echo "  3. Release BOOT button"
        echo "  4. Run this script again"
        exit 1
    fi
fi
