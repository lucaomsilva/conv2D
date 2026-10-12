#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "./conv/conv2d.h"

// Generic timing wrapper function
static inline double func_time(
#ifdef MODE_ALIGNED
    const float* __restrict__ input,
    float* __restrict__ output,
#else
    const float* input,
    float* output,
#endif
    int N, const float kernel[3][3])
{
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    conv2d(input, output, N, kernel);
    clock_gettime(CLOCK_MONOTONIC, &end);

    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
}

int main(int argc, char* argv[]) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;

    printf("===========================================\n");
#if defined(MODE_ALIGNED)
    printf("  2D Convolution SIMD Aligned (AVX2)       \n");
#elif defined(MODE_UNALIGNED)
    printf("  2D Convolution SIMD Unaligned (Pragma)   \n");
#else
    printf("  2D Convolution Auto-Vectorization (Auto) \n");
#endif
    printf("===========================================\n");
    printf("Matrix Size : %d x %d\n", N, N);
    printf("Access Mode : Row-Major (Contiguous)\n");

    size_t raw_bytes = N * N * sizeof(float);
    size_t bytes = ((raw_bytes + 31) / 32) * 32;

#ifdef MODE_ALIGNED
    float* input  = (float*) aligned_alloc(32, bytes);
    float* output = (float*) aligned_alloc(32, bytes);
#else
    float* input  = (float*) malloc(bytes);
    float* output = (float*) calloc(N * N, sizeof(float));
#endif /* ifdef MODE_ALIGNED */

    if (!input || !output) {
        fprintf(stderr, "Error: Memory allocation failed!\n");
        return 1;
    }

#ifdef MODE_ALIGNED
    memset(output, 0, bytes);
#endif

    // Initialize matrix with random values
    srand(42);
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    // Execute convolution inside func_time
    double elapsed = func_time(input, output, N, GAUSSIAN_KERNEL);

    printf("Execution Time: %.6f seconds\n", elapsed);
    printf("===========================================\n");
    printf("JSON_METRICS:{\"kernel_time_s\": %.6f}\n", elapsed);

    free(input);
    free(output);
    return 0;
}
