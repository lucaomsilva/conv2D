#ifndef CONV2D_H
#define CONV2D_H

#include <stddef.h>

// 3x3 Gaussian Blur Kernel
static const float GAUSSIAN_KERNEL[3][3] = {{1.0f / 16, 2.0f / 16, 1.0f / 16},
                                            {2.0f / 16, 4.0f / 16, 2.0f / 16},
                                            {1.0f / 16, 2.0f / 16, 1.0f / 16}};

// 3x3 Identity Kernel
static const float IDENTITY_KERNEL[3][3] = {
    {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

// 3x3 Box Blur Kernel
static const float BOX_KERNEL[3][3] = {{1.0f / 9, 1.0f / 9, 1.0f / 9},
                                       {1.0f / 9, 1.0f / 9, 1.0f / 9},
                                       {1.0f / 9, 1.0f / 9, 1.0f / 9}};

#ifdef MODE_SIMD
#define CONV_RESTRICT __restrict__
#else
#define CONV_RESTRICT
#endif

#ifdef LAYOUT_BLOCKED
// Blocked layout: nb x nb blocks, input blocks (B+2)^2 with halo, output B^2
size_t blocked_in_len(int N, int B);
size_t blocked_out_len(int N, int B);
void pack_input(const float *in, float *bin, int N, int B);
void conv2d_blocked(const float *CONV_RESTRICT bin, float *CONV_RESTRICT bout,
                    int N, const float kernel[3][3], int B);
void unpack_output(const float *bout, float *out, int N, int B);
#else
// 2D Convolution with Memory Tiling (Blocking)
void conv2d(const float *CONV_RESTRICT input, float *CONV_RESTRICT output,
            int N, const float kernel[3][3], int B);
#endif

#endif // CONV2D_H
