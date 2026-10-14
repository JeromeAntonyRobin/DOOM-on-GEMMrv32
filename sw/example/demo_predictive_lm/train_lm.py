import numpy as np
import os
import math

# Corpus covering computer architecture, AI, FPGA, RISC-V, systolic arrays, hardware
sentences = [
    "artificial intelligence on embedded fpga enables real time edge computation",
    "the hardware accelerator executes matrix multiplication with high throughput",
    "the systolic array delivers parallel multiply accumulate operations on edge devices",
    "riscv processor controls direct memory access for neural network inference",
    "deep learning models require efficient tensor computation and low latency",
    "the memory crossbar arbitrates between cpu and systolic coprocessor",
    "neural networks process speech recognition and image classification efficiently",
    "the future of computing is accelerated edge artificial intelligence",
    "embedded hardware runs machine learning with low power consumption",
    "matrix multiplication is the core kernel of modern deep learning",
    "the coprocessor performs accelerated tensor operations using wide bram",
    "systolic architectures enable massive parallel execution for neural networks"
]

# Build vocabulary of frequent words
words_set = set()
for s in sentences:
    for w in s.strip().split():
        words_set.add(w.lower())

unique_words = sorted(list(words_set))
print(f"Total unique words: {len(unique_words)}")

# Target vocabulary size is 64 (multiple of 8 for hardware 8x8 tiling)
# Pad or limit to 64
vocab = unique_words[:64]
# If fewer than 64, pad with common tech words
extra_words = ["system", "data", "model", "algorithm", "speedup", "pipeline", "clock", "memory", 
               "bus", "register", "instruction", "core", "logic", "array", "layer", "weight", 
               "activation", "tensor", "device", "stream", "buffer", "cache", "driver", "kernel"]
for w in extra_words:
    if len(vocab) < 64 and w not in vocab:
        vocab.append(w)
while len(vocab) < 64:
    vocab.append(f"token{len(vocab)}")

vocab = sorted(vocab)
assert len(vocab) == 64, f"Vocab size must be 64, got {len(vocab)}"

word_to_id = {w: i for i, w in enumerate(vocab)}
id_to_word = {i: w for i, w in enumerate(vocab)}

# Ensure "artificial", "intelligence", "neural", "hardware" are in vocab
assert "artificial" in word_to_id
assert "intelligence" in word_to_id
assert "neural" in word_to_id
assert "hardware" in word_to_id

# Prepare training pairs (word_t -> word_{t+1})
train_pairs = []
# Give extra weight to key bigrams
for _ in range(50):
    train_pairs.append((word_to_id["artificial"], word_to_id["intelligence"]))
    train_pairs.append((word_to_id["artificial"], word_to_id["neural"]))
    train_pairs.append((word_to_id["artificial"], word_to_id["hardware"]))

for s in sentences:
    tokens = [w.lower() for w in s.strip().split() if w.lower() in word_to_id]
    for i in range(len(tokens) - 1):
        train_pairs.append((word_to_id[tokens[i]], word_to_id[tokens[i+1]]))

# Repeat corpus to simulate a rich training dataset
train_pairs = train_pairs * 20
print(f"Total training transitions: {len(train_pairs)}")

# Model dimensions
V = 64
H = 64

np.random.seed(42)
W_xh = np.random.randn(H, V) * 0.1
W_hh = np.random.randn(H, H) * 0.1
W_hy = np.random.randn(V, H) * 0.1
b_h  = np.zeros((H, 1))
b_y  = np.zeros((V, 1))

# Adagrad memory
mW_xh = np.zeros_like(W_xh)
mW_hh = np.zeros_like(W_hh)
mW_hy = np.zeros_like(W_hy)
mb_h  = np.zeros_like(b_h)
mb_y  = np.zeros_like(b_y)

lr = 0.05
h_prev = np.zeros((H, 1))

# Training loop
for epoch in range(15):
    total_loss = 0.0
    for w_in, w_out in train_pairs:
        # One-hot input
        x = np.zeros((V, 1))
        x[w_in] = 1.0
        
        # Forward pass: h = tanh(W_xh * x + W_hh * h_prev + b_h)
        h_raw = np.dot(W_xh, x) + np.dot(W_hh, h_prev) + b_h
        h = np.tanh(h_raw)
        
        # Logits: y = W_hy * h + b_y
        logits = np.dot(W_hy, h) + b_y
        
        # Softmax
        exp_logits = np.exp(logits - np.max(logits))
        probs = exp_logits / np.sum(exp_logits)
        
        loss = -np.log(probs[w_out, 0] + 1e-12)
        total_loss += loss
        
        # Backward pass
        dlogits = np.copy(probs)
        dlogits[w_out, 0] -= 1.0
        
        dW_hy = np.dot(dlogits, h.T)
        db_y  = dlogits
        
        dh = np.dot(W_hy.T, dlogits)
        dh_raw = (1.0 - h * h) * dh
        
        dW_xh = np.dot(dh_raw, x.T)
        dW_hh = np.dot(dh_raw, h_prev.T)
        db_h  = dh_raw
        
        # Clip gradients
        for g in [dW_xh, dW_hh, dW_hy, db_h, db_y]:
            np.clip(g, -5.0, 5.0, out=g)
            
        # Adagrad update
        for p, dp, m in zip([W_xh, W_hh, W_hy, b_h, b_y],
                            [dW_xh, dW_hh, dW_hy, db_h, db_y],
                            [mW_xh, mW_hh, mW_hy, mb_h, mb_y]):
            m += dp * dp
            p -= lr * dp / (np.sqrt(m) + 1e-8)
            
        h_prev = h
    print(f"Epoch {epoch+1}/15, Avg Loss: {total_loss / len(train_pairs):.4f}")

# Verification of predictions for "artificial"
x = np.zeros((V, 1))
x[word_to_id["artificial"]] = 1.0
h = np.tanh(np.dot(W_xh, x) + b_h)
logits = np.dot(W_hy, h) + b_y
probs = np.exp(logits - np.max(logits)) / np.sum(np.exp(logits - np.max(logits)))
top_indices = np.argsort(probs[:, 0])[::-1][:5]
print("\nPredictions for 'artificial':")
for idx in top_indices:
    print(f"  {id_to_word[idx]}: {probs[idx, 0]*100:.1f}%")

# Quantize weights to INT8 (-127 to +127)
scale_w = 64.0
W_xh_int8 = np.clip(np.round(W_xh * scale_w), -127, 127).astype(np.int8)
W_hh_int8 = np.clip(np.round(W_hh * scale_w), -127, 127).astype(np.int8)
W_hy_int8 = np.clip(np.round(W_hy * scale_w), -127, 127).astype(np.int8)

# Biases to INT32
b_h_int32 = np.round(b_h * scale_w * scale_w).astype(np.int32).flatten()
b_y_int32 = np.round(b_y * scale_w * scale_w).astype(np.int32).flatten()

# Generate C header
out_dir = r"C:\Users\Lenovo\.gemini\antigravity\scratch\GEMMrv32\GEMMrv32-main\sw\example\demo_predictive_lm"
os.makedirs(out_dir, exist_ok=True)
header_path = os.path.join(out_dir, "lm_weights.h")

with open(header_path, "w") as f:
    f.write("#ifndef LM_WEIGHTS_H\n#define LM_WEIGHTS_H\n\n#include <stdint.h>\n\n")
    f.write(f"#define VOCAB_SIZE {V}\n")
    f.write(f"#define HIDDEN_DIM {H}\n\n")
    
    # Vocabulary array
    f.write("static const char* const VOCAB[VOCAB_SIZE] = {\n")
    for i, w in enumerate(vocab):
        f.write(f'    "{w}", // {i}\n')
    f.write("};\n\n")
    
    # Weights W_xh (64x64)
    f.write("// Input-to-hidden weights (64x64)\n")
    f.write("static const int8_t W_XH[HIDDEN_DIM * VOCAB_SIZE] __attribute__((aligned(32))) = {\n")
    for r in range(H):
        row_str = ", ".join(str(int(W_xh_int8[r, c])) for c in range(V))
        f.write(f"    {row_str},\n")
    f.write("};\n\n")
    
    # Weights W_hh (64x64)
    f.write("// Hidden-to-hidden recurrent weights (64x64)\n")
    f.write("static const int8_t W_HH[HIDDEN_DIM * HIDDEN_DIM] __attribute__((aligned(32))) = {\n")
    for r in range(H):
        row_str = ", ".join(str(int(W_hh_int8[r, c])) for c in range(H))
        f.write(f"    {row_str},\n")
    f.write("};\n\n")
    
    # Weights W_hy (64x64)
    f.write("// Hidden-to-output logits weights (64x64)\n")
    f.write("static const int8_t W_HY[VOCAB_SIZE * HIDDEN_DIM] __attribute__((aligned(32))) = {\n")
    for r in range(V):
        row_str = ", ".join(str(int(W_hy_int8[r, c])) for c in range(H))
        f.write(f"    {row_str},\n")
    f.write("};\n\n")
    
    # Biases
    f.write("// Hidden bias (64)\n")
    f.write("static const int32_t B_H[HIDDEN_DIM] = {\n    " + ", ".join(str(int(v)) for v in b_h_int32) + "\n};\n\n")
    
    f.write("// Output bias (64)\n")
    f.write("static const int32_t B_Y[VOCAB_SIZE] = {\n    " + ", ".join(str(int(v)) for v in b_y_int32) + "\n};\n\n")
    
    f.write("#endif // LM_WEIGHTS_H\n")

print(f"Header generated successfully at: {header_path}")
