#include "../src/conv/conv2d.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON 1e-4f

#if defined(MODE_SIMD)
void test_memory_alignment(const float *ptr) {
    assert(ptr != NULL && "Pointer must not be NULL");
    assert(((uintptr_t)ptr % 32) == 0 &&
           "Pointer must be strictly 32-byte aligned!");
}
#endif

// Scalar baseline reference (Ground Truth)
static void scalar_reference(const float *in, float *out, int N,
                             const float k[3][3]) {
    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            float sum = 0.0f;
            for (int kr = -1; kr <= 1; kr++) {
                for (int kc = -1; kc <= 1; kc++) {
                    sum += in[(r + kr) * N + (c + kc)] * k[kr + 1][kc + 1];
                }
            }
            out[r * N + c] = sum;
        }
    }
}

#ifndef LAYOUT_BLOCKED
void test_tiled_vs_baseline(int N, int B) {
    printf("[TEST] Running equivalence test (Matrix: %dx%d, Block: %dx%d)...\n",
           N, N, B, B);

    float *input, *out_ref, *out_tiled;
    size_t raw_bytes = N * N * sizeof(float);
    size_t bytes = ((raw_bytes + 31) / 32) * 32;

#if defined(MODE_SIMD)
    input = (float *)aligned_alloc(32, bytes);
    out_ref = (float *)aligned_alloc(32, bytes);
    out_tiled = (float *)aligned_alloc(32, bytes);
    test_memory_alignment(input);
    test_memory_alignment(out_tiled);
#else
    input = (float *)malloc(bytes);
    out_ref = (float *)malloc(bytes);
    out_tiled = (float *)malloc(bytes);
#endif

    assert(input && out_ref && out_tiled && "Memory allocation failed");
    memset(out_ref, 0, bytes);
    memset(out_tiled, 0, bytes);

    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    scalar_reference(input, out_ref, N, GAUSSIAN_KERNEL);

    conv2d(input, out_tiled, N, GAUSSIAN_KERNEL, B);

    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            int idx = r * N + c;
            float diff = fabsf(out_tiled[idx] - out_ref[idx]);
            assert(diff < EPSILON &&
                   "Tiled output deviated from scalar baseline!");
        }
    }

    free(input);
    free(out_ref);
    free(out_tiled);
    printf("       -> PASSED (Block %d, N=%d)\n", B, N);
}
#else
static float *alloc32(size_t n_floats) {
    float *p =
        (float *)aligned_alloc(32, (n_floats * sizeof(float) + 31) / 32 * 32);
    assert(p != NULL && "Memory allocation failed");
    return p;
}

// Every block cell must equal the source cell, or 0 outside the matrix
void test_pack_halo(int N, int B) {
    printf("[TEST] Pack/halo test (Matrix: %dx%d, Block: %dx%d)...\n", N, N, B,
           B);

    const int nb = (N - 2 + B - 1) / B;
    const int S = B + 2;
    assert(blocked_in_len(N, B) == (size_t)nb * nb * S * S);
    assert(blocked_out_len(N, B) == (size_t)nb * nb * B * B);

    float *input = alloc32((size_t)N * N);
    float *bin = alloc32(blocked_in_len(N, B));
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(i + 1);
    }

    pack_input(input, bin, N, B);

    for (int bi = 0; bi < nb; bi++) {
        for (int bj = 0; bj < nb; bj++) {
            const float *blk = bin + ((size_t)bi * nb + bj) * S * S;
            for (int r = 0; r < S; r++) {
                for (int c = 0; c < S; c++) {
                    const int gr = bi * B + r;
                    const int gc = bj * B + c;
                    const float want =
                        (gr < N && gc < N) ? input[gr * N + gc] : 0.0f;
                    assert(blk[r * S + c] == want && "Wrong halo/pack cell");
                }
            }
        }
    }

    free(input);
    free(bin);
    printf("       -> PASSED (Block %d, N=%d)\n", B, N);
}

void test_blocked_vs_baseline(int N, int B) {
    printf(
        "[TEST] Blocked layout equivalence (Matrix: %dx%d, Block: %dx%d)...\n",
        N, N, B, B);

    float *input = alloc32((size_t)N * N);
    float *out_ref = alloc32((size_t)N * N);
    float *out_blk = alloc32((size_t)N * N);
    float *bin = alloc32(blocked_in_len(N, B));
    float *bout = alloc32(blocked_out_len(N, B));
#if defined(MODE_SIMD)
    test_memory_alignment(bout);
#endif
    memset(out_ref, 0, (size_t)N * N * sizeof(float));
    memset(out_blk, 0, (size_t)N * N * sizeof(float));

    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    scalar_reference(input, out_ref, N, GAUSSIAN_KERNEL);

    pack_input(input, bin, N, B);
    conv2d_blocked(bin, bout, N, GAUSSIAN_KERNEL, B);
    unpack_output(bout, out_blk, N, B);

    // Border cells must stay untouched (0) after unpack
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            int idx = r * N + c;
            float diff = fabsf(out_blk[idx] - out_ref[idx]);
            assert(diff < EPSILON &&
                   "Blocked output deviated from scalar baseline!");
        }
    }

    free(input);
    free(out_ref);
    free(out_blk);
    free(bin);
    free(bout);
    printf("       -> PASSED (Block %d, N=%d)\n", B, N);
}
#endif

int main() {
    printf("===========================================\n");
#if defined(LAYOUT_BLOCKED) && defined(STORE_NT)
    printf("  Testing: Blocked Layout + SIMD + NT      \n");
#elif defined(LAYOUT_BLOCKED) && defined(MODE_SIMD)
    printf("  Testing: Blocked Layout + SIMD           \n");
#elif defined(LAYOUT_BLOCKED)
    printf("  Testing: Blocked Layout (Seq)            \n");
#elif defined(MODE_SIMD)
    printf("  Testing: Combined (Tiling + SIMD)        \n");
#else
    printf("  Testing: Isolated Memory Tiling (Seq)    \n");
#endif
    printf("===========================================\n");

#ifdef LAYOUT_BLOCKED
    int test_sizes[] = {3, 64, 65, 130, 1024};
#else
    int test_sizes[] = {64, 65, 1024};
#endif
    int block_sizes[] = {8, 16, 32, 64};
    const int n_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

    for (int s = 0; s < n_sizes; s++) {
        int N = test_sizes[s];
        for (int b = 0; b < 4; b++) {
            int B = block_sizes[b];
#ifdef LAYOUT_BLOCKED
            test_pack_halo(N, B);
            test_blocked_vs_baseline(N, B);
#else
            test_tiled_vs_baseline(N, B);
#endif
        }
    }

    printf("===========================================\n");
    printf("  ALL BLOCKING UNIT TESTS PASSED!          \n");
    printf("===========================================\n\n");
    return 0;
}
