#!/usr/bin/env python3
import sys
import time
import os
import argparse
import serial

def run_flasher(port='/dev/ttyUSB1', baud=19200, bin_path='bin/demo_vga_doom.bin'):
    if not os.path.exists(bin_path):
        print(f"[!] Error: binary file '{bin_path}' not found!")
        sys.exit(1)

    file_size = os.path.getsize(bin_path)
    with open(bin_path, 'rb') as f:
        bin_data = f.read()

    print(f"[*] Opening serial port {port} at {baud} baud...")
    try:
        ser = serial.Serial(port, baud, timeout=0.3)
    except Exception as e:
        print(f"[!] Serial error: {e}")
        sys.exit(1)

    with ser:
        ser.reset_input_buffer()
        print("[*] Probing NEORV32 bootloader...")

        # Handshake: Send space / newlines to abort bootloader countdown and get CMD:>
        ser.write(b" \r\n")
        time.sleep(0.2)
        resp = ser.read(ser.in_waiting).decode('latin-1', errors='replace')

        # If not at prompt, send 'h' to check
        if "CMD:>" not in resp:
            ser.write(b"h\r\n")
            time.sleep(0.3)
            resp += ser.read(ser.in_waiting).decode('latin-1', errors='replace')

        if "CMD:>" in resp or "NEORV32" in resp:
            print("[+] Bootloader prompt detected!")
        else:
            print("[!] Bootloader prompt not seen immediately. Proceeding with upload trigger...")

        # Send 'u' to request upload
        print("[*] Sending 'u' command (Upload via UART)...")
        ser.write(b"u")
        time.sleep(0.4)

        u_resp = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
        if u_resp.strip():
            print(f"[BOOTLOADER]:\n{u_resp.strip()}")

        # Stream binary chunks with progress
        print(f"[*] Streaming {file_size} bytes of executable...")
        chunk_size = 64
        total = file_size
        for i in range(0, total, chunk_size):
            chunk = bin_data[i:i+chunk_size]
            ser.write(chunk)
            pct = min(i + chunk_size, total) * 100 // total
            sys.stdout.write(f"\r[*] Progress: {min(i + chunk_size, total)}/{total} bytes ({pct}%)")
            sys.stdout.flush()
            time.sleep(0.01)

        print("\n[*] Upload payload complete. Verifying bootloader response...")
        time.sleep(0.6)
        ack_resp = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
        if ack_resp.strip():
            print(f"[BOOTLOADER]:\n{ack_resp.strip()}")

        # Send 'e' to execute
        print("[*] Sending 'e' command (Execute application)...")
        ser.write(b"e")
        time.sleep(0.3)

        print("\n=======================================================")
        print("  DOOM 2 EXECUTION STARTED! (Streaming live telemetry) ")
        print("  Controls: W, A, S, D = Move, SPACE = Fire, M = Minimap")
        print("  Press Ctrl+C to exit terminal monitor.")
        print("=======================================================\n")

        try:
            while True:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
                    sys.stdout.write(data)
                    sys.stdout.flush()
                time.sleep(0.02)
        except KeyboardInterrupt:
            print("\n[*] Exiting terminal monitor.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Autonomous firmware flasher & terminal monitor for DOOM on GEMMrv32.")
    parser.add_argument("binary", nargs="?", default="bin/demo_vga_doom.bin", help="Path to executable .bin file")
    parser.add_argument("--port", "-p", default="/dev/ttyUSB1", help="Serial port (default: /dev/ttyUSB1)")
    parser.add_argument("--baud", "-b", type=int, default=19200, help="Baud rate (default: 19200)")
    args = parser.parse_args()

    run_flasher(port=args.port, baud=args.baud, bin_path=args.binary)
EOF
chmod +x tools/upload_serial.py
