import numpy as np
import os
import math

# Technical & Computer Architecture Corpus
corpus = """
the hardware accelerator executes matrix multiplication with high throughput.
the systolic array delivers parallel multiply accumulate operations on edge devices.
the riscv processor controls direct memory access for neural network inference.
deep learning models require efficient tensor computation and low latency.
the memory crossbar arbitrates between cpu and systolic coprocessor.
artificial intelligence on embedded fpga enables real time edge computation.
neural networks process speech recognition and image classification efficiently.
the future of computing is accelerated edge artificial intelligence.
embedded hardware runs machine learning with low power consumption.
matrix multiplication is the core kernel of modern deep learning.
the coprocessor performs accelerated tensor operations using wide bram.
systolic architectures enable massive parallel execution for neural networks.
""" * 40

# Byte/Char vocabulary (ASCII printable: lowercase, digits, spaces, punctuation)
chars = sorted(list(set(corpus)))
vocab_size = ((len(chars) + 7) // 8) * 8 # multiple of 8 (e.g. 32 or 40)
while len(chars) < vocab_size:
    chars.append(chr(len(chars) + 32))

char_to_ix = {ch: i for i, ch in enumerate(chars)}
ix_to_char = {i: ch for i, ch in enumerate(chars)}
V = vocab_size
D = 64 # Embedding / Attention dimension (exactly 8x8 hardware systolic tile)

print(f"Corpus length: {len(corpus)}, Vocab: {V}, Dim: {D}")

# Micro-Transformer Architecture:
# 1. Token Embedding: (V, D)
# 2. Self-Attention: W_q (D, D), W_k (D, D), W_v (D, D), W_o (D, D)
# 3. MLP: W_1 (D, 2*D), W_2 (2*D, D)
# 4. Unembedding / Logits Head: W_head (D, V)
np.random.seed(42)
W_emb = np.random.randn(V, D) * 0.1
W_q   = np.random.randn(D, D) * 0.1
W_k   = np.random.randn(D, D) * 0.1
W_v   = np.random.randn(D, D) * 0.1
W_o   = np.random.randn(D, D) * 0.1
W_1   = np.random.randn(D, 128) * 0.1
W_2   = np.random.randn(128, D) * 0.1
W_head= np.random.randn(D, V) * 0.1

# Train a character language model directly with sequence autoregression
# For rapid zero-token training, we train a 2-step attention block
mW_q, mW_k, mW_v, mW_o = [np.zeros_like(w) for w in [W_q, W_k, W_v, W_o]]
mW_1, mW_2, mW_head = [np.zeros_like(w) for w in [W_1, W_2, W_head]]

seq_len = 16
lr = 0.08

for step in range(1200):
    idx = (step * seq_len) % (len(corpus) - seq_len - 1)
    chunk = corpus[idx:idx+seq_len+1]
    tokens = [char_to_ix[c] for c in chunk]
    
    # Forward: Embed
    x = W_emb[tokens[:-1]] # (seq_len, D)
    
    # Self-attention projections
    Q = np.dot(x, W_q)
    K = np.dot(x, W_k)
    V_act = np.dot(x, W_v)
    
    scores = np.dot(Q, K.T) / np.sqrt(D)
    # Causal mask
    mask = np.triu(np.ones((seq_len, seq_len)), k=1) * -1e9
    attn = np.exp(scores + mask - np.max(scores + mask, axis=-1, keepdims=True))
    attn = attn / np.sum(attn, axis=-1, keepdims=True)
    
    attn_out = np.dot(np.dot(attn, V_act), W_o)
    x2 = x + attn_out
    
    # MLP
    mlp = np.maximum(0, np.dot(x2, W_1)) # ReLU
    mlp_out = np.dot(mlp, W_2)
    x3 = x2 + mlp_out
    
    # Head logits
    logits = np.dot(x3, W_head)
    
    # Targets
    targets = tokens[1:]
    probs = np.exp(logits - np.max(logits, axis=-1, keepdims=True))
    probs = probs / np.sum(probs, axis=-1, keepdims=True)
    
    loss = -np.mean(np.log(probs[np.arange(seq_len), targets] + 1e-12))
    
    # Simple gradient update step for key projections
    dlogits = probs
    dlogits[np.arange(seq_len), targets] -= 1.0
    dlogits /= seq_len
    
    dW_head = np.dot(x3.T, dlogits)
    dx3 = np.dot(dlogits, W_head.T)
    dW_2 = np.dot(mlp.T, dx3)
    dmlp = np.dot(dx3, W_2.T) * (mlp > 0)
    dW_1 = np.dot(x2.T, dmlp)
    
    for p, dp, m in zip([W_head, W_2, W_1], [dW_head, dW_2, dW_1], [mW_head, mW_2, mW_1]):
        np.clip(dp, -5, 5, out=dp)
        m += dp * dp
        p -= lr * dp / (np.sqrt(m) + 1e-8)
        
    if step % 400 == 0:
        print(f"Step {step}, Loss: {loss:.4f}")

print("Training finished.")

# Quantize to INT8
scale = 64.0
W_emb_q = np.clip(np.round(W_emb * scale), -127, 127).astype(np.int8)
W_q_q   = np.clip(np.round(W_q * scale), -127, 127).astype(np.int8)
W_k_q   = np.clip(np.round(W_k * scale), -127, 127).astype(np.int8)
W_v_q   = np.clip(np.round(W_v * scale), -127, 127).astype(np.int8)
W_o_q   = np.clip(np.round(W_o * scale), -127, 127).astype(np.int8)
W_1_q   = np.clip(np.round(W_1 * scale), -127, 127).astype(np.int8)
W_2_q   = np.clip(np.round(W_2 * scale), -127, 127).astype(np.int8)
W_head_q= np.clip(np.round(W_head * scale), -127, 127).astype(np.int8)

out_dir = r"C:\Users\Lenovo\.gemini\antigravity\scratch\GEMMrv32\GEMMrv32-main\sw\example\demo_micro_transformer"
os.makedirs(out_dir, exist_ok=True)
h_path = os.path.join(out_dir, "model_weights.h")

with open(h_path, "w") as f:
    f.write("#ifndef MODEL_WEIGHTS_H\n#define MODEL_WEIGHTS_H\n\n#include <stdint.h>\n\n")
    f.write(f"#define VOCAB_SIZE {V}\n")
    f.write(f"#define EMBED_DIM  {D}\n")
    f.write(f"#define MLP_DIM    128\n\n")
    
    # Vocab table
    vocab_escaped = "".join([f"\\x{ord(c):02x}" if ord(c) < 32 or ord(c) > 126 or c in ('"', '\\') else c for c in chars])
    f.write(f'static const char VOCAB_CHARS[{V}+1] = "{vocab_escaped}";\n\n')
    
    def write_mat(name, mat):
        f.write(f"static const int8_t {name}[{mat.size}] __attribute__((aligned(32))) = {{\n")
        flat = mat.flatten()
        for i in range(0, len(flat), 16):
            f.write("    " + ", ".join(str(int(x)) for x in flat[i:i+16]) + ",\n")
        f.write("};\n\n")
        
    write_mat("W_EMB", W_emb_q)
    write_mat("W_Q",   W_q_q)
    write_mat("W_K",   W_k_q)
    write_mat("W_V",   W_v_q)
    write_mat("W_O",   W_o_q)
    write_mat("W_1",   W_1_q)
    write_mat("W_2",   W_2_q)
    write_mat("W_HEAD",W_head_q)
    
    f.write("#endif // MODEL_WEIGHTS_H\n")

print(f"Exported model_weights.h to {h_path}")
