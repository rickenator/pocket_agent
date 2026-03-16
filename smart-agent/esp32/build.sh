#!/bin/bash

# ESP32-S3 Smart Agent Build Script

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo -e "${GREEN}=== ESP32-S3 Smart Agent Build Script ===${NC}"
echo ""

# Check if ESP-IDF is installed
if [ -z "$IDF_PATH" ]; then
    echo -e "${YELLOW}ESP-IDF environment not found${NC}"
    echo "Please source the ESP-IDF export script first:"
    echo "  source $HOME/esp/esp-idf/export.sh"
    exit 1
fi

# Check for required tools
echo -e "${GREEN}Checking for required tools...${NC}"
for tool in cmake idf.py python3; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: $tool is not installed${NC}"
        exit 1
    fi
    echo "  ✓ $tool"
done
echo ""

# Configuration options
CONFIG_MODE="sim"  # sim or hardware
CONFIG_BACKEND="ollama"  # ollama or gemini

echo -e "${YELLOW}Configuration${NC}"
echo "  Display mode: $CONFIG_MODE (simulation/hardware)"
echo "  AI backend: $CONFIG_BACKEND (ollama/gemini)"
echo ""
read -p "Use default configuration? (Y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    read -p "Display mode [sim/hardware]: " CONFIG_MODE
    read -p "AI backend [ollama/gemini]: " CONFIG_BACKEND
fi

# Build
echo -e "${GREEN}Building firmware...${NC}"
echo ""

# Configure with menuconfig
idf.py menuconfig

# Build
idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}Build successful!${NC}"
    echo ""
    echo "To flash:"
    echo "  ./flash.sh /dev/ttyUSB0"
    echo ""
    echo "To monitor:"
    echo "  ./flash.sh /dev/ttyUSB0 monitor"
else
    echo ""
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi