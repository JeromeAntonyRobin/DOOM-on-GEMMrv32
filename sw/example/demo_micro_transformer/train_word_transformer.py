import numpy as np
import os
import re
import collections

# Domain sentences designed for an AI hardware accelerator showcase demo
tech_sentences = [
    "the hardware accelerator executes matrix multiplication with high throughput and low power",
    "the systolic array delivers parallel multiply accumulate operations for deep learning models",
    "the riscv processor offloads tensor computation to the custom hardware coprocessor",
    "direct memory access streams feature maps across the wide bus without cpu bottleneck",
    "neural network inference runs in real time on the embedded fpga edge device",
    "matrix multiplication is the core computational kernel of modern artificial intelligence",
    "the eight by eight systolic array computes sixty four multiply accumulates every cycle",
    "deep learning architectures require high memory bandwidth and parallel matrix execution",
    "the coprocessor performs accelerated linear algebra and convolution layers efficiently",
    "embedded machine learning enables fast intelligent edge processing with low latency",
    "the transformer model attends to previous tokens to predict the next word sequence",
    "hardware acceleration dramatically reduces execution cycles compared to scalar software"
]

corpus_text = " ".join(tech_sentences * 50).lower()
words = corpus_text.split()

# Build exactly 64-word vocabulary (64 is 8x8, perfectly matches our systolic array width!)
vocab_counts = collections.Counter(words)
vocab = [w for w, c in vocab_counts.most_common(64)]
while len(vocab) < 64:
    vocab.append(f"token_{len(vocab)}")

word_to_ix = {w: i for i, w in enumerate(vocab)}
ix_to_word = {i: w for i, w in enumerate(vocab)}

tokens = [word_to_ix[w] for w in words if w in word_to_ix]
print(f"Total tokens in corpus: {len(tokens)}, Vocabulary: {len(vocab)} unique words")

# Transformer Dimensions
V = 64   # Vocab = 64 words (Multiple of 8!)
D = 64   # Embedding Dim = 64 (Matches 8x8 Systolic Array Tile!)
MLP_DIM = 128 # 16 x 8

np.random.seed(42)
W_emb = np.random.randn(V, D) * 0.15
W_q   = np.random.randn(D, D) * 0.15
W_k   = np.random.randn(D, D) * 0.15
W_v   = np.random.randn(D, D) * 0.15
W_o   = np.random.randn(D, D) * 0.15
W_1   = np.random.randn(D, MLP_DIM) * 0.15
W_2   = np.random.randn(MLP_DIM, D) * 0.15
W_head= np.random.randn(D, V) * 0.15

# Adam Optimizer states
params = [W_emb, W_q, W_k, W_v, W_o, W_1, W_2, W_head]
m_opt = [np.zeros_like(p) for p in params]
v_opt = [np.zeros_like(p) for p in params]

print("Training Word-Level Micro-Transformer with Adam...")
seq_len = 8
batch_size = 32
num_steps = 2500
lr = 0.003
beta1, beta2 = 0.9, 0.999

for step in range(num_steps):
    # Sample batch
    batch_X = []
    batch_Y = []
    for _ in range(batch_size):
        idx = np.random.randint(0, len(tokens) - seq_len - 1)
        batch_X.append(tokens[idx : idx + seq_len])
        batch_Y.append(tokens[idx + 1 : idx + seq_len + 1])
        
    X_b = np.array(batch_X) # (B, T)
    Y_b = np.array(batch_Y) # (B, T)
    
    # Forward Pass
    X_emb = W_emb[X_b] # (B, T, D)
    
    Q = np.matmul(X_emb, W_q) # (B, T, D)
    K = np.matmul(X_emb, W_k)
    V_act = np.matmul(X_emb, W_v)
    
    scores = np.matmul(Q, np.transpose(K, (0, 2, 1))) / np.sqrt(D) # (B, T, T)
    mask = np.triu(np.ones((seq_len, seq_len)), k=1) * -1e9
    attn = np.exp(scores + mask - np.max(scores + mask, axis=-1, keepdims=True))
    attn = attn / np.sum(attn, axis=-1, keepdims=True)
    
    attn_out = np.matmul(np.matmul(attn, V_act), W_o)
    x2 = X_emb + attn_out
    
    mlp = np.maximum(0, np.matmul(x2, W_1)) # ReLU (B, T, MLP)
    mlp_out = np.matmul(mlp, W_2)
    x3 = x2 + mlp_out
    
    logits = np.matmul(x3, W_head) # (B, T, V)
    
    # Loss
    probs = np.exp(logits - np.max(logits, axis=-1, keepdims=True))
    probs = probs / np.sum(probs, axis=-1, keepdims=True)
    
    loss = -np.mean(np.log(probs[np.arange(batch_size)[:, None], np.arange(seq_len), Y_b] + 1e-12))
    
    # Backward
    dlogits = probs.copy()
    dlogits[np.arange(batch_size)[:, None], np.arange(seq_len), Y_b] -= 1.0
    dlogits /= (batch_size * seq_len)
    
    dW_head = np.tensordot(x3, dlogits, axes=([0, 1], [0, 1]))
    dx3 = np.matmul(dlogits, W_head.T)
    
    dW_2 = np.tensordot(mlp, dx3, axes=([0, 1], [0, 1]))
    dmlp = np.matmul(dx3, W_2.T) * (mlp > 0)
    dW_1 = np.tensordot(x2, dmlp, axes=([0, 1], [0, 1]))
    
    dx2 = dx3 + np.matmul(dmlp, W_1.T)
    
    dattn_out = dx2
    dW_o = np.tensordot(np.matmul(attn, V_act), dattn_out, axes=([0, 1], [0, 1]))
    dcontext = np.matmul(dattn_out, W_o.T)
    
    dV = np.matmul(np.transpose(attn, (0, 2, 1)), dcontext)
    dW_v = np.tensordot(X_emb, dV, axes=([0, 1], [0, 1]))
    
    dattn = np.matmul(dcontext, np.transpose(V_act, (0, 2, 1)))
    dscores = attn * (dattn - np.sum(dattn * attn, axis=-1, keepdims=True)) / np.sqrt(D)
    
    dQ = np.matmul(dscores, K)
    dK = np.matmul(np.transpose(dscores, (0, 2, 1)), Q)
    
    dW_q = np.tensordot(X_emb, dQ, axes=([0, 1], [0, 1]))
    dW_k = np.tensordot(X_emb, dK, axes=([0, 1], [0, 1]))
    
    dX_emb = dx2 + np.matmul(dQ, W_q.T) + np.matmul(dK, W_k.T) + np.matmul(dV, W_v.T)
    
    dW_emb = np.zeros_like(W_emb)
    np.add.at(dW_emb, X_b.flatten(), dX_emb.reshape(-1, D))
    
    # Adam Update
    grads = [dW_emb, dW_q, dW_k, dW_v, dW_o, dW_1, dW_2, dW_head]
    for p, g, m, v in zip(params, grads, m_opt, v_opt):
        np.clip(g, -1.0, 1.0, out=g)
        m[:] = beta1 * m + (1 - beta1) * g
        v[:] = beta2 * v + (1 - beta2) * (g ** 2)
        m_hat = m / (1 - beta1 ** (step + 1))
        v_hat = v / (1 - beta2 ** (step + 1))
        p -= lr * m_hat / (np.sqrt(v_hat) + 1e-8)
        
    if step % 500 == 0 or step == num_steps - 1:
        print(f"Step {step:4d} | Cross-Entropy Loss: {loss:.4f}")

print("\nTesting text generation on PC:")
test_prompts = ["the hardware", "deep learning", "matrix multiplication", "the systolic"]
for prompt in test_prompts:
    prompt_words = prompt.split()
    gen = list(prompt_words)
    curr_tokens = [word_to_ix.get(w, 0) for w in prompt_words]
    for _ in range(12):
        ctx = curr_tokens[-seq_len:]
        x_emb = W_emb[ctx]
        q = np.dot(x_emb[-1:], W_q)
        k = np.dot(x_emb, W_k)
        v = np.dot(x_emb, W_v)
        s = np.dot(q, k.T) / np.sqrt(D)
        a = np.exp(s - np.max(s)) / np.sum(np.exp(s - np.max(s)))
        ctx_vec = np.dot(a, v)
        out = np.dot(ctx_vec, W_o)
        x2 = x_emb[-1:] + out
        h1 = np.maximum(0, np.dot(x2, W_1))
        h2 = np.dot(h1, W_2)
        x3 = x2 + h2
        logits = np.dot(x3, W_head)[0]
        next_tok = int(np.argmax(logits))
        gen.append(ix_to_word[next_tok])
        curr_tokens.append(next_tok)
    print(f"  [{prompt}] -> {' '.join(gen)}")

print("\nQuantizing weights to INT8...")
scale = 64.0
out_h = r"C:\Users\Lenovo\Desktop\GEMMrv32\GEMMrv32-main\sw\example\demo_micro_transformer\model_weights.h"

with open(out_h, "w") as f:
    f.write("#ifndef MODEL_WEIGHTS_H\n#define MODEL_WEIGHTS_H\n\n#include <stdint.h>\n\n")
    f.write(f"#define VOCAB_SIZE 64\n")
    f.write(f"#define EMBED_DIM  64\n")
    f.write(f"#define MLP_DIM    128\n\n")
    
    # Write vocabulary dictionary
    f.write("static const char* const VOCAB_WORDS[64] = {\n")
    for w in vocab:
        f.write(f'    "{w}",\n')
    f.write("};\n\n")
    
    def write_mat(name, mat):
        q = np.clip(np.round(mat * scale), -127, 127).astype(np.int8)
        f.write(f"static const int8_t {name}[{q.size}] __attribute__((aligned(32))) = {{\n")
        flat = q.flatten()
        for i in range(0, len(flat), 16):
            f.write("    " + ", ".join(str(int(x)) for x in flat[i:i+16]) + ",\n")
        f.write("};\n\n")

    write_mat("W_EMB", W_emb)
    write_mat("W_Q",   W_q)
    write_mat("W_K",   W_k)
    write_mat("W_V",   W_v)
    write_mat("W_O",   W_o)
    write_mat("W_1",   W_1)
    write_mat("W_2",   W_2)
    write_mat("W_HEAD",W_head)
    
    f.write("#endif // MODEL_WEIGHTS_H\n")

print(f"Exported clean weights header to {out_h}")
