#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../src/conv/conv2d.h"

#define EPSILON 1e-4f

// Only check 32-byte alignment when testing MODE_ALIGNED
#if defined(MODE_ALIGNED)
void test_memory_alignment(const float* ptr) {
    assert(ptr != NULL && "Pointer must not be NULL");
    assert(((uintptr_t)ptr % 32) == 0 && "Pointer must be strictly 32-byte aligned!");
}
#endif

// Scalar baseline reference (Ground Truth)
static void scalar_reference(const float* in, float* out, int N, const float k[3][3]) {
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

void test_simd(int N) {
    printf("[TEST] Running equivalence test for N=%d...\n", N);

    float *input, *out_ref, *out_simd;
    size_t raw_bytes = N * N * sizeof(float);

#if defined(MODE_ALIGNED)
    // 32-byte aligned allocation
    size_t bytes = ((raw_bytes + 31) / 32) * 32;
    input    = (float*) aligned_alloc(32, bytes);
    out_ref  = (float*) aligned_alloc(32, bytes);
    out_simd = (float*) aligned_alloc(32, bytes);

    test_memory_alignment(input);
    test_memory_alignment(out_simd);
#else
    // Standard unaligned malloc
    size_t bytes = raw_bytes;
    input    = (float*) malloc(bytes);
    out_ref  = (float*) malloc(bytes);
    out_simd = (float*) malloc(bytes);
#endif

    assert(input && out_ref && out_simd && "Allocation failed");
    memset(out_ref, 0, bytes);
    memset(out_simd, 0, bytes);

    // Initialize with random numbers
    for (int i = 0; i < N * N; i++) input[i] = (float)(rand() % 256);

    // 1. Run Ground Truth (Scalar)
    scalar_reference(input, out_ref, N, GAUSSIAN_KERNEL);

    // 2. Run the compiled conv2d (variant determined by -D flag!)
    conv2d(input, out_simd, N, GAUSSIAN_KERNEL);

    // 3. Assert mathematical equality
    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            int idx = r * N + c;
            float diff = fabsf(out_simd[idx] - out_ref[idx]);
            assert(diff < EPSILON && "SIMD output deviated from scalar baseline!");
        }
    }

    free(input);
    free(out_ref);
    free(out_simd);
    printf("       -> PASSED: Output matches scalar baseline identically.\n");
}

int main() {
    printf("===========================================\n");
#if defined(MODE_ALIGNED)
    printf("  Testing: SIMD Aligned (32-byte boundary) \n");
#elif defined(MODE_UNALIGNED)
    printf("  Testing: SIMD Unaligned (Pragma Only)    \n");
#else
    printf("  Testing: Auto-Vectorization              \n");
#endif
    printf("===========================================\n");

    test_simd(64);   // Small power of 2
    test_simd(65);   // Odd size (tests remainder/tail loops)
    test_simd(1024); // Large benchmark size

    printf("===========================================\n");
    printf("  ALL TESTS PASSED SUCCESSFULLY!           \n");
    printf("===========================================\n\n");
    return 0;
}
