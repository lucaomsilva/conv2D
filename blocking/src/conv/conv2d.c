#include "conv2d.h"
#include <assert.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

void conv2d(
#ifdef MODE_SIMD
    const float *__restrict__ input, float *__restrict__ output,
#else
    const float *input, float *output,
#endif
    int N, const float kernel[3][3], int B) {
    assert(input != NULL && "Input matrix pointer must not be NULL");
    assert(output != NULL && "Output matrix pointer must not be NULL");
    assert(N > 2 && "Matrix dimension N must be greater than 2");
    assert(B > 0 && "Block size B must be greater than 0");

    for (int ii = 1; ii < N - 1; ii += B) {
        const int r_end = MIN(ii + B, N - 1);
        for (int jj = 1; jj < N - 1; jj += B) {
            const int c_end = MIN(jj + B, N - 1);
            for (int r = ii; r < r_end; r++) {
#ifdef MODE_SIMD
#pragma omp simd aligned(input, output: 32)
#endif
                for (int c = jj; c < c_end; c++) {
                    float sum = 0.0f;
                    for (int kr = -1; kr <= 1; kr++) {
                        for (int kc = -1; kc <= 1; kc++) {
                            sum += input[(r + kr) * N + (c + kc)] *
                                   kernel[kr + 1][kc + 1];
                        }
                    }
                    output[r * N + c] = sum;
                }
            }
        }
    }
}
