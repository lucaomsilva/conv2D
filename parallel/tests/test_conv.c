#include "../src/conv/conv2d.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define EPSILON 1e-4f

static const int THREADS[] = {1, 2, 7, 16};
static const omp_sched_t SCHEDULES[] = {omp_sched_static, omp_sched_dynamic,
                                        omp_sched_guided};
static const char *SCHEDULE_NAMES[] = {"static", "dynamic", "guided"};

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

static void test_parallel(int N, int threads, int s) {
    printf("[TEST] N=%d threads=%d schedule=%s...\n", N, threads,
           SCHEDULE_NAMES[s]);

    size_t bytes = (((size_t)N * N * sizeof(float) + 31) / 32) * 32;
    float *input = (float *)aligned_alloc(32, bytes);
    float *out_ref = (float *)aligned_alloc(32, bytes);
    float *out_omp = (float *)aligned_alloc(32, bytes);
    assert(input && out_ref && out_omp && "Allocation failed");
    assert(((uintptr_t)input % 32) == 0 && ((uintptr_t)out_omp % 32) == 0 &&
           "Pointer must be strictly 32-byte aligned!");

    memset(out_ref, 0, bytes);
    // Sentinel: the border must stay untouched
    for (size_t i = 0; i < bytes / sizeof(float); i++) {
        out_omp[i] = -1.0f;
    }
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    scalar_reference(input, out_ref, N, GAUSSIAN_KERNEL);

    omp_set_num_threads(threads);
    omp_set_schedule(SCHEDULES[s], 0);
    conv2d(input, out_omp, N, GAUSSIAN_KERNEL);

    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            int idx = r * N + c;
            if (r == 0 || c == 0 || r == N - 1 || c == N - 1) {
                assert(out_omp[idx] == -1.0f && "Border was written!");
            } else {
                assert(fabsf(out_omp[idx] - out_ref[idx]) < EPSILON &&
                       "OpenMP output deviated from scalar baseline!");
            }
        }
    }

    free(input);
    free(out_ref);
    free(out_omp);
    printf("       -> PASSED\n");
}

int main() {
    printf("===========================================\n");
    printf("  Testing: OpenMP + SIMD Aligned           \n");
    printf("===========================================\n");

    const int sizes[] = {3, 64, 65, 1024};
    for (size_t n = 0; n < sizeof(sizes) / sizeof(sizes[0]); n++) {
        for (size_t t = 0; t < sizeof(THREADS) / sizeof(THREADS[0]); t++) {
            for (int s = 0; s < 3; s++) {
                test_parallel(sizes[n], THREADS[t], s);
            }
        }
    }

    printf("===========================================\n");
    printf("  ALL TESTS PASSED SUCCESSFULLY!           \n");
    printf("===========================================\n\n");
    return 0;
}
