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

int main() {
    printf("===========================================\n");
#if defined(MODE_SIMD)
    printf("  Testing: Combined (Tiling + SIMD)        \n");
#else
    printf("  Testing: Isolated Memory Tiling (Seq)    \n");
#endif
    printf("===========================================\n");

    int test_sizes[] = {64, 65, 1024};
    int block_sizes[] = {8, 16, 32, 64};

    for (int s = 0; s < 3; s++) {
        int N = test_sizes[s];
        for (int b = 0; b < 4; b++) {
            int B = block_sizes[b];
            test_tiled_vs_baseline(N, B);
        }
    }

    printf("===========================================\n");
    printf("  ALL BLOCKING UNIT TESTS PASSED!          \n");
    printf("===========================================\n\n");
    return 0;
}
