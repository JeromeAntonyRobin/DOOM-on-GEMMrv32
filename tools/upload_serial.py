#!/usr/bin/env python3
import sys
import time
import argparse
import serial

def upload(port, baudrate, binary_path):
    print(f"[*] Opening serial port {port} at {baudrate} baud...")
    try:
        ser = serial.Serial(port, baudrate, timeout=1.0)
    except Exception as e:
        print(f"[!] Error opening serial port: {e}")
        sys.exit(1)

    with open(binary_path, "rb") as f:
        binary_data = f.read()

    print(f"[*] Loaded binary '{binary_path}' ({len(binary_data)} bytes)")
    print("[*] Sending 'u' (Upload command to NEORV32 bootloader)...")
    
    # Flush incoming
    ser.reset_input_buffer()
    ser.write(b"u")
    time.sleep(0.5)

    print("[*] Uploading binary payload...")
    # Send binary with progress
    chunk_size = 256
    total = len(binary_data)
    for i in range(0, total, chunk_size):
        chunk = binary_data[i:i+chunk_size]
        ser.write(chunk)
        sys.stdout.write(f"\r[*] Progress: {min(i+chunk_size, total)}/{total} bytes ({min(i+chunk_size, total)*100//total}%)")
        sys.stdout.flush()
        time.sleep(0.01)

    print("\n[*] Upload complete!")
    time.sleep(0.5)

    print("[*] Sending 'e' (Execute command)...")
    ser.write(b"e")
    time.sleep(0.2)

    # Read output
    ser.timeout = 0.5
    out = ser.read(1024)
    if out:
        try:
            print("[*] Target Response:\n" + out.decode(errors='ignore'))
        except Exception:
            pass

    ser.close()
    print("[*] Done!")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upload executable to NEORV32 bootloader over UART.")
    parser.add_argument("--port", "-p", default="/dev/ttyUSB1", help="Serial port (default: /dev/ttyUSB1)")
    parser.add_argument("--baud", "-b", type=int, default=19200, help="Baud rate (default: 19200)")
    parser.add_argument("binary", help="Path to executable .bin file")
    args = parser.parse_args()

    upload(args.port, args.baud, args.binary)
