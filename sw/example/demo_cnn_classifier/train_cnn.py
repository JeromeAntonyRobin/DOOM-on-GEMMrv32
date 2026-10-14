import numpy as np
import os
import cv2

# Classes: 10 Digits ('0'-'9') + 26 Uppercase Letters ('A'-'Z') = 36 Classes
NUM_CLASSES = 36
CLASSES = [str(i) for i in range(10)] + [chr(ord('A') + i) for i in range(26)]
print(f"Total Classes: {NUM_CLASSES} -> {CLASSES}")

# Generate Synthetic 28x28 rendered dataset for the 36 classes using OpenCV fonts
def generate_synthetic_samples(cls_idx, count=80):
    char = CLASSES[cls_idx]
    samples = []
    fonts = [
        cv2.FONT_HERSHEY_SIMPLEX,
        cv2.FONT_HERSHEY_PLAIN,
        cv2.FONT_HERSHEY_DUPLEX,
        cv2.FONT_HERSHEY_COMPLEX,
        cv2.FONT_HERSHEY_TRIPLEX
    ]
    for _ in range(count):
        img = np.zeros((48, 48), dtype=np.uint8)
        font = np.random.choice(fonts)
        scale = np.random.uniform(0.9, 1.3)
        thickness = np.random.choice([1, 2, 3])
        
        # Text size
        (tw, th), baseline = cv2.getTextSize(char, font, scale, thickness)
        x = max(0, (48 - tw) // 2 + np.random.randint(-4, 5))
        y = max(th, (48 + th) // 2 + np.random.randint(-4, 5))
        cv2.putText(img, char, (x, y), font, scale, 255, thickness, cv2.LINE_AA)
        
        # Slight rotation / shift
        angle = np.random.uniform(-15, 15)
        M = cv2.getRotationMatrix2D((24, 24), angle, 1.0)
        img = cv2.warpAffine(img, M, (48, 48))
        
        # Resize to 28x28
        img28 = cv2.resize(img, (28, 28), interpolation=cv2.INTER_AREA)
        
        # Normalize to [-1, 1] range float
        norm = (img28.astype(np.float32) / 127.5) - 1.0
        samples.append(norm)
    return np.array(samples)

print("Synthesizing dataset...")
X_train, y_train = [], []
for c in range(NUM_CLASSES):
    s = generate_synthetic_samples(c, count=100)
    X_train.append(s)
    y_train.append(np.full((100,), c, dtype=np.int32))

X_train = np.concatenate(X_train, axis=0) # (3600, 28, 28)
y_train = np.concatenate(y_train, axis=0)

# Shuffle
perm = np.random.permutation(len(X_train))
X_train = X_train[perm]
y_train = y_train[perm]

# Model Architecture:
# Conv1: 8 filters of 5x5 (stride 2) -> output 12x12x8
# Flatten: 12*12*8 = 1152 -> directly or with Conv2:
# Let's do:
# Input: 28x28
# im2col Conv1: K=25 (5x5), N=8 filters. M=12*12=144. Output = 144 x 8 -> Reshape to 12 x 12 x 8
# Conv2 (stride 2): K=8*3*3 = 72, N=16 filters (or 16 filters). M=5*5 = 25. Output = 25 x 16 -> 400 features
# Dense FC: 400 -> 36 classes (padded to 40 classes for 8-byte stride alignment!)
# This is a pure standard TFLite Micro CNN!

print("Training CNN with NumPy Backprop...")
np.random.seed(42)

# Weights
# Conv1: (25, 8)
W_c1 = np.random.randn(25, 8) * 0.1
b_c1 = np.zeros(8)

# Conv2: (3x3x8, 16) -> (72, 16)
W_c2 = np.random.randn(72, 16) * 0.1
b_c2 = np.zeros(16)

# Dense: 5*5*16 = 400 -> 40 (padded from 36)
OUT_DIM = 40
W_fc = np.random.randn(400, OUT_DIM) * 0.1
b_fc = np.zeros(OUT_DIM)

# Helpers for im2col
def im2col_conv1(x):
    # x: (28, 28), filter 5x5, stride 2, no pad -> out 12x12 = 144
    cols = []
    for r in range(0, 24, 2):
        for c in range(0, 24, 2):
            cols.append(x[r:r+5, c:c+5].flatten())
    return np.array(cols) # (144, 25)

def im2col_conv2(feat):
    # feat: (12, 12, 8), filter 3x3x8 = 72, stride 2, no pad -> out 5x5 = 25
    cols = []
    for r in range(0, 9, 2):
        for c in range(0, 9, 2):
            cols.append(feat[r:r+3, c:c+3, :].flatten())
    return np.array(cols) # (25, 72)

# Training loop
lr = 0.01
batch_size = 32
num_epochs = 15

for epoch in range(num_epochs):
    epoch_loss = 0.0
    correct = 0
    total = 0
    for b in range(0, len(X_train), batch_size):
        xb = X_train[b:b+batch_size]
        yb = y_train[b:b+batch_size]
        
        # Accumulate grads
        g_W_c1, g_b_c1 = np.zeros_like(W_c1), np.zeros_like(b_c1)
        g_W_c2, g_b_c2 = np.zeros_like(W_c2), np.zeros_like(b_c2)
        g_W_fc, g_b_fc = np.zeros_like(W_fc), np.zeros_like(b_fc)
        
        for i in range(len(xb)):
            # Forward
            col1 = im2col_conv1(xb[i]) # (144, 25)
            c1_out = np.maximum(0, np.dot(col1, W_c1) + b_c1) # ReLU, (144, 8)
            feat1 = c1_out.reshape(12, 12, 8)
            
            col2 = im2col_conv2(feat1) # (25, 72)
            c2_out = np.maximum(0, np.dot(col2, W_c2) + b_c2) # ReLU, (25, 16)
            feat2_flat = c2_out.flatten() # (400,)
            
            logits = np.dot(feat2_flat, W_fc) + b_fc # (40,)
            
            # Softmax
            exp_l = np.exp(logits - np.max(logits))
            probs = exp_l / np.sum(exp_l)
            
            target = yb[i]
            loss = -np.log(probs[target] + 1e-10)
            epoch_loss += loss
            if np.argmax(probs[:36]) == target:
                correct += 1
            total += 1
            
            # Backward
            dlogits = probs.copy()
            dlogits[target] -= 1.0
            
            g_W_fc += np.outer(feat2_flat, dlogits)
            g_b_fc += dlogits
            
            dfeat2 = np.dot(W_fc, dlogits) # (400,)
            dc2_out = dfeat2.reshape(25, 16) * (c2_out > 0)
            
            g_W_c2 += np.dot(col2.T, dc2_out)
            g_b_c2 += np.sum(dc2_out, axis=0)
            
            dcol2 = np.dot(dc2_out, W_c2.T) # (25, 72)
            dfeat1 = np.zeros((12, 12, 8))
            idx = 0
            for r in range(0, 9, 2):
                for c in range(0, 9, 2):
                    dfeat1[r:r+3, c:c+3, :] += dcol2[idx].reshape(3, 3, 8)
                    idx += 1
                    
            dc1_out = dfeat1.reshape(144, 8) * (c1_out > 0)
            g_W_c1 += np.dot(col1.T, dc1_out)
            g_b_c1 += np.sum(dc1_out, axis=0)
            
        W_fc -= (lr / len(xb)) * g_W_fc
        b_fc -= (lr / len(xb)) * g_b_fc
        W_c2 -= (lr / len(xb)) * g_W_c2
        b_c2 -= (lr / len(xb)) * g_b_c2
        W_c1 -= (lr / len(xb)) * g_W_c1
        b_c1 -= (lr / len(xb)) * g_b_c1
        
    print(f"Epoch {epoch+1}/{num_epochs} - Loss: {epoch_loss/total:.4f} - Acc: {correct/total*100:.1f}%")

print("Training finished! Quantizing weights to INT8...")

# Quantization
def quant(mat, scale):
    return np.clip(np.round(mat * scale), -127, 127).astype(np.int8)

scale = 64.0
W_c1_q = quant(W_c1, scale)
W_c2_q = quant(W_c2, scale)
W_fc_q = quant(W_fc, scale)

b_c1_q = np.round(b_c1 * scale * scale).astype(np.int32)
b_c2_q = np.round(b_c2 * scale * scale).astype(np.int32)
b_fc_q = np.round(b_fc * scale * scale).astype(np.int32)

out_h = r"C:\Users\Lenovo\.gemini\antigravity\scratch\GEMMrv32\GEMMrv32-main\sw\example\demo_cnn_classifier\cnn_model_weights.h"

with open(out_h, "w") as f:
    f.write("#ifndef CNN_MODEL_WEIGHTS_H\n#define CNN_MODEL_WEIGHTS_H\n\n#include <stdint.h>\n\n")
    f.write(f"#define CNN_NUM_CLASSES 36\n")
    f.write(f"#define CNN_OUT_DIM     40\n\n")
    f.write('static const char CNN_CLASSES[36+1] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";\n\n')
    
    def write_arr(name, arr, dtype="int8_t"):
        f.write(f"static const {dtype} {name}[{arr.size}] __attribute__((aligned(32))) = {{\n")
        flat = arr.flatten()
        stride = 16 if dtype == "int8_t" else 8
        for i in range(0, len(flat), stride):
            f.write("    " + ", ".join(str(int(x)) for x in flat[i:i+stride]) + ",\n")
        f.write("};\n\n")
        
    # Weights & Biases
    # Conv1: W (25, 8) -> padded K to 32 for clean 8x8 hardware systolic tiles!
    # Padding W_c1 from (25, 8) to (32, 8)
    W_c1_pad = np.zeros((32, 8), dtype=np.int8)
    W_c1_pad[:25, :8] = W_c1_q
    write_arr("W_CONV1", W_c1_pad)
    write_arr("B_CONV1", b_c1_q, "int32_t")
    
    # Conv2: W (72, 16) -> 72 is exactly 9 * 8, 16 is 2 * 8! Perfect multiple of 8!
    write_arr("W_CONV2", W_c2_q)
    write_arr("B_CONV2", b_c2_q, "int32_t")
    
    # FC: W (400, 40) -> 400 is 50 * 8, 40 is 5 * 8! Perfect multiple of 8!
    write_arr("W_FC", W_fc_q)
    write_arr("B_FC", b_fc_q, "int32_t")
    
    f.write("#endif // CNN_MODEL_WEIGHTS_H\n")

print(f"Generated {out_h} successfully!")
