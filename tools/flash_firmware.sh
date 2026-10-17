#!/bin/bash
# ==============================================================================
# flash_firmware.sh: Upload DOOM Firmware & Stream Telemetry
# ==============================================================================
set -e

PORT="${1:-/dev/ttyUSB1}"
BINARY="${2:-bin/demo_vga_doom.bin}"

echo "===================================================================="
echo "  DOOM-on-GEMMrv32: Autonomous Firmware Upload & Run"
echo "  Port: $PORT | Binary: $BINARY"
echo "===================================================================="

if [ ! -f "$BINARY" ]; then
    echo "[*] Binary '$BINARY' missing or outdated. Rebuilding..."
    make build
fi

python3 tools/upload_serial.py "$BINARY" --port "$PORT" --baud 19200
