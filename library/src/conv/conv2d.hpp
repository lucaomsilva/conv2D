#ifndef CONV2D_HPP
#define CONV2D_HPP

#include <opencv2/core.hpp>

// 3x3 Gaussian Blur Kernel
inline static const cv::Mat GAUSSIAN_KERNEL =
    (cv::Mat_<float>(3, 3) << 1.0f / 16, 2.0f / 16, 1.0f / 16, 2.0f / 16,
     4.0f / 16, 2.0f / 16, 1.0f / 16, 2.0f / 16, 1.0f / 16);

// 3x3 Identity Kernel
inline static const cv::Mat IDENTITY_KERNEL =
    (cv::Mat_<float>(3, 3) << 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
     0.0f);

// 3x3 Box Blur Kernel
inline static const cv::Mat BOX_KERNEL =
    (cv::Mat_<float>(3, 3) << 1.0f / 9, 1.0f / 9, 1.0f / 9, 1.0f / 9, 1.0f / 9,
     1.0f / 9, 1.0f / 9, 1.0f / 9, 1.0f / 9);

#endif // CONV2D_HPP
