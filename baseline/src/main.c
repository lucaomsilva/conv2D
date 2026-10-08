#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "./conv/conv2d.h"
#include "./write/write.h"

// Function pointer type matching convolution signature:
// void func(const float* input, float* output, int N, const float kernel[3][3])
typedef void (*conv_func_t)(const float*, float*, int, const float[3][3]);

// Generic timing wrapper function
double func_time(conv_func_t conv_fn, const float* input, float* output, int N, const float kernel[3][3]) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    conv_fn(input, output, N, kernel);
    clock_gettime(CLOCK_MONOTONIC, &end);

    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
}

int main(int argc, char* argv[]) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int mode = (argc > 2) ? atoi(argv[2]) : 0; // 0 = Row-Major (Contiguous), 1 = Column-Major (Non-Contiguous)
    const char* output_csv_path = (argc > 3 && argv[3][0] != '\0') ? argv[3] : NULL;

    printf("===========================================\n");
    printf("  2D Convolution Baseline Execution        \n");
    printf("===========================================\n");
    printf("Matrix Size : %d x %d\n", N, N);
    printf("Access Mode : %s\n", (mode == 0) ? "Row-Major (Contiguous)" : "Column-Major (Non-Contiguous)");
    if (output_csv_path) {
        printf("CSV Output  : %s\n", output_csv_path);
    }

    float* input  = (float*) malloc(N * N * sizeof(float));
    float* output = (float*) calloc(N * N, sizeof(float));

    if (!input || !output) {
        fprintf(stderr, "Error: Memory allocation failed!\n");
        return 1;
    }

    // Initialize matrix with random values
    srand(42);
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    // Select convolution function pointer based on mode
    conv_func_t conv_fn = (mode == 0) ? conv2d_row_major : conv2d_col_major;

    // Execute convolution inside func_time
    double elapsed = func_time(conv_fn, input, output, N, GAUSSIAN_KERNEL);

    printf("Execution Time: %.6f seconds\n", elapsed);
    printf("===========================================\n");

    // Save execution time ONLY if a CSV path flag/argument was provided
    if (output_csv_path) {
        write_time_to_filepath(output_csv_path, NULL, NULL, elapsed);
    }

    free(input);
    free(output);
    return 0;
}
