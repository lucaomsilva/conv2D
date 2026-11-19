#include "conv2d.h"
#include <assert.h>

void conv2d(const float *__restrict__ input, float *__restrict__ output, int N,
            const float kernel[3][3]) {
    assert(input != NULL && "Input matrix pointer must not be NULL");
    assert(output != NULL && "Output matrix pointer must not be NULL");
    assert(N > 2 &&
           "Matrix dimension N must be greater than 2 for a 3x3 kernel");

    // Local copy: in the outlined region GCC drops restrict and reloads kernel
    float k[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            k[i][j] = kernel[i][j];
        }
    }

    // Schedule comes from OMP_SCHEDULE / omp_set_schedule()
#pragma omp parallel for schedule(runtime) firstprivate(k)
    for (int r = 1; r < N - 1; r++) {
#pragma omp simd aligned(input, output: 32)
        for (int c = 1; c < N - 1; c++) {
            float sum = 0.0f;
            for (int kr = -1; kr <= 1; kr++) {
                for (int kc = -1; kc <= 1; kc++) {
                    sum += input[(r + kr) * N + (c + kc)] * k[kr + 1][kc + 1];
                }
            }
            output[r * N + c] = sum;
        }
    }
}
