import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import json
import os
import subprocess
import numpy as np
from PIL import Image, ImageDraw

# Root directory of the project on Desktop
PROJECT_DIR = r"C:\Users\Lenovo\Desktop\GEMMrv32\GEMMrv32-main"
DEFAULT_BIN = os.path.join(PROJECT_DIR, r"sw\example\demo_cnn_classifier\neorv32_exe.bin")

class CNNApp:
    def __init__(self, root):
        self.root = root
        self.root.title("GEMMrv32 - 8x8 Systolic CNN Classifier Dashboard")
        self.root.geometry("860x680")
        self.root.resizable(False, False)
        self.root.configure(bg="#1e1e2e")

        self.ser = None
        self.is_connected = False
        self.serial_port = "COM10"
        self.baud_rate = 19200

        # Canvas drawing parameters
        self.canvas_size = 280
        self.grid_size = 28
        self.cell_size = self.canvas_size // self.grid_size
        self.brush_size = 18

        # In-memory image for antialiased downsampling
        self.image = Image.new("L", (self.canvas_size, self.canvas_size), color=0)
        self.draw = ImageDraw.Draw(self.image)

        self.setup_ui()
        self.auto_connect()

    def setup_ui(self):
        # Header Title
        title_frame = tk.Frame(self.root, bg="#1e1e2e")
        title_frame.pack(fill="x", pady=10, padx=20)

        lbl_title = tk.Label(title_frame, text="NEORV32 8x8 Systolic Array CNN Accelerator",
                             font=("Consolas", 16, "bold"), fg="#89b4fa", bg="#1e1e2e")
        lbl_title.pack(anchor="w")

        lbl_sub = tk.Label(title_frame, text="36-Class Classifier (0-9 & A-Z) | Live Hardware Acceleration Dashboard",
                            font=("Consolas", 10), fg="#a6adc8", bg="#1e1e2e")
        lbl_sub.pack(anchor="w")

        # Top Action Bar: Kill Processes & Program Firmware
        action_bar = tk.Frame(self.root, bg="#181825", bd=1, relief="ridge")
        action_bar.pack(fill="x", padx=20, pady=(0, 10))

        self.btn_kill = tk.Button(action_bar, text="KILL BG PROCESSES / FREE COM PORT",
                                  command=self.kill_bg_processes,
                                  font=("Consolas", 9, "bold"), bg="#f38ba8", fg="#11111b",
                                  activebackground="#eba0ac", relief="flat", padx=10, pady=5)
        self.btn_kill.pack(side="left", padx=10, pady=8)

        self.btn_program = tk.Button(action_bar, text="PROGRAM / FLASH FIRMWARE (PROG Wait)",
                                     command=self.start_flash_flow,
                                     font=("Consolas", 9, "bold"), bg="#fab387", fg="#11111b",
                                     activebackground="#f9e2af", relief="flat", padx=10, pady=5)
        self.btn_program.pack(side="left", padx=5, pady=8)

        self.lbl_flash_status = tk.Label(action_bar, text="Ready", font=("Consolas", 9, "italic"),
                                         fg="#89dceb", bg="#181825")
        self.lbl_flash_status.pack(side="left", padx=15, pady=8)

        # Main Container
        main_frame = tk.Frame(self.root, bg="#1e1e2e")
        main_frame.pack(fill="both", expand=True, padx=20, pady=5)

        # Left Column: Canvas & Controls
        left_col = tk.Frame(main_frame, bg="#181825", bd=2, relief="groove")
        left_col.pack(side="left", padx=(0, 15), pady=5, fill="y")

        lbl_draw = tk.Label(left_col, text="Draw Digit (0-9) or Letter (A-Z):",
                            font=("Consolas", 11, "bold"), fg="#cdd6f4", bg="#181825")
        lbl_draw.pack(pady=(10, 5))

        self.canvas = tk.Canvas(left_col, width=self.canvas_size, height=self.canvas_size,
                                bg="#000000", cursor="pencil", highlightthickness=1, highlightbackground="#45475a")
        self.canvas.pack(padx=15, pady=5)
        self.canvas.bind("<B1-Motion>", self.paint)
        self.canvas.bind("<Button-1>", self.paint)

        # Control Buttons
        btn_frame = tk.Frame(left_col, bg="#181825")
        btn_frame.pack(fill="x", padx=15, pady=10)

        self.btn_send = tk.Button(btn_frame, text="RUN CLASSIFICATION", command=self.send_to_fpga,
                                  font=("Consolas", 11, "bold"), bg="#a6e3a1", fg="#11111b",
                                  activebackground="#94e2d5", relief="flat", padx=10, pady=6)
        self.btn_send.pack(fill="x", pady=(0, 6))

        self.btn_clear = tk.Button(btn_frame, text="Clear Canvas", command=self.clear_canvas,
                                   font=("Consolas", 10), bg="#45475a", fg="#cdd6f4",
                                   activebackground="#585b70", relief="flat", padx=5, pady=4)
        self.btn_clear.pack(fill="x")

        # Right Column: Live Telemetry & Metrics
        right_col = tk.Frame(main_frame, bg="#181825", bd=2, relief="groove")
        right_col.pack(side="right", fill="both", expand=True, pady=5)

        lbl_metric = tk.Label(right_col, text="HARDWARE ACCELERATION TELEMETRY",
                              font=("Consolas", 12, "bold"), fg="#f9e2af", bg="#181825")
        lbl_metric.pack(pady=(12, 10))

        # Prediction Box
        pred_frame = tk.Frame(right_col, bg="#11111b", bd=2, relief="sunken")
        pred_frame.pack(fill="x", padx=20, pady=5)

        tk.Label(pred_frame, text="PREDICTED CLASS", font=("Consolas", 9), fg="#6c7086", bg="#11111b").pack(pady=(6, 0))
        self.lbl_pred = tk.Label(pred_frame, text="-", font=("Consolas", 48, "bold"), fg="#a6e3a1", bg="#11111b")
        self.lbl_pred.pack(pady=(0, 6))

        # Speedup & Benchmark Table
        stats_frame = tk.Frame(right_col, bg="#181825")
        stats_frame.pack(fill="x", padx=20, pady=10)

        # SW Cycles
        row1 = tk.Frame(stats_frame, bg="#181825")
        row1.pack(fill="x", pady=4)
        tk.Label(row1, text="CPU Baseline (SW):", font=("Consolas", 10), fg="#cdd6f4", bg="#181825").pack(side="left")
        self.lbl_sw_cyc = tk.Label(row1, text="0 cycles", font=("Consolas", 10, "bold"), fg="#fab387", bg="#181825")
        self.lbl_sw_cyc.pack(side="right")

        # HW Cycles
        row2 = tk.Frame(stats_frame, bg="#181825")
        row2.pack(fill="x", pady=4)
        tk.Label(row2, text="8x8 Systolic Unit (HW):", font=("Consolas", 10), fg="#cdd6f4", bg="#181825").pack(side="left")
        self.lbl_hw_cyc = tk.Label(row2, text="0 cycles", font=("Consolas", 10, "bold"), fg="#89dceb", bg="#181825")
        self.lbl_hw_cyc.pack(side="right")

        # Speedup
        row3 = tk.Frame(stats_frame, bg="#181825")
        row3.pack(fill="x", pady=6)
        tk.Label(row3, text="HARDWARE SPEEDUP:", font=("Consolas", 12, "bold"), fg="#f9e2af", bg="#181825").pack(side="left")
        self.lbl_speedup = tk.Label(row3, text="0.0x", font=("Consolas", 16, "bold"), fg="#f38ba8", bg="#181825")
        self.lbl_speedup.pack(side="right")

        # Connection & Log Status Bar
        self.lbl_status = tk.Label(self.root, text="Status: Initializing...", font=("Consolas", 9),
                                   fg="#a6adc8", bg="#1e1e2e", anchor="w")
        self.lbl_status.pack(fill="x", side="bottom", padx=20, pady=5)

    def paint(self, event):
        x1, y1 = (event.x - self.brush_size), (event.y - self.brush_size)
        x2, y2 = (event.x + self.brush_size), (event.y + self.brush_size)
        self.canvas.create_oval(x1, y1, x2, y2, fill="#ffffff", outline="#ffffff")
        self.draw.ellipse([x1, y1, x2, y2], fill=255)

    def clear_canvas(self):
        self.canvas.delete("all")
        self.image = Image.new("L", (self.canvas_size, self.canvas_size), color=0)
        self.draw = ImageDraw.Draw(self.image)
        self.lbl_pred.config(text="-", fg="#a6e3a1")
        self.lbl_sw_cyc.config(text="0 cycles")
        self.lbl_hw_cyc.config(text="0 cycles")
        self.lbl_speedup.config(text="0.0x", fg="#f38ba8")

    def auto_connect(self):
        try:
            if self.ser and self.ser.is_open:
                return True
            self.ser = serial.Serial(self.serial_port, self.baud_rate, timeout=1.0)
            self.is_connected = True
            self.lbl_status.config(text=f"Status: Connected to FPGA on {self.serial_port} @ {self.baud_rate} baud")
            return True
        except Exception as e:
            self.lbl_status.config(text=f"Status: {self.serial_port} not opened: {e}")
            return False

    def kill_bg_processes(self):
        """Kills any background processes holding COM ports or python child instances"""
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
                self.ser = None

            # Kill any other python instances except current PID
            current_pid = os.getpid()
            cmd = f'powershell -Command "Get-Process python* -ErrorAction SilentlyContinue | Where-Object {{ $_.Id -ne {current_pid} }} | Stop-Process -Force -ErrorAction SilentlyContinue"'
            subprocess.run(cmd, shell=True)

            time.sleep(0.5)
            self.lbl_status.config(text="Status: All background python processes terminated. COM port released.")
            messagebox.showinfo("Processes Killed", "All external processes holding the serial port have been terminated.\nCOM10 is free!")
            self.auto_connect()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to kill processes: {e}")

    def start_flash_flow(self):
        """Waits for user to click PROG button on Basys 3, detects bootloader, and flashes firmware"""
        if not os.path.exists(DEFAULT_BIN):
            messagebox.showerror("Binary Not Found", f"Cannot find firmware binary at:\n{DEFAULT_BIN}")
            return

        if self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None

        threading.Thread(target=self._worker_flash, daemon=True).start()

    def _worker_flash(self):
        self.lbl_flash_status.config(text="PRESS RED PROG BUTTON ON BASYS 3 BOARD NOW!", fg="#fab387")
        self.lbl_status.config(text="Status: Awaiting PROG button press on FPGA board...")

        bootloader_found = False
        start_time = time.time()

        while time.time() - start_time < 30.0:
            try:
                with serial.Serial(self.serial_port, self.baud_rate, timeout=0.2) as ser:
                    ser.reset_input_buffer()
                    ser.write(b"\r\n")
                    time.sleep(0.1)
                    ser.write(b"h\r\n")
                    time.sleep(0.2)
                    resp = ser.read(ser.in_waiting).decode('latin-1', errors='replace')
                    if "Available CMDs:" in resp or "CMD:>" in resp:
                        bootloader_found = True
                        break
            except Exception:
                pass
            time.sleep(0.5)

        if not bootloader_found:
            self.lbl_flash_status.config(text="Timeout waiting for PROG button", fg="#f38ba8")
            self.lbl_status.config(text="Status: Timed out waiting for Bootloader prompt.")
            return

        # Bootloader detected! Upload executable
        self.lbl_flash_status.config(text="Bootloader detected! Flashing firmware...", fg="#a6e3a1")
        self.lbl_status.config(text="Status: Uploading compiled binary via bootloader UART...")

        try:
            with open(DEFAULT_BIN, "rb") as f:
                bin_data = f.read()

            with serial.Serial(self.serial_port, self.baud_rate, timeout=1.0) as ser:
                ser.reset_input_buffer()
                # Send 'u' command
                ser.write(b"u")
                time.sleep(0.5)
                ser.read(ser.in_waiting)

                # Send binary stream
                ser.write(bin_data)
                ser.flush()
                time.sleep(1.0)
                ser.read(ser.in_waiting)

                # Send 'e' command
                ser.write(b"e")
                time.sleep(0.5)
                ser.read(ser.in_waiting)

            self.lbl_flash_status.config(text="Firmware flashed & booted successfully!", fg="#a6e3a1")
            self.lbl_status.config(text="Status: CNN Accelerator is running live on FPGA!")
            time.sleep(0.5)
            self.auto_connect()
            messagebox.showinfo("Success", "Firmware flashed and booted successfully!\nYou can now draw and run classification.")
        except Exception as e:
            self.lbl_flash_status.config(text=f"Flashing failed: {e}", fg="#f38ba8")
            self.lbl_status.config(text=f"Status: Flashing error: {e}")

    def send_to_fpga(self):
        if not self.ser or not self.ser.is_open:
            if not self.auto_connect():
                messagebox.showerror("Port Error", "Could not open COM10. Ensure FPGA is plugged in or click 'Kill BG Processes'.")
                return

        threading.Thread(target=self._worker_send, daemon=True).start()

    def _worker_send(self):
        try:
            self.lbl_status.config(text="Status: Downsampling 28x28 image and sending to FPGA...")
            # Downsample PIL image to 28x28
            img_resized = self.image.resize((28, 28), Image.Resampling.BILINEAR)
            arr = np.array(img_resized, dtype=np.float32)
            # Normalize to INT8 (-128 to 127)
            norm = np.clip((arr / 127.5 - 1.0) * 127.0, -128, 127).astype(np.int8)

            # Flush
            self.ser.reset_input_buffer()
            # Send sync header '!IMG'
            self.ser.write(b"!IMG")
            self.ser.write(norm.tobytes())
            self.ser.flush()

            # Read response
            start_t = time.time()
            line = ""
            while time.time() - start_t < 4.0:
                l = self.ser.readline().decode('latin-1', errors='replace').strip()
                if "RESULT:" in l:
                    line = l
                    break

            if "RESULT:" in line:
                payload = line.split("RESULT:")[1]
                data = json.loads(payload)
                pred = data.get("pred", "?")
                sw = data.get("sw_cycles", 0)
                hw = data.get("hw_cycles", 0)
                spd = data.get("speedup", "0.0x")

                self.root.after(0, lambda: self.update_results(pred, sw, hw, spd))
            else:
                self.lbl_status.config(text="Status: No RESULT packet received from FPGA. (Click 'Program / Flash' if needed)")
        except Exception as e:
            self.lbl_status.config(text=f"Status: Communication error: {e}")

    def update_results(self, pred, sw_cyc, hw_cyc, speedup):
        self.lbl_pred.config(text=str(pred))
        self.lbl_sw_cyc.config(text=f"{sw_cyc:,} cycles")
        self.lbl_hw_cyc.config(text=f"{hw_cyc:,} cycles")
        self.lbl_speedup.config(text=str(speedup), fg="#a6e3a1")
        self.lbl_status.config(text="Status: Classification complete!")

if __name__ == "__main__":
    root = tk.Tk()
    app = CNNApp(root)
    root.mainloop()
