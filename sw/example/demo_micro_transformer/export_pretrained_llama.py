import struct
import os
import numpy as np

# Load pre-trained stories260K.bin
bin_path = r"C:\Users\Lenovo\Desktop\GEMMrv32\stories260K.bin"
tok_path = r"C:\Users\Lenovo\Desktop\GEMMrv32\tok512.bin"

# Read vocabulary
with open(tok_path, "rb") as f:
    max_len = struct.unpack("i", f.read(4))[0]
    vocab_strings = []
    for i in range(512):
        score = struct.unpack("f", f.read(4))[0]
        length = struct.unpack("i", f.read(4))[0]
        b = f.read(length)
        try:
            s = b.decode("utf-8")
        except:
            s = " "
        vocab_strings.append(s)

print(f"Loaded {len(vocab_strings)} vocab entries from tok512.bin")

# Read weights
with open(bin_path, "rb") as f:
    dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len = struct.unpack("7i", f.read(28))
    print(f"stories260K header: dim={dim}, hidden_dim={hidden_dim}, n_layers={n_layers}, vocab_size={vocab_size}")

    # 1. Token Embedding Table: (512, 64)
    w_emb = np.fromfile(f, dtype=np.float32, count=vocab_size * dim).reshape(vocab_size, dim)

    # 2. Layer 0 Attention
    # rms_att: (64,)
    rms_att0 = np.fromfile(f, dtype=np.float32, count=dim)
    # wq: (64, 64)
    wq0 = np.fromfile(f, dtype=np.float32, count=dim * dim).reshape(dim, dim)
    # wk: (64, 32) -> pad to (64, 64) for clean systolic tiles
    wk0 = np.fromfile(f, dtype=np.float32, count=dim * 32).reshape(dim, 32)
    # wv: (64, 32) -> pad to (64, 64)
    wv0 = np.fromfile(f, dtype=np.float32, count=dim * 32).reshape(dim, 32)
    # wo: (64, 64)
    wo0 = np.fromfile(f, dtype=np.float32, count=dim * dim).reshape(dim, dim)

    # 3. Layer 0 FFN (SwiGLU)
    rms_ffn0 = np.fromfile(f, dtype=np.float32, count=dim)
    # w1 (gate): (64, 172) -> truncate or pad to 128 or 176 for 8-aligned tiles
    w1_0 = np.fromfile(f, dtype=np.float32, count=dim * hidden_dim).reshape(dim, hidden_dim)
    w2_0 = np.fromfile(f, dtype=np.float32, count=hidden_dim * dim).reshape(hidden_dim, dim)
    w3_0 = np.fromfile(f, dtype=np.float32, count=dim * hidden_dim).reshape(dim, hidden_dim)

print("Layer 0 extracted! Quantizing to INT8 for bare-metal NEORV32 BRAM...")

# Padding
# wk and wv pad cols to 64
wk0_pad = np.zeros((64, 64), dtype=np.float32)
wk0_pad[:, :32] = wk0
wv0_pad = np.zeros((64, 64), dtype=np.float32)
wv0_pad[:, :32] = wv0

# FFN: hidden_dim is 172, pad to 176 (multiple of 8)
w1_pad = np.zeros((64, 176), dtype=np.float32)
w1_pad[:, :hidden_dim] = w1_0

w2_pad = np.zeros((176, 64), dtype=np.float32)
w2_pad[:hidden_dim, :] = w2_0

w3_pad = np.zeros((64, 176), dtype=np.float32)
w3_pad[:, :hidden_dim] = w3_0

# Quantize to INT8
def quant_mat(mat, scale=64.0):
    return np.clip(np.round(mat * scale), -127, 127).astype(np.int8)

q_emb = quant_mat(w_emb, 64.0)
q_wq  = quant_mat(wq0, 64.0)
q_wk  = quant_mat(wk0_pad, 64.0)
q_wv  = quant_mat(wv0_pad, 64.0)
q_wo  = quant_mat(wo0, 64.0)
q_w1  = quant_mat(w1_pad, 64.0)
q_w2  = quant_mat(w2_pad, 64.0)
q_w3  = quant_mat(w3_pad, 64.0)

# Unembedding head is tied to w_emb.T in Llama! (64, 512)
q_head = quant_mat(w_emb.T, 64.0)

out_h = r"C:\Users\Lenovo\Desktop\GEMMrv32\GEMMrv32-main\sw\example\demo_micro_transformer\pretrained_llama_weights.h"

with open(out_h, "w", encoding="utf-8") as f:
    f.write("#ifndef PRETRAINED_LLAMA_WEIGHTS_H\n#define PRETRAINED_LLAMA_WEIGHTS_H\n\n#include <stdint.h>\n\n")
    f.write("#define LLAMA_VOCAB_SIZE  512\n")
    f.write("#define LLAMA_DIM         64\n")
    f.write("#define LLAMA_HIDDEN_DIM  176\n\n")

    # Vocab string table
    f.write("static const char* const LLAMA_VOCAB[512] = {\n")
    for i, s in enumerate(vocab_strings):
        escaped = s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r')
        f.write(f'    "{escaped}",\n')
    f.write("};\n\n")

    def dump_c(name, arr):
        f.write(f"static const int8_t {name}[{arr.size}] __attribute__((aligned(32))) = {{\n")
        flat = arr.flatten()
        for i in range(0, len(flat), 16):
            f.write("    " + ", ".join(str(int(x)) for x in flat[i:i+16]) + ",\n")
        f.write("};\n\n")

    dump_c("LLAMA_W_EMB", q_emb)
    dump_c("LLAMA_W_Q",   q_wq)
    dump_c("LLAMA_W_K",   q_wk)
    dump_c("LLAMA_W_V",   q_wv)
    dump_c("LLAMA_W_O",   q_wo)
    dump_c("LLAMA_W_1",   q_w1)
    dump_c("LLAMA_W_2",   q_w2)
    dump_c("LLAMA_W_3",   q_w3)

    f.write("#endif // PRETRAINED_LLAMA_WEIGHTS_H\n")


print(f"Successfully generated {out_h}!")
