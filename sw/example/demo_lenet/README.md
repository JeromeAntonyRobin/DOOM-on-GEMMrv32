# Hardware-Accelerated LeNet Inference (Color Recognition)

This is a complete end-to-end Convolutional Neural Network deployed on bare-metal RISC-V! 
The entire inference (Conv1, MaxPool, Conv2, FC1, FC2, FC3) is fully accelerated using the custom systolic array driver in `gemmrv_hw`.

## Tutorial

The model was trained offline to recognize the dominant color of small `32x32` images (such as cars, blocks, or solid colors). We have included several test images in the `images/` directory.

### Step 1: Compile the Firmware
You must compile the embedded C code against the NEORV32 library and our hardware driver.
```bash
# Inside the demo_lenet folder:
make clean_all exe
```

### Step 2: Flash the FPGA
If you haven't already, flash the bitstream to the Nexys A7 over JTAG using Vivado Hardware Manager.

### Step 3: Run the Inference Pipeline
You can use a python script to automatically sync with the bootloader, stream the `neorv32_exe.bin`, and then stream any of the color images (e.g. `images/blue.jpeg` or `images/red.jpeg`).

```bash
# Provide any of the 8+ car color images!
python3 auto_run_benchmark.py sw/example/demo_lenet/images/red.jpeg
```

### Expected Output
The hardware will perform an accelerated `im2col` matrix multiplication. 
* Total inference takes ~27ms on hardware vs ~400ms on software!
* The terminal will print a 23-element logit array proving 100% mathematical parity.
