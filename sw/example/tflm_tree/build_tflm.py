import os
import glob
import subprocess

PREFIX = "/mnt/FPGA_DISK/Xilinx/Vitis/2024.2/gnu/riscv/lin/riscv64-unknown-elf/bin/riscv64-unknown-elf-"
CC = PREFIX + "gcc"
CXX = PREFIX + "g++"
OBJCOPY = PREFIX + "objcopy"
OBJDUMP = PREFIX + "objdump"

CFLAGS = [
    "-march=rv32im_zicsr_zifencei", "-mabi=ilp32",
    "-Os", "-g", "-Wall", "-ffunction-sections", "-fdata-sections",
    "-I.", "-I../../../sw/lib/include",
    "-Ithird_party/gemmlowp", "-Ithird_party/flatbuffers/include", "-Ithird_party/ruy", "-Iexamples/hello_world",
    "-Ithird_party/kissfft", "-DTF_LITE_STATIC_MEMORY"
]
CXXFLAGS = CFLAGS + ["-fno-rtti", "-fno-exceptions", "-fno-threadsafe-statics", "-std=c++17"]

# LDFLAGS with proper memory limits
LDFLAGS = [
    "-march=rv32im_zicsr_zifencei", "-mabi=ilp32",
    "-Wl,--gc-sections", "-nostartfiles",
    "-Wl,--defsym,__neorv32_rom_size=256k",
    "-Wl,--defsym,__neorv32_ram_size=128k",
    "-T../../../sw/common/neorv32.ld",
    "-lm", "-lc", "-lgcc"
]

# Run within sw/example/tflm_tree
sources = glob.glob("**/*.c", recursive=True) + glob.glob("**/*.cpp", recursive=True) + glob.glob("**/*.S", recursive=True)
# Exclude test files except hello_world_test.cpp
sources = [s for s in sources if not (s.endswith("_test.cc") or s.endswith("_test.cpp")) or s.endswith("hello_world_test.cpp")]

# Add neorv32 core files
core_sources = glob.glob("../../../sw/lib/source/*.c")
core_sources.append("../../../sw/common/crt0.S")
sources.extend(core_sources)

objects = []
if not os.path.exists("build"):
    os.makedirs("build")

import concurrent.futures

def compile_src(src):
    # Ensure build dir exists for this file
    obj = "build/" + src.replace("../../../", "").replace("/", "_").replace(".c", ".o").replace(".cpp", ".o").replace(".S", ".o")
    if os.path.exists(obj) and os.path.getmtime(obj) > os.path.getmtime(src):
        return obj
    cmd = [CXX if src.endswith(".cpp") else CC] + (CXXFLAGS if src.endswith(".cpp") else CFLAGS) + ["-c", src, "-o", obj]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error compiling {src}:\n{res.stderr}")
        return None
    return obj

with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
    results = list(executor.map(compile_src, sources))

if None in results:
    print("Build failed.")
    exit(1)

objects = results
print("Linking...")
cmd = [CC] + objects + LDFLAGS + ["-o", "build/main.elf"]
res = subprocess.run(cmd, capture_output=True, text=True)
if res.returncode != 0:
    print(f"Error linking:\n{res.stderr}")
    exit(1)

subprocess.run([OBJCOPY, "-O", "binary", "build/main.elf", "build/neorv32_exe.bin"])
print("Build success! build/neorv32_exe.bin is ready.")

# Convert to exe wrapper using image_gen
print("Generating neorv32_exe.bin for bootloader...")
subprocess.run(["../../../sw/image_gen/image_gen", "-t", "exe", "-b", os.popen("riscv64-unknown-elf-readelf -h build/main.elf | grep 'Entry point address' | awk '{print $4}'").read().strip(), "-i", "build/neorv32_exe.bin", "-o", "build/neorv32_exe_final.bin"])
