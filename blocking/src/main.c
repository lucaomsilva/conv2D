#include "./conv/conv2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Generic timing wrapper function
static inline double func_time(
#ifdef MODE_SIMD
    const float *__restrict__ input, float *__restrict__ output,
#else
    const float *input, float *output,
#endif
    int N, const float kernel[3][3], int B) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    conv2d(input, output, N, kernel, B);
    clock_gettime(CLOCK_MONOTONIC, &end);

    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
}

int main(int argc, char *argv[]) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int B = (argc > 2) ? atoi(argv[2]) : 32;

    printf("===========================================\n");
#ifdef MODE_SIMD
    printf("  2D Convolution Combined: Tiling + SIMD  \n");
#else
    printf("  2D Convolution Isolated: Memory Tiling  \n");
#endif
    printf("===========================================\n");
    printf("Matrix Size : %d x %d\n", N, N);
    printf("Block Size  : %d x %d\n", B, B);

    size_t raw_bytes = N * N * sizeof(float);
    size_t bytes = ((raw_bytes + 31) / 32) * 32;

#ifdef MODE_SIMD
    float *input = (float *)aligned_alloc(32, bytes);
    float *output = (float *)aligned_alloc(32, bytes);
#else
    float *input = (float *)malloc(bytes);
    float *output = (float *)calloc(N * N, sizeof(float));
#endif

    if (!input || !output) {
        fprintf(stderr, "Error: Memory allocation failed!\n");
        return 1;
    }

#ifdef MODE_SIMD
    memset(output, 0, bytes);
#endif

    // Initialize matrix with random values
    srand(42);
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    // Execute tiled convolution inside func_time
    double elapsed = func_time(input, output, N, GAUSSIAN_KERNEL, B);

    printf("Execution Time: %.6f seconds\n", elapsed);
    printf("===========================================\n");
    printf("JSON_METRICS:{\"kernel_time_s\": %.6f}\n", elapsed);

    free(input);
    free(output);
    return 0;
}
