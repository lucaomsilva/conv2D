#ifndef CONV2D_H
#define CONV2D_H

#include <stddef.h>

// 3x3 Gaussian Blur Kernel
static const float GAUSSIAN_KERNEL[3][3] = {
    {1.0f/16, 2.0f/16, 1.0f/16},
    {2.0f/16, 4.0f/16, 2.0f/16},
    {1.0f/16, 2.0f/16, 1.0f/16}
};

// 3x3 Identity Kernel
static const float IDENTITY_KERNEL[3][3] = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f}
};

// 3x3 Box Blur Kernel
static const float BOX_KERNEL[3][3] = {
    {1.0f/9, 1.0f/9, 1.0f/9},
    {1.0f/9, 1.0f/9, 1.0f/9},
    {1.0f/9, 1.0f/9, 1.0f/9}
};

// 2D Convolution with Memory Tiling (Blocking)
#ifdef MODE_SIMD
void conv2d(const float* __restrict__ input, float* __restrict__ output, int N, const float kernel[3][3], int B);
#else
void conv2d(const float* input, float* output, int N, const float kernel[3][3], int B);
#endif

#endif // CONV2D_H
