import sys
import time
import os

def upload_and_run(port='COM10', baud=19200, bin_path=r'C:\Users\Lenovo\.gemini\antigravity\scratch\GEMMrv32\GEMMrv32-main\sw\example\demo_predictive_lm\neorv32_exe.upload.bin'):
    try:
        import serial
    except ImportError:
        import subprocess
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pyserial"])
        import serial

    if not os.path.exists(bin_path):
        print(f"Error: binary file not found at {bin_path}")
        return

    file_size = os.path.getsize(bin_path)
    with open(bin_path, 'rb') as f:
        bin_data = f.read()

    print(f"Connecting to {port} @ {baud} baud...")
    with serial.Serial(port, baud, timeout=0.2) as ser:
        ser.reset_input_buffer()
        print("Waiting for bootloader prompt or sending reset/abort...")
        
        # Send newlines / abort chars
        ser.write(b'\r\n')
        time.sleep(0.2)
        ser.write(b'h\r\n')
        time.sleep(0.3)
        
        resp = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
        print(f"[BOOTLOADER]:\n{resp}")

        # Send 'u' command for UART upload
        print("\nSending 'u' command (Upload via UART)...")
        ser.write(b'u')
        time.sleep(0.5)

        resp = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
        print(f"[BOOTLOADER]:\n{resp}")

        # Stream binary
        print(f"Streaming {file_size} bytes of executable...")
        ser.write(bin_data)
        ser.flush()

        time.sleep(1.0)
        resp = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
        print(f"[BOOTLOADER]:\n{resp}")

        # Start executable ('e')
        print("\nSending 'e' command (Execute)...")
        ser.write(b'e')
        time.sleep(0.5)

        # Print application output
        print("\n=== APPLICATION STARTED ===")
        start = time.time()
        while time.time() - start < 3.0:
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
                sys.stdout.write(chunk)
                sys.stdout.flush()
            time.sleep(0.05)

if __name__ == "__main__":
    target_bin = sys.argv[1] if len(sys.argv) > 1 else r'C:\Users\Lenovo\.gemini\antigravity\scratch\GEMMrv32\GEMMrv32-main\sw\example\demo_micro_transformer\neorv32_exe.bin'
    upload_and_run(bin_path=target_bin)

