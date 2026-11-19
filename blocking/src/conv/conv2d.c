#include "conv2d.h"
#include <assert.h>

#ifdef STORE_NT
#include <immintrin.h>
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#ifndef LAYOUT_BLOCKED

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

#else // LAYOUT_BLOCKED

static inline int num_blocks(int N, int B) {
    return (N - 2 + B - 1) / B;
}

size_t blocked_in_len(int N, int B) {
    const size_t nb = num_blocks(N, B);
    return nb * nb * (size_t)(B + 2) * (B + 2);
}

size_t blocked_out_len(int N, int B) {
    const size_t nb = num_blocks(N, B);
    return nb * nb * (size_t)B * B;
}

// Block (bi,bj) row r maps to global row bi*B + r (r = 0..B+1, halo included)
void pack_input(const float *in, float *bin, int N, int B) {
    assert(in != NULL && bin != NULL && N > 2 && B > 0);
    const int nb = num_blocks(N, B);
    const int S = B + 2;

    for (int bi = 0; bi < nb; bi++) {
        for (int bj = 0; bj < nb; bj++) {
            float *blk = bin + ((size_t)bi * nb + bj) * S * S;
            for (int r = 0; r < S; r++) {
                const int gr = bi * B + r;
                for (int c = 0; c < S; c++) {
                    const int gc = bj * B + c;
                    blk[r * S + c] =
                        (gr < N && gc < N) ? in[(size_t)gr * N + gc] : 0.0f;
                }
            }
        }
    }
}

void conv2d_blocked(const float *CONV_RESTRICT bin, float *CONV_RESTRICT bout,
                    int N, const float kernel[3][3], int B) {
    assert(bin != NULL && bout != NULL && N > 2 && B > 0);
#ifdef STORE_NT
    assert(B % 8 == 0 && B <= 64 && "STORE_NT needs B % 8 == 0 and B <= 64");
#endif
    const int nb = num_blocks(N, B);
    const int S = B + 2;
    const size_t nblk = (size_t)nb * nb;

    for (size_t b = 0; b < nblk; b++) {
        const float *blk = bin + b * S * S;
        float *oblk = bout + b * B * B;
        for (int r = 0; r < B; r++) {
            float *orow = oblk + (size_t)r * B;
#ifdef STORE_NT
            float tmp[64] __attribute__((aligned(32)));
            float *dst = tmp;
#else
            float *dst = orow;
#endif
#if defined(STORE_NT_PRAGMA)
#pragma omp simd aligned(dst: 32) nontemporal(dst)
#elif defined(MODE_SIMD)
#pragma omp simd aligned(dst: 32)
#endif
            for (int c = 0; c < B; c++) {
                float sum = 0.0f;
                for (int kr = 0; kr < 3; kr++) {
                    for (int kc = 0; kc < 3; kc++) {
                        sum += blk[(r + kr) * S + c + kc] * kernel[kr][kc];
                    }
                }
                dst[c] = sum;
            }
#ifdef STORE_NT
            for (int c = 0; c < B; c += 8) {
                _mm256_stream_ps(orow + c, _mm256_load_ps(tmp + c));
            }
#endif
        }
    }
#ifdef STORE_NT
    _mm_sfence(); // NT stores are weakly ordered
#endif
}

void unpack_output(const float *bout, float *out, int N, int B) {
    assert(bout != NULL && out != NULL && N > 2 && B > 0);
    const int nb = num_blocks(N, B);

    for (int bi = 0; bi < nb; bi++) {
        const int rows = MIN(B, N - 2 - bi * B);
        for (int bj = 0; bj < nb; bj++) {
            const int cols = MIN(B, N - 2 - bj * B);
            const float *oblk = bout + ((size_t)bi * nb + bj) * B * B;
            for (int r = 0; r < rows; r++) {
                const size_t gr = 1 + (size_t)bi * B + r;
                for (int c = 0; c < cols; c++) {
                    out[gr * N + 1 + bj * B + c] = oblk[r * B + c];
                }
            }
        }
    }
}

#endif // LAYOUT_BLOCKED
