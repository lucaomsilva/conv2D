#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "../src/conv/conv2d.h"
#include "../src/write/write.h"

// Tolerance threshold for floating-point comparisons
#define EPSILON 1e-5f

// Test 1: Identity Kernel Validation
// An identity kernel must preserve the exact values of inner pixels.
void test_identity_kernel() {
    printf("[TEST] Running test_identity_kernel...\n");
    int N = 5;
    float input[25] = {
         1.0f,  2.0f,  3.0f,  4.0f,  5.0f,
         6.0f,  7.0f,  8.0f,  9.0f, 10.0f,
        11.0f, 12.0f, 13.0f, 14.0f, 15.0f,
        16.0f, 17.0f, 18.0f, 19.0f, 20.0f,
        21.0f, 22.0f, 23.0f, 24.0f, 25.0f
    };
    float output[25] = {0};

    conv2d_row_major(input, output, N, IDENTITY_KERNEL);

    // Verify inner elements (rows 1..3, cols 1..3)
    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            int idx = r * N + c;
            float diff = fabsf(output[idx] - input[idx]);
            assert(diff < EPSILON);
        }
    }
    printf("       -> PASSED: Identity kernel output matches input.\n");
}

// Test 2: Box Blur Constant Matrix Validation
// A constant matrix filled with 9.0f blurred by a 1/9 box kernel must equal 9.0f.
void test_box_blur_kernel() {
    printf("[TEST] Running test_box_blur_kernel...\n");
    int N = 5;
    float input[25];
    float output[25] = {0};

    for (int i = 0; i < 25; i++) {
        input[i] = 9.0f;
    }

    conv2d_row_major(input, output, N, BOX_KERNEL);

    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            int idx = r * N + c;
            float diff = fabsf(output[idx] - 9.0f);
            assert(diff < EPSILON);
        }
    }
    printf("       -> PASSED: Box blur output matches constant expectation.\n");
}

// Test 3: Row-Major vs Column-Major Equivalence Test
// Both traversal orders must yield mathematically identical outputs.
void test_row_vs_col_major_equivalence() {
    printf("[TEST] Running test_row_vs_col_major_equivalence...\n");
    int N = 64;
    float* input   = (float*) malloc(N * N * sizeof(float));
    float* out_row = (float*) calloc(N * N, sizeof(float));
    float* out_col = (float*) calloc(N * N, sizeof(float));

    assert(input != NULL);
    assert(out_row != NULL);
    assert(out_col != NULL);

    // Fill with pseudo-random floats
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256) / 1.0f;
    }

    conv2d_row_major(input, out_row, N, GAUSSIAN_KERNEL);
    conv2d_col_major(input, out_col, N, GAUSSIAN_KERNEL);

    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            int idx = r * N + c;
            float diff = fabsf(out_row[idx] - out_col[idx]);
            assert(diff < EPSILON);
        }
    }

    free(input);
    free(out_row);
    free(out_col);
    printf("       -> PASSED: Row-Major and Column-Major outputs are identical.\n");
}

// Test 4: CSV Time Logging Test
void test_write_time_csv() {
    printf("[TEST] Running test_write_time_csv...\n");
    int status = write_time_csv("2026-10-08 10:00:00", 0.123456);
    assert(status == 0);
    printf("       -> PASSED: Execution time CSV write returned success.\n");
}

int main() {
    printf("=======================================\n");
    printf("  Running 2D Convolution Unit Tests    \n");
    printf("=======================================\n");

    test_identity_kernel();
    test_box_blur_kernel();
    test_row_vs_col_major_equivalence();
    test_write_time_csv();

    printf("=======================================\n");
    printf("  ALL UNIT TESTS PASSED SUCCESSFULLY!  \n");
    printf("=======================================\n");
    return 0;
}
