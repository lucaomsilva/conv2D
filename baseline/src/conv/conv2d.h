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

// Row-Major Baseline Convolution (Contiguous Memory Access)
void conv2d_row_major(const float* input, float* output, int N, const float kernel[3][3]);

// Column-Major Baseline Convolution (Non-Contiguous Memory Access)
void conv2d_col_major(const float* input, float* output, int N, const float kernel[3][3]);

#endif // CONV2D_H
