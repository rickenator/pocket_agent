#!/bin/bash

# ESP32-S3 Smart Agent Flash Script

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Check if ESP-IDF is installed
if [ -z "$IDF_PATH" ]; then
    echo -e "${RED}ESP-IDF environment not found${NC}"
    exit 1
fi

# Check for port
PORT=${1:-/dev/ttyUSB0}

if [ ! -e "$PORT" ]; then
    echo -e "${RED}Error: Port $PORT not found${NC}"
    echo "Usage: $0 [PORT]"
    exit 1
fi

echo -e "${GREEN}=== Flashing ESP32-S3 Smart Agent ===${NC}"
echo "Port: $PORT"
echo ""

# Flash and monitor
echo -e "${GREEN}Flashing and monitoring...${NC}"
echo ""

# Stop any existing monitor
pkill -f "idf.py.*monitor" 2>/dev/null || true

# Flash
idf.py -p "$PORT" flash

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}Flashing successful!${NC}"
    echo ""
    echo "To exit monitor: Ctrl+]"
else
    echo ""
    echo -e "${RED}Flashing failed!${NC}"
    exit 1
fi