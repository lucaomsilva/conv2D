#include <assert.h>
#include "conv2d.h"

void conv2d_row_major(const float* input, float* output, int N, const float kernel[3][3]) {
    // Ensure input and output pointers are non-NULL and dimension N is valid
    assert(input != NULL && "Input matrix pointer must not be NULL");
    assert(output != NULL && "Output matrix pointer must not be NULL");
    assert(N > 2 && "Matrix dimension N must be greater than 2 for a 3x3 kernel");

    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            float sum = 0.0f;
            for (int kr = -1; kr <= 1; kr++) {
                for (int kc = -1; kc <= 1; kc++) {
                    sum += input[(r + kr) * N + (c + kc)] * kernel[kr + 1][kc + 1];
                }
            }
            output[r * N + c] = sum;
        }
    }
}

void conv2d_col_major(const float* input, float* output, int N, const float kernel[3][3]) {
    // Ensure input and output pointers are non-NULL and dimension N is valid
    assert(input != NULL && "Input matrix pointer must not be NULL");
    assert(output != NULL && "Output matrix pointer must not be NULL");
    assert(N > 2 && "Matrix dimension N must be greater than 2 for a 3x3 kernel");

    for (int c = 1; c < N - 1; c++) {
        for (int r = 1; r < N - 1; r++) {
            float sum = 0.0f;
            for (int kr = -1; kr <= 1; kr++) {
                for (int kc = -1; kc <= 1; kc++) {
                    sum += input[(r + kr) * N + (c + kc)] * kernel[kr + 1][kc + 1];
                }
            }
            output[r * N + c] = sum;
        }
    }
}
