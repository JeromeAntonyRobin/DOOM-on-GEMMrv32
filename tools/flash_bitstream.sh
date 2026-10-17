#!/bin/bash
# ==============================================================================
# flash_bitstream.sh: Program Basys 3 FPGA Bitstream Autonomously
# ==============================================================================
set -e

BITSTREAM="${1:-bit/neorv32_basys3.bit}"

if [ ! -f "$BITSTREAM" ]; then
    echo "[!] Error: Bitstream file '$BITSTREAM' not found!"
    exit 1
fi

echo "===================================================================="
echo "  DOOM-on-GEMMrv32: Autonomous Bitstream Programmer (Basys 3)"
echo "  Target: $BITSTREAM"
echo "===================================================================="

# Check if openFPGALoader can access USB without sudo
if openFPGALoader --board=basys3 --detect >/dev/null 2>&1; then
    echo "[*] USB access verified. Programming FPGA SRAM directly..."
    openFPGALoader --board=basys3 "$BITSTREAM"
else
    echo "[*] Standard user permission restricted. Invoking with sudo..."
    sudo $(which openFPGALoader || echo ~/.local/bin/openFPGALoader) --board=basys3 "$BITSTREAM"
fi

echo "[+] SUCCESS: FPGA successfully programmed! Monitor sync locked onto 640x480 @ 60Hz."
