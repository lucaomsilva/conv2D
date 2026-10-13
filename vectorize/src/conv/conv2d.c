#include "conv2d.h"
#include <assert.h>

void conv2d(
#ifdef MODE_ALIGNED
    const float *__restrict__ input, float *__restrict__ output,
#else
    const float *input, float *output,
#endif /* ifdef MODE_ALIGNED */
    int N, const float kernel[3][3]) {
    assert(input != NULL && "Input matrix pointer must not be NULL");
    assert(output != NULL && "Output matrix pointer must not be NULL");
    assert(N > 2 &&
           "Matrix dimension N must be greater than 2 for a 3x3 kernel");

    for (int r = 1; r < N - 1; r++) {
#if defined(MODE_ALIGNED)
#pragma omp simd aligned(input, output: 32)
#elif defined(MODE_UNALIGNED)
#pragma omp simd
#endif
        for (int c = 1; c < N - 1; c++) {
            float sum = 0.0f;
            for (int kr = -1; kr <= 1; kr++) {
                for (int kc = -1; kc <= 1; kc++) {
                    sum +=
                        input[(r + kr) * N + (c + kc)] * kernel[kr + 1][kc + 1];
                }
            }
            output[r * N + c] = sum;
        }
    }
}
