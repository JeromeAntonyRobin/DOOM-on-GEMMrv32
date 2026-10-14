#include <stdint.h>
#include <string.h>
#include <neorv32.h>
#include "../gemm_lib/gemmrv_hw.h"
#include "pretrained_llama_weights.h"

#define MAX_CTX 8

// DMA aligned working buffers
static int8_t  buf_X[8 * LLAMA_DIM]         __attribute__((aligned(32))); // 8 x 64
static int32_t buf_Q32[8 * LLAMA_DIM]       __attribute__((aligned(32))); // 8 x 64
static int32_t buf_K32[8 * LLAMA_DIM]       __attribute__((aligned(32))); // 8 x 64
static int32_t buf_V32[8 * LLAMA_DIM]       __attribute__((aligned(32))); // 8 x 64
static int8_t  buf_V8[8 * LLAMA_DIM]        __attribute__((aligned(32))); // 8 x 64
static int8_t  buf_Attn8[8 * 8]             __attribute__((aligned(32))); // 8 x 8
static int32_t buf_Ctx32[8 * LLAMA_DIM]     __attribute__((aligned(32))); // 8 x 64
static int8_t  buf_Ctx8[8 * LLAMA_DIM]      __attribute__((aligned(32))); // 8 x 64
static int32_t buf_Out32[8 * LLAMA_DIM]     __attribute__((aligned(32))); // 8 x 64

// SwiGLU FFN Buffers: Dim = 64, Hidden = 176
static int32_t buf_W1_32[8 * LLAMA_HIDDEN_DIM] __attribute__((aligned(32))); // 8 x 176 (Gate)
static int32_t buf_W3_32[8 * LLAMA_HIDDEN_DIM] __attribute__((aligned(32))); // 8 x 176 (Up)
static int8_t  buf_H_8[8 * LLAMA_HIDDEN_DIM]   __attribute__((aligned(32))); // 8 x 176 (SwiGLU activation)
static int32_t buf_W2_32[8 * LLAMA_DIM]        __attribute__((aligned(32))); // 8 x 64 (Down)

// Unembedding Logits Head: 8 x 512
static int32_t buf_Logits[8 * LLAMA_VOCAB_SIZE] __attribute__((aligned(32))); // 8 x 512

static uint16_t token_history[MAX_CTX];
static int      history_len = 0;

// Simple subword matcher: maps word string to token ID in 512 vocabulary
static int str_to_token(const char* word) {
    for (int i = 0; i < LLAMA_VOCAB_SIZE; i++) {
        if (strcmp(LLAMA_VOCAB[i], word) == 0) return i;
    }
    // Prefix match
    for (int i = 0; i < LLAMA_VOCAB_SIZE; i++) {
        if (strstr(LLAMA_VOCAB[i], word) != NULL) return i;
    }
    return 1; // Default
}

// Forward pass: Evaluates pre-trained Llama layer with 8 Systolic GEMMs
static int forward_pass_llama(uint32_t* gemm_cycles) {
    uint32_t c_start = neorv32_cpu_csr_read(CSR_MCYCLE);

    int T = history_len;
    if (T > 8) T = 8;
    if (T < 1) T = 1;

    for (int i = 0; i < 8 * LLAMA_DIM; i++) buf_X[i] = 0;

    int hist_start = history_len - T;
    for (int t = 0; t < T; t++) {
        int tok = token_history[hist_start + t];
        for (int d = 0; d < LLAMA_DIM; d++) {
            buf_X[t * LLAMA_DIM + d] = LLAMA_W_EMB[tok * LLAMA_DIM + d];
        }
    }

    // Wrap matrices for Systolic Array
    gemmrv_mat mat_X    = GEMMRV_MAT(buf_X, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat mat_WQ   = GEMMRV_MAT(LLAMA_W_Q, LLAMA_DIM, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat mat_WK   = GEMMRV_MAT(LLAMA_W_K, LLAMA_DIM, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat mat_WV   = GEMMRV_MAT(LLAMA_W_V, LLAMA_DIM, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat mat_WO   = GEMMRV_MAT(LLAMA_W_O, LLAMA_DIM, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat mat_W1   = GEMMRV_MAT(LLAMA_W_1, LLAMA_DIM, LLAMA_HIDDEN_DIM, LLAMA_HIDDEN_DIM);
    gemmrv_mat mat_W3   = GEMMRV_MAT(LLAMA_W_3, LLAMA_DIM, LLAMA_HIDDEN_DIM, LLAMA_HIDDEN_DIM);
    gemmrv_mat mat_W2   = GEMMRV_MAT(LLAMA_W_2, LLAMA_HIDDEN_DIM, LLAMA_DIM, LLAMA_DIM);


    gemmrv_mat_out out_Q = GEMMRV_MAT_OUT(buf_Q32, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat_out out_K = GEMMRV_MAT_OUT(buf_K32, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat_out out_V = GEMMRV_MAT_OUT(buf_V32, 8, LLAMA_DIM, LLAMA_DIM);

    // 1-3. Q, K, V Attention Projections on Hardware
    gemmrv_mult(&mat_X, &mat_WQ, &out_Q);
    gemmrv_mult(&mat_X, &mat_WK, &out_K);
    gemmrv_mult(&mat_X, &mat_WV, &out_V);

    for (int i = 0; i < 8 * LLAMA_DIM; i++) {
        int32_t val = buf_V32[i] >> 6;
        if (val > 127) val = 127; else if (val < -128) val = -128;
        buf_V8[i] = (int8_t)val;
    }

    // 4. Scaled Dot-Product Attention
    int last_idx = T - 1;
    int32_t scores[8];
    int32_t max_s = -2000000000;
    for (int j = 0; j < T; j++) {
        int64_t dot = 0;
        for (int d = 0; d < LLAMA_DIM; d++) {
            dot += ((int64_t)buf_Q32[last_idx * LLAMA_DIM + d] >> 6) * ((int64_t)buf_K32[j * LLAMA_DIM + d] >> 6);
        }
        int32_t s = (int32_t)(dot >> 8);
        scores[j] = s;
        if (s > max_s) max_s = s;
    }

    for (int i = 0; i < 8 * 8; i++) buf_Attn8[i] = 0;
    int32_t exp_sum = 0;
    int32_t exp_s[8];
    for (int j = 0; j < T; j++) {
        int32_t diff = scores[j] - max_s;
        if (diff < -8) exp_s[j] = 0;
        else exp_s[j] = (1 << (diff + 8));
        exp_sum += exp_s[j];
    }
    if (exp_sum == 0) exp_sum = 1;

    for (int j = 0; j < T; j++) {
        buf_Attn8[last_idx * 8 + j] = (int8_t)((exp_s[j] * 127) / exp_sum);
    }

    // 5. Context = Attn * V on Hardware
    gemmrv_mat mat_Attn = GEMMRV_MAT(buf_Attn8, 8, 8, 8);
    gemmrv_mat mat_V8   = GEMMRV_MAT(buf_V8, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat_out out_Ctx = GEMMRV_MAT_OUT(buf_Ctx32, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mult(&mat_Attn, &mat_V8, &out_Ctx);

    for (int i = 0; i < 8 * LLAMA_DIM; i++) {
        int32_t val = buf_Ctx32[i] >> 7;
        if (val > 127) val = 127; else if (val < -128) val = -128;
        buf_Ctx8[i] = (int8_t)val;
    }

    // 6. Attention Out: Ctx * W_o on Hardware
    gemmrv_mat mat_Ctx = GEMMRV_MAT(buf_Ctx8, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mat_out out_AttnOut = GEMMRV_MAT_OUT(buf_Out32, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mult(&mat_Ctx, &mat_WO, &out_AttnOut);

    // Residual
    for (int i = 0; i < 8 * LLAMA_DIM; i++) {
        int32_t val = (int32_t)buf_X[i] + (buf_Out32[i] >> 6);
        if (val > 127) val = 127; else if (val < -128) val = -128;
        buf_X[i] = (int8_t)val;
    }

    // 7. SwiGLU Feed-Forward Network: Gate (W1) and Up (W3)
    gemmrv_mat_out out_W1 = GEMMRV_MAT_OUT(buf_W1_32, 8, LLAMA_HIDDEN_DIM, LLAMA_HIDDEN_DIM);
    gemmrv_mat_out out_W3 = GEMMRV_MAT_OUT(buf_W3_32, 8, LLAMA_HIDDEN_DIM, LLAMA_HIDDEN_DIM);
    gemmrv_mult(&mat_X, &mat_W1, &out_W1);
    gemmrv_mult(&mat_X, &mat_W3, &out_W3);

    // SwiGLU activation: (W1 * sigmoid) * W3 -> quantized int8
    for (int i = 0; i < 8 * LLAMA_HIDDEN_DIM; i++) {
        int32_t g = buf_W1_32[i] >> 6;
        int32_t u = buf_W3_32[i] >> 6;
        if (g < 0) g = 0; // ReLU approximation to SiLU
        int32_t prod = (g * u) >> 6;
        if (prod > 127) prod = 127; else if (prod < -128) prod = -128;
        buf_H_8[i] = (int8_t)prod;
    }

    // 8. Down projection: H * W2 on Hardware
    gemmrv_mat mat_H = GEMMRV_MAT(buf_H_8, 8, LLAMA_HIDDEN_DIM, LLAMA_HIDDEN_DIM);
    gemmrv_mat_out out_W2 = GEMMRV_MAT_OUT(buf_W2_32, 8, LLAMA_DIM, LLAMA_DIM);
    gemmrv_mult(&mat_H, &mat_W2, &out_W2);

    for (int i = 0; i < 8 * LLAMA_DIM; i++) {
        int32_t val = (int32_t)buf_X[i] + (buf_W2_32[i] >> 6);
        if (val > 127) val = 127; else if (val < -128) val = -128;
        buf_X[i] = (int8_t)val;
    }

    // 9. Vocabulary Unembedding: X * W_EMB^T (512 logits)
    // Compute dot products directly against tied token embeddings to fit into 128KB ROM
    for (int v = 0; v < LLAMA_VOCAB_SIZE; v++) {
        int32_t acc = 0;
        const int8_t* emb_vec = &LLAMA_W_EMB[v * LLAMA_DIM];
        const int8_t* x_vec   = &buf_X[last_idx * LLAMA_DIM];
        for (int d = 0; d < LLAMA_DIM; d++) {
            acc += (int32_t)x_vec[d] * (int32_t)emb_vec[d];
        }
        buf_Logits[v] = acc;
    }

    // Apply repetition penalty to recently generated tokens
    for (int t = 0; t < T; t++) {
        int prev = token_history[hist_start + t];
        if (prev >= 0 && prev < LLAMA_VOCAB_SIZE) {
            buf_Logits[prev] -= 8000; // Penalize immediate repeats
        }
    }

    *gemm_cycles = neorv32_cpu_csr_read(CSR_MCYCLE) - c_start;

    // Argmax from logits
    int32_t* final_logits = buf_Logits;

    int best_tok = 265; // ' the'
    int32_t max_logit = -2000000000;
    for (int v = 250; v < LLAMA_VOCAB_SIZE; v++) { // Only meaningful English BPE tokens
        if (final_logits[v] > max_logit) {
            max_logit = final_logits[v];
            best_tok = v;
        }
    }

    return best_tok;

}

void generate_llama_story(const char* prompt, int num_tokens) {
    history_len = 0;
    neorv32_uart0_printf("\n[PROMPT]: \"%s\"\n[GENERATION]: %s", prompt, prompt);

    // Tokenize
    char pbuf[64];
    strncpy(pbuf, prompt, sizeof(pbuf) - 1);
    pbuf[sizeof(pbuf) - 1] = '\0';

    char* token = pbuf;
    char* next_space;
    while (token && *token) {
        while (*token == ' ') token++;
        if (!*token) break;
        next_space = strchr(token, ' ');
        if (next_space) *next_space = '\0';

        int tok_id = str_to_token(token);
        if (history_len < MAX_CTX) {
            token_history[history_len++] = tok_id;
        } else {
            for (int i = 0; i < MAX_CTX - 1; i++) token_history[i] = token_history[i + 1];
            token_history[MAX_CTX - 1] = tok_id;
        }

        if (next_space) token = next_space + 1;
        else break;
    }

    uint32_t total_cycles = 0;
    for (int t = 0; t < num_tokens; t++) {
        uint32_t cycles = 0;
        int next_id = forward_pass_llama(&cycles);
        total_cycles += cycles;

        // Print subword string directly
        neorv32_uart0_printf("%s", LLAMA_VOCAB[next_id]);

        if (history_len < MAX_CTX) {
            token_history[history_len++] = next_id;
        } else {
            for (int i = 0; i < MAX_CTX - 1; i++) token_history[i] = token_history[i + 1];
            token_history[MAX_CTX - 1] = next_id;
        }
    }

    neorv32_uart0_printf(".\n");
    neorv32_uart0_printf("\n--- HARDWARE ACCELERATION METRICS ---\n");
    neorv32_uart0_printf("Tokens Generated:            %d\n", num_tokens);
    neorv32_uart0_printf("Average Cycles per Token:    %u\n", total_cycles / num_tokens);
    neorv32_uart0_printf("Systolic Coprocessor Calls:  %d Dense Hardware GEMMs\n", num_tokens * 8);
    neorv32_uart0_printf("------------------------------------\n");
}

int main(void) {
    neorv32_gpio_port_set(0x01);
    neorv32_uart0_setup(19200, 0);
    neorv32_gpio_port_set(0x0F);

    neorv32_uart0_printf("\n\n=======================================================\n");
    neorv32_uart0_printf("=== NEORV32 8x8 SYSTOLIC ARRAY PRE-TRAINED LLAMA-2 ===\n");
    neorv32_uart0_printf("=== Weights: Pre-Trained stories260K (TinyStories)   ===\n");
    neorv32_uart0_printf("=== Accelerated: 8 Systolic GEMMs per Generated Word===\n");
    neorv32_uart0_printf("=======================================================\n");

    // Demonstration run
    generate_llama_story("Once upon a time", 24);
    generate_llama_story("An FPGA is", 24);

    while (1) {
        neorv32_uart0_printf("\nEnter prompt (or press Enter for default):\n> ");
        char buf[64];
        int idx = 0;
        while (1) {
            char c = neorv32_uart0_getc();
            if (c == '\r' || c == '\n') {
                buf[idx] = '\0';
                break;
            } else if (c == 8 || c == 127) {
                if (idx > 0) {
                    idx--;
                    neorv32_uart0_putc('\b');
                    neorv32_uart0_putc(' ');
                    neorv32_uart0_putc('\b');
                }
            } else if (idx < 62) {
                buf[idx++] = c;
                neorv32_uart0_putc(c);
            }
        }

        if (idx == 0) {
            generate_llama_story("Once there was", 24);
        } else {
            generate_llama_story(buf, 24);
        }
    }

    return 0;
}
