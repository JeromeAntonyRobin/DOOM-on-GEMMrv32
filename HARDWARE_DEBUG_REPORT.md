# GEMMrv32 Hardware & Firmware Debugging Comprehensive Report

**Board:** Digilent Basys 3 (`xc7a35tcpg236-1`)  
**Core:** NEORV32 RISC-V SoC with Custom 8x8 GEMM Systolic Array GPU, VGA Raster Controller, and PS/2 Keyboard Bridge  
**Date:** October 13, 2026  

---

## 1. Executive Summary

During bringup of the standalone workstation application (`demo_vga_doom`) on the Digilent Basys 3 board, two major symptoms occurred:
1. **Video Issue:** The VGA monitor backlight illuminated and locked onto 640x480 @ 60 Hz sync timing, but displayed a completely blank / black screen with no color bars. Meanwhile, the UART console (`COM10` @ 19200 baud) spewed an endless loop of:
   ```text
   ==============================================
   ==============================================
   ```
2. **Keyboard Issue:** The Dell USB keyboard connected to the Basys 3 USB port (`J4`) did not register any inputs or draw blocks on the screen, and the NumLock/CapsLock LEDs remained dark.

Both issues have been investigated down to their exact hardware/firmware root causes and fixed.

---

## 2. Issue 1: Infinite Reboot Loop & Blank Monitor

### Symptoms
- Monitor backlight turned on (sync locked).
- No pixels or colors appeared.
- UART repeatedly printed the first banner divider line and reset continuously.
- FPGA reset occasionally reported CPU bus trap error codes (`0x00000002 0x00000000`).

### Root Cause Analysis
1. **Physical Hardware RAM Size:**
   - In `sw/basys/neorv32_basys3_top.vhd`, the Data Memory (`DMEM_SIZE`) is synthesized as **64 KB** (`64 * 1024` bytes).
   - Base address: `0x80000000`.
   - Highest valid RAM address: `0x8000FFFF`.
2. **Software Linker Script Mismatch:**
   - In `sw/common/neorv32.ld`, the RAM size was hardcoded:
     ```ld
     __neorv32_ram_size = 128k;
     ```
   - In RISC-V startup code (`crt0.S`), the initial stack pointer (`sp`) is defined as `ORIGIN(ram) + LENGTH(ram)`.
   - Because of the 128k setting, `sp` was initialized to:
     $$\text{sp} = \text{0x80000000} + \text{128 KB} = \text{0x80020000}$$
   - This address is **64 KB above the physical top of hardware RAM**.
3. **The Crash Mechanism:**
   - Inside `main()`, the program called `neorv32_uart0_printf("\n==============================================\n")`.
   - This function performed standard stack allocation:
     ```assembly
     addi sp, sp, -32
     sw   ra, 28(sp)    # Written to 0x8001FFE0 (OUTSIDE 64K RAM!)
     ```
   - The write was pushed into unmapped space.
   - When the printf completed, it executed:
     ```assembly
     lw   ra, 28(sp)    # Read from 0x8001FFE0
     ret               # Jumps to (ra)
     ```
   - Because the address did not exist in hardware RAM, the bus returned `0x00000000`.
   - The CPU executed `ret` to `0x00000000`, which is the reset entry point (`_start`) in ROM!
   - Every single time the first print finished, the CPU rebooted from instruction 0.
   - Because it rebooted instantly on line 1, it never executed:
     - `setup_palette()` (CLUT palette RAM was never loaded).
     - `render_test_screen()` (160x120 VRAM was never filled).
     - The keyboard polling loop was never reached.

### Fix Implemented
- In `sw/common/neorv32.ld`, changed `__neorv32_ram_size` to be configurable and default to `64k`:
  ```ld
  __neorv32_rom_size = DEFINED(__neorv32_rom_size) ? __neorv32_rom_size : 64k;
  __neorv32_ram_size = DEFINED(__neorv32_ram_size) ? __neorv32_ram_size : 64k;
  ```
- In `makefile`, passed `--defsym,__neorv32_ram_size=65536`.
- Recompiled `main.elf` and inspected symbols via `readelf`:
  - Verified `__crt0_stack_top = 0x80010000` (exact top of 64 KB RAM).
- Embedded the corrected binary into `rtl/core/neorv32_imem_image.vhd`.
- The CPU now maintains stack integrity, runs through `main()`, writes the color palette, and renders the 8 solid color bars on the monitor.

---

## 3. Issue 2: Bootloader Mode (`BOOT_MODE_SELECT => 0`) Crash

### Symptoms
- When configuring `BOOT_MODE_SELECT => 0` to enable UART `u` upload mode, the board outputted nothing on `COM10` and the monitor showed "no signal".

### Root Cause Analysis
- The pre-compiled Bootloader ROM image in `rtl/core/neorv32_bootrom_image.vhd` had also been built with the old hardcoded linker script!
- Line 10 of `neorv32_bootrom_image.vhd` contained:
  ```assembly
  auipc x4, 0x80220   # Points sp to 0x80020000 (outside 64 KB RAM)
  ```
- Because the bootloader's stack pointer also exceeded 64 KB, the bootloader suffered the exact same startup stack corruption / bus exception as the user application.
- Additionally, in bootloader mode, the CPU only runs the serial console prompt and does not touch the VGA controller, leaving the monitor with no video signal.

### Fix Implemented
- Recompiled `sw/bootloader` with `-march=rv32i_zicsr_zifencei -mabi=ilp32` and the updated linker script (`__neorv32_ram_size=256`, `__neorv32_ram_base=0x80000000`).
- Verified via `readelf` that `__crt0_stack_top` is now `0x80000100` (inside the first 256 bytes of physical RAM).
- Extracted `sw/bootloader/elf.bin` and generated the new `rtl/core/neorv32_bootrom_image.vhd`.
- The bootloader stack is now safely located inside physical RAM.

---

## 3. Issue 2: Wishbone XBUS Acknowledge Logic

### Symptoms
- Early attempts to stop unmapped timeouts used a wildcard acknowledge:
  ```vhdl
  if (xbus_cyc = '1') and (xbus_stb = '1') then xbus_ack <= '1';
  ```
- This caused unmapped stack reads to silently return `0x00000000` instead of letting developers see bus faults.

### Fix Implemented
- In `sw/basys/neorv32_basys3_top.vhd`, restricted `xbus_ack` to only assert for mapped peripherals:
  ```vhdl
  if (vga_sel or ps2_sel) and (xbus_ack = '0') then
    xbus_ack <= '1';
  else
    xbus_ack <= '0';
  end if;
  ```
- Any unmapped access outside VGA (`0x90000000`-`0x9001FFFF`) and PS/2 (`0x90020000`-`0x9002FFFF`) is no longer masked.

---

## 4. Issue 3: PS/2 Keyboard Input Failure

### Symptoms
- The monitor showed color bars, but pressing keys on the connected USB keyboard did not register or draw 4x4 color blocks.
- The keyboard's NumLock and CapsLock LEDs were off.

### Root Cause Analysis
1. **Keyboard LEDs (NumLock/CapsLock):**
   - Under the USB HID / PS/2 standard, keyboards **do not** toggle their own indicator LEDs autonomously.
   - The host system must send command `0xED` (Set LEDs) followed by the LED bitmask to turn them on. Because the NEORV32 PS/2 controller is currently a receive-only bridge, the LEDs remain unlit even though the keyboard receives 5V VBUS power from the board.
2. **The Hardware Pin Swap in `basys3.xdc`:**
   - According to the official Digilent Basys 3 schematic and Master XDC:
     ```tcl
     set_property PACKAGE_PIN C17 [get_ports PS2Clk]
     set_property PACKAGE_PIN B17 [get_ports PS2Data]
     ```
   - In our design constraint file `sw/basys/basys3.xdc`, the pin definitions were **swapped backwards**:
     ```tcl
     set_property PACKAGE_PIN B17 [get_ports ps2_clk_i]   # WRONG: B17 is Data!
     set_property PACKAGE_PIN C17 [get_ports ps2_data_i]  # WRONG: C17 is Clock!
     ```
   - The FPGA was attempting to clock on the data line and sample data from the clock line. Zero valid PS/2 packets could be received.
3. **PS/2 State Machine Resynchronization:**
   - The original receiver lacked an idle frame timeout. If an electrical glitch occurred when plugging the keyboard in, the 11-bit counter (`bit_count`) stayed permanently misaligned.

### Fix Implemented
1. **Corrected Pin Mappings in `sw/basys/basys3.xdc`:**
   - `PACKAGE_PIN C17` mapped to `ps2_clk_i`.
   - `PACKAGE_PIN B17` mapped to `ps2_data_i`.
   - `PULLUP true` enabled on both pins.
2. **Upgraded Receiver in `rtl/core/neorv32_ps2.vhd`:**
   - Added an idle timer (`idle_cnt`). If `ps2_clk` stays high for > 100 µs (5,000 cycles at 50 MHz), `bit_count` resets to 0.
   - Replaced shift-indexing with explicit frame decoding:
     - `bit_count = 0`: Verify Start bit (`0`).
     - `bit_count = 1..8`: Store data bits directly into `rx_data_reg(bit_count - 1)` (LSB first).
     - `bit_count = 9`: Parity bit.
     - `bit_count = 10`: Verify Stop bit (`1`), then push the verified byte into the 16-entry FIFO.

3. **Wishbone Read Double-Pop Bug:**
   - In [`neorv32_basys3_top.vhd`](file:///C:/Users/Lenovo/Desktop/GEMMrv32/sw/basys/neorv32_basys3_top.vhd#L177), `ps2_re` was combinationally driven by `ps2_sel and not xbus_we`.
   - Because a Wishbone read cycle spans 2 clock cycles (strobe cycle + acknowledge cycle), `ps2_re` was asserted for two consecutive clock edges on a single `lw` instruction.
   - Result: Every CPU read popped two scancodes from the FIFO, prematurely advancing `fifo_rptr` before the CPU sampled the bus, returning `0x00` when only one byte was buffered.
   - **Fix:** Gated `ps2_re <= ps2_sel and not xbus_we and xbus_ack`, ensuring the FIFO pops exactly once at the precise clock edge when the Wishbone bus master captures the scancode.

4. **FIFO Simultaneous Push/Pop Race Condition:**
   - In [`neorv32_ps2.vhd`](file:///C:/Users/Lenovo/Desktop/GEMMrv32/rtl/core/neorv32_ps2.vhd#L107-L148), simultaneous stop-bit frame capture and CPU FIFO read resulted in overwriting `fifo_count`.
   - **Fix:** Refactored into unified `fifo_push` and `fifo_pop` strobe variables with atomic count increments/decrements.

5. **Hardware LED Telemetry for PS/2 Pins:**
   - Routed physical PS/2 pins to onboard LEDs:
     - `LD0` = PS/2 Clock (`C17`)
     - `LD1` = PS/2 Data (`B17`)
     - `LD7` = MMCM Locked (50 MHz clock healthy)

6. **Bootloader UART Stream Synchronization & `ERROR_SIGNATURE` Recovery:**
   - **Root Cause:** When typing `u` followed by `<Enter>` in terminal emulators, the trailing `\r` and `\n` characters remained in the UART0 hardware RX FIFO. When `system_app_load` began, it immediately consumed `\r` and `\n` as the first bytes of the binary header instead of the `"NEO!"` (`0x214F454E`) signature, causing an immediate `\aERROR_SIGNATURE` mismatch.
   - Furthermore, the upstream bootloader executed `break;` on upload failure, causing the CPU to drop into an infinite `wfi` panic loop that permanently hung the board until reset.
   - **Fix:**
     1. Updated `uart_setup()` in `sw/bootloader/hal/source/uart.c` to proactively drain the hardware RX FIFO.
     2. Updated `uart_stream_get()` to automatically discard any leading CR/LF/space characters before reading the 4-byte executable header, synchronizing directly onto the binary file's payload.
     3. Changed `main.c` to flush and `continue;` on upload errors rather than `break;`, allowing the console to cleanly return to `CMD:>` so developers can immediately re-try uploads without resetting the board.

---

## 5. File Modifications Summary

| File | Change Summary |
|---|---|
| `sw/common/neorv32.ld` | Fixed default RAM size from hardcoded 128 KB to 64 KB (`0x80010000` stack top). |
| `sw/bootloader/Makefile` & `rtl/core/neorv32_bootrom_image.vhd` | Rebuilt bootloader with stack pointer pointing inside 64 KB RAM (`0x80000100`). |
| `sw/example/demo_vga_doom/makefile` | Added `--defsym,__neorv32_ram_size=65536`. |
| `rtl/core/neorv32_imem_image.vhd` | Re-embedded compiled binary with corrected stack pointer initialization and crosshair cursor. |
| `sw/basys/neorv32_basys3_top.vhd` | Switched to `BOOT_MODE_SELECT => 0` (Bootloader UART upload mode); gated `ps2_re` with `xbus_ack = '1'`. |
| `sw/basys/basys3.xdc` | Corrected inverted pins: `C17` = `ps2_clk_i`, `B17` = `ps2_data_i`. |
| `rtl/core/neorv32_ps2.vhd` | Added 100 µs idle timeout resync, direct bit-indexed PS/2 framing, and atomic push/pop FIFO count logic. |
| `sw/basys/build_basys3.tcl` | Added `set_param general.maxThreads 8` and automatic JTAG programming on build completion. |

---

## 6. Issue 4: Uploader Premature Execution & `ERROR_EXCEPTION 0x7 0xFFE006A8 0x0001000C`

### Symptoms
- During serial firmware upload in `gemm_dashboard.py`, the terminal displayed:
  ```text
  [SEND FILE SUCCESS] Sent 3700 bytes successfully.
  ERROR_EXCEPTION 0x00000007 0xFFE006A8 0x0001000C
  ```

### Root Cause Analysis
1. **Blind Boot Trigger (`'e'`) in Dashboard:**
   - In `gemm_dashboard.py`, after sending the binary stream, the script blindly issued `flash_ser.write(b"e")` without waiting for or verifying that the bootloader returned `"OK"`.
   - If the upload failed or was rejected due to an `ERROR_SIGNATURE` framing shift, the bootloader dropped back to `CMD:>`.
   - The premature `'e'` command instructed the bootloader to jump to address `0x00000000` (`system_app_boot()`) where invalid or blank instructions remained in IMEM.
2. **Double Exception in Trap Handler:**
   - The invalid instructions caused the CPU to trap.
   - When jumping to the bootloader's trap handler (`0xFFE006A4`), the stack pointer had been decremented or was pointing into unmapped memory (`0x0001000C` - just above 64 KB IMEM boundary).
   - Executing `sw ra, 76(sp)` at `0xFFE006A8` triggered a Store Access Fault (`mcause = 0x7`, `mtval = 0x0001000C`).
3. **Outdated Application Binary in Bin Directory:**
   - The compiled `demo_vga_doom.bin` in `GEMMrvStudio/example/bin/` was outdated and had not been synchronized with the latest compilation.

### Fix Implemented
1. **Robust Handshake in `gemm_dashboard.py`:**
   - Pre-sends `b" \r\n"` to abort the 8-second countdown and obtain the `CMD:>` prompt cleanly.
   - Issues `u` and verifies `"Awaiting"` state before streaming.
   - Transmits data in 256-byte chunks with paced pacing (`10 ms`).
   - Polls and verifies the bootloader's `"OK"` response before sending `'e'`. If `"OK"` is not received, `'e'` is never sent, preventing any invalid memory execution.
2. **Re-packaged Fresh Binary:**
   - Re-compiled `demo_vga_doom` and packaged `demo_vga_doom.bin` (3700 bytes, `sp = 0x80010000`, checksum `0xD067C5C8`).

---

## 7. Verification Checklist

1. **VGA Output:**
   - 640x480 @ 60 Hz sync active.
   - 8 solid color bars (Black, Blue, Green, Cyan, Red, Magenta, Yellow, White) rendered across screen.
   - White outer border and center crosshair rendered.
2. **UART Console (`COM10` @ 19200 baud):**
   - Emits startup banner once without looping.
   - Prints `[VGA] Color bars rendered successfully!`.
   - Prints `[PS/2] Ready for keyboard input...`.
3. **PS/2 Keyboard:**
   - Type keys on the USB keyboard.
   - Scancodes and ASCII characters are printed to `COM10`.
   - 4x4 colored blocks are drawn at the cursor position on the VGA screen.

