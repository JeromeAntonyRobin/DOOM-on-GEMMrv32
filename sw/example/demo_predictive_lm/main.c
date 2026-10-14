#include <stdint.h>
#include <string.h>
#include <neorv32.h>
#include "lm_weights.h"
#include "../gemm_lib/gemmrv_hw.h"

#define BAUD_RATE 19200

// Internal buffers placed in wide_bram (DMEM)
static int8_t  vec_x[64 * 8]      __attribute__((aligned(32))); // One-hot or input embedding (padded to 8 cols)
static int8_t  vec_h_in[64 * 8]   __attribute__((aligned(32))); // Current hidden state
static int32_t vec_h_raw[64 * 8]  __attribute__((aligned(32))); // Raw accumulator output
static int32_t vec_logits[64 * 8] __attribute__((aligned(32))); // Output logits

// Current hidden state of the RNN
static int8_t h_state[64] __attribute__((aligned(32)));

// Softmax probabilities (fixed point 0..100)
static int probs_pct[VOCAB_SIZE];

// Simple integer square root / tanh approximation
static inline int8_t fast_tanh(int32_t val) {
    // val is scaled by ~4096 from INT8 matrix mult
    int32_t x = val >> 6; // approximate scaling
    if (x > 127)  return 127;
    if (x < -127) return -127;
    return (int8_t)x;
}

// Compute next-word predictions given input word token ID
static void run_rnn_step(int token_id, int use_hw, uint32_t* elapsed_cycles) {
    uint32_t start = neorv32_cpu_csr_read(CSR_MCYCLE);

    // 1. Prepare input vector (one-hot into vec_x with stride 8)
    memset(vec_x, 0, sizeof(vec_x));
    if (token_id >= 0 && token_id < VOCAB_SIZE) {
        vec_x[token_id * 8] = 64; // One-hot unit magnitude
    }

    // Prepare previous hidden state into vec_h_in (stride 8)
    memset(vec_h_in, 0, sizeof(vec_h_in));
    for (int i = 0; i < HIDDEN_DIM; i++) {
        vec_h_in[i * 8] = h_state[i];
    }

    if (use_hw) {
        // HW Matrix Multiplication using the 8x8 Systolic Coprocessor:
        // A = W_XH (64x64, stride 64), B = vec_x (64x8, stride 8), C = vec_h_raw (64x8, stride 8)
        gemmrv_mat A_in = GEMMRV_MAT(W_XH, 64, 64, 64);
        gemmrv_mat B_in = GEMMRV_MAT(vec_x, 64, 8, 8);
        gemmrv_mat_out C_in = GEMMRV_MAT_OUT(vec_h_raw, 64, 8, 8);
        gemmrv_mult(&A_in, &B_in, &C_in);

        // Recurrent contribution: W_HH * h_state
        // Note: For demonstration, apply recurrent state update
        for (int i = 0; i < HIDDEN_DIM; i++) {
            int32_t acc = vec_h_raw[i * 8] + B_H[i];
            for (int k = 0; k < HIDDEN_DIM; k++) {
                acc += ((int32_t)W_HH[i * 64 + k] * (int32_t)h_state[k]) >> 6;
            }
            h_state[i] = fast_tanh(acc);
            vec_h_in[i * 8] = h_state[i];
        }

        // Output Logits: W_HY * h_state
        gemmrv_mat A_out = GEMMRV_MAT(W_HY, 64, 64, 64);
        gemmrv_mat B_out = GEMMRV_MAT(vec_h_in, 64, 8, 8);
        gemmrv_mat_out C_out = GEMMRV_MAT_OUT(vec_logits, 64, 8, 8);
        gemmrv_mult(&A_out, &B_out, &C_out);
    } else {
        // Pure scalar CPU software fallback
        for (int i = 0; i < HIDDEN_DIM; i++) {
            int32_t acc = B_H[i];
            for (int k = 0; k < VOCAB_SIZE; k++) {
                acc += (int32_t)W_XH[i * 64 + k] * (int32_t)vec_x[k * 8];
            }
            for (int k = 0; k < HIDDEN_DIM; k++) {
                acc += ((int32_t)W_HH[i * 64 + k] * (int32_t)h_state[k]) >> 6;
            }
            h_state[i] = fast_tanh(acc);
            vec_h_in[i * 8] = h_state[i];
        }

        for (int i = 0; i < VOCAB_SIZE; i++) {
            int32_t acc = B_Y[i];
            for (int k = 0; k < HIDDEN_DIM; k++) {
                acc += (int32_t)W_HY[i * 64 + k] * (int32_t)vec_h_in[k * 8];
            }
            vec_logits[i * 8] = acc;
        }
    }

    // 2. Fixed-point Softmax to compute percentages (0..100)
    int32_t max_logit = -2000000000;
    for (int i = 0; i < VOCAB_SIZE; i++) {
        if (vec_logits[i * 8] > max_logit) {
            max_logit = vec_logits[i * 8];
        }
    }

    int32_t sum_exp = 0;
    int32_t exps[VOCAB_SIZE];
    for (int i = 0; i < VOCAB_SIZE; i++) {
        int32_t diff = (vec_logits[i * 8] - max_logit);
        if (diff < -10000) {
            exps[i] = 1;
        } else {
            int32_t step = diff / 1500; // 0, -1, -2, -3...
            if (step > 0) step = 0;
            if (step < -7) step = -7;
            exps[i] = 256 >> (-step);
        }
        sum_exp += exps[i];
    }

    if (sum_exp <= 0) sum_exp = 1;
    for (int i = 0; i < VOCAB_SIZE; i++) {
        probs_pct[i] = (int)((exps[i] * 100) / sum_exp);
    }

    uint32_t end = neorv32_cpu_csr_read(CSR_MCYCLE);
    if (elapsed_cycles) {
        *elapsed_cycles = end - start;
    }
}

// Find vocabulary index by word string
static int find_token(const char* word) {
    if (!word || strlen(word) == 0) return -1;
    for (int i = 0; i < VOCAB_SIZE; i++) {
        if (strcmp(word, VOCAB[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// Filter and Rank Top 3 tokens matching prefix
static void get_top3(const char* prefix, int* top_indices, int* top_probs) {
    int top1 = -1, top2 = -1, top3 = -1;
    int p1 = -1, p2 = -1, p3 = -1;
    int prefix_len = prefix ? strlen(prefix) : 0;

    for (int i = 0; i < VOCAB_SIZE; i++) {
        // Check prefix match
        if (prefix_len > 0) {
            if (strncmp(VOCAB[i], prefix, prefix_len) != 0) {
                continue;
            }
        }
        int p = probs_pct[i];
        if (p > p1) {
            p3 = p2; top3 = top2;
            p2 = p1; top2 = top1;
            p1 = p;  top1 = i;
        } else if (p > p2) {
            p3 = p2; top3 = top2;
            p2 = p;  top2 = i;
        } else if (p > p3) {
            p3 = p;  top3 = i;
        }
    }

    // Fallbacks if fewer than 3 matched prefix
    if (top1 == -1) top1 = 0;
    if (top2 == -1) top2 = (top1 == 0) ? 1 : 0;
    if (top3 == -1) top3 = (top2 == 1) ? 2 : 1;

    top_indices[0] = top1; top_probs[0] = (p1 >= 0) ? p1 : 5;
    top_indices[1] = top2; top_probs[1] = (p2 >= 0) ? p2 : 3;
    top_indices[2] = top3; top_probs[2] = (p3 >= 0) ? p3 : 2;
}

// Render dynamic suggestion bar right below the typing prompt
static void render_suggestions(const char* prefix) {
    int top_idx[3];
    int top_pct[3];
    get_top3(prefix, top_idx, top_pct);

    // Normalize probabilities so top 3 look realistic
    int total = top_pct[0] + top_pct[1] + top_pct[2];
    if (total == 0) total = 100;
    int p0 = (top_pct[0] * 100) / total;
    int p1 = (top_pct[1] * 100) / total;
    int p2 = 100 - p0 - p1;
    if (p2 < 0) p2 = 0;

    // ANSI escape sequences:
    // \033[s    : Save cursor position
    // \033[B    : Move down 1 line
    // \033[2K\r : Clear line and return to column 0
    // \033[u    : Restore cursor position
    neorv32_uart0_printf("\033[s\n\r\033[2KTop Suggestions: [1] %s (%d%%)  [2] %s (%d%%)  [3] %s (%d%%)\033[u",
                         VOCAB[top_idx[0]], p0,
                         VOCAB[top_idx[1]], p1,
                         VOCAB[top_idx[2]], p2);
}

int main(void) {
    neorv32_rte_setup();
    neorv32_uart0_setup(BAUD_RATE, 0);

    // Initial hardware reset of hidden state
    memset(h_state, 0, sizeof(h_state));

    neorv32_uart0_printf("\n\n\033[2J\033[H"); // Clear screen
    neorv32_uart0_printf("=================================================================\n\r");
    neorv32_uart0_printf("===  GEMMrv32 LIVE PREDICTIVE LANGUAGE MODEL (8x8 SYSTOLIC)  ===\n\r");
    neorv32_uart0_printf("===  Interactive Autocomplete & Next-Word Generation Engine   ===\n\r");
    neorv32_uart0_printf("=================================================================\n\r");
    neorv32_uart0_printf("Hardware: 8x8 Systolic Array (64 MACs @ 50MHz, 256-bit DMA)\n\r");
    neorv32_uart0_printf("Instructions: Type characters; watch suggestions update LIVE below!\n\r");
    neorv32_uart0_printf("              Press SPACE/ENTER to accept word, TAB for auto-complete.\n\r");
    neorv32_uart0_printf("=================================================================\n\r\n\r");

    char current_word[32];
    int word_len = 0;
    memset(current_word, 0, sizeof(current_word));

    // Initialize model with a default forward pass
    uint32_t hw_cycles = 0;
    run_rnn_step(find_token("the"), 1, &hw_cycles);

    neorv32_uart0_printf(">>> ");
    render_suggestions("");

    while (1) {
        char c = neorv32_uart0_getc();

        if (c == '\r' || c == '\n') {
            if (word_len > 0) {
                // User pressed ENTER on a typed word -> Run sentence completion
                neorv32_uart0_printf("\n\r\n\r[Auto-Completing Sentence with 8x8 Systolic Coprocessor...]\n\r");
                int token = find_token(current_word);
                if (token < 0) token = find_token("artificial");
                
                neorv32_uart0_printf("Generated: %s", VOCAB[token]);
                for (int step = 0; step < 8; step++) {
                    uint32_t cyc = 0;
                    run_rnn_step(token, 1, &cyc);
                    int top_idx[3], top_pct[3];
                    get_top3(NULL, top_idx, top_pct);
                    token = top_idx[0];
                    neorv32_uart0_printf(" %s", VOCAB[token]);
                }
                neorv32_uart0_printf(".\n\r\n\r>>> ");
                word_len = 0;
                current_word[0] = '\0';
                render_suggestions("");
            } else {
                neorv32_uart0_printf("\n\r>>> ");
                render_suggestions("");
            }
        }
        else if (c == ' ') {
            // Space pressed: process completed word token through RNN
            current_word[word_len] = '\0';
            int token = find_token(current_word);
            neorv32_uart0_putc(' ');

            if (token >= 0) {
                // Run forward pass through hardware systolic array
                run_rnn_step(token, 1, &hw_cycles);
                // Reset current typing word buffer
                word_len = 0;
                current_word[0] = '\0';
                render_suggestions(NULL); // Show predicted NEXT words!
            } else {
                word_len = 0;
                current_word[0] = '\0';
                render_suggestions("");
            }
        }
        else if (c == '\t') {
            // Tab pressed: Autocomplete with top suggestion #1
            int top_idx[3], top_pct[3];
            get_top3(current_word, top_idx, top_pct);
            const char* full_word = VOCAB[top_idx[0]];
            
            // Print remaining characters
            for (int i = word_len; full_word[i] != '\0'; i++) {
                neorv32_uart0_putc(full_word[i]);
            }
            neorv32_uart0_putc(' ');

            // Feed completed token into model
            run_rnn_step(top_idx[0], 1, &hw_cycles);
            word_len = 0;
            current_word[0] = '\0';
            render_suggestions("");
        }
        else if (c == 0x7F || c == '\b') {
            // Backspace handled
            if (word_len > 0) {
                word_len--;
                current_word[word_len] = '\0';
                neorv32_uart0_printf("\b \b");
                render_suggestions(current_word);
            }
        }
        else if (c >= 'a' && c <= 'z') {
            if (word_len < 30) {
                current_word[word_len++] = c;
                current_word[word_len] = '\0';
                neorv32_uart0_putc(c);

                // If user just finished typing a full vocabulary word, predict NEXT words immediately
                int exact_token = find_token(current_word);
                if (exact_token >= 0) {
                    run_rnn_step(exact_token, 1, &hw_cycles);
                    render_suggestions(NULL); // show next word predictions!
                } else {
                    render_suggestions(current_word); // show prefix completions
                }
            }
        }
        else if (c >= 'A' && c <= 'Z') {
            char lower_c = c + ('a' - 'A');
            if (word_len < 30) {
                current_word[word_len++] = lower_c;
                current_word[word_len] = '\0';
                neorv32_uart0_putc(lower_c);
                int exact_token = find_token(current_word);
                if (exact_token >= 0) {
                    run_rnn_step(exact_token, 1, &hw_cycles);
                    render_suggestions(NULL);
                } else {
                    render_suggestions(current_word);
                }
            }
        }
    }

    return 0;
}
