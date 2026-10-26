#include "../src/conv/conv2d.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

constexpr float EPSILON = 1e-4f;

// Scalar baseline reference (Ground Truth)
static void scalar_reference(const cv::Mat &in, cv::Mat &out,
                             const cv::Mat &kernel) {
    int N = in.rows;
    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            float sum = 0.0f;
            for (int kr = -1; kr <= 1; kr++) {
                for (int kc = -1; kc <= 1; kc++) {
                    sum += in.at<float>(r + kr, c + kc) *
                           kernel.at<float>(kr + 1, kc + 1);
                }
            }
            out.at<float>(r, c) = sum;
        }
    }
}

void test_opencv(int N, const cv::Mat &kernel, const char *kernel_name) {
    std::cout << "[TEST] Running equivalence test (Matrix: " << N << "x" << N
              << ", Kernel: " << kernel_name << ")...\n";

    cv::Mat input(N, N, CV_32F);
    cv::Mat out_ref = cv::Mat::zeros(N, N, CV_32F);
    cv::Mat out_cv = cv::Mat::zeros(N, N, CV_32F);

    // Populate input matrix with random values [0, 255]
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            input.at<float>(r, c) = static_cast<float>(std::rand() % 256);
        }
    }

    scalar_reference(input, out_ref, kernel);

    cv::filter2D(input, out_cv, -1, kernel);

    for (int r = 1; r < N - 1; r++) {
        for (int c = 1; c < N - 1; c++) {
            float diff =
                std::fabs(out_cv.at<float>(r, c) - out_ref.at<float>(r, c));
            assert(diff < EPSILON &&
                   "OpenCV filter2D output deviated from scalar baseline!");
        }
    }

    std::cout << "       -> PASSED (Kernel: " << kernel_name << ", N=" << N
              << ")\n";
}

int main() {
    std::cout << "===========================================\n";
    std::cout << "  Testing: OpenCV 2D Convolution (C++)     \n";
    std::cout << "===========================================\n";

    int test_sizes[] = {64, 65, 1024};

    cv::setNumThreads(1);

    for (int N : test_sizes) {
        test_opencv(N, GAUSSIAN_KERNEL, "GAUSSIAN_KERNEL");
        test_opencv(N, IDENTITY_KERNEL, "IDENTITY_KERNEL");
        test_opencv(N, BOX_KERNEL, "BOX_KERNEL");
    }

    std::cout << "===========================================\n";
    std::cout << "  ALL UNIT TESTS PASSED!                   \n";
    std::cout << "===========================================\n\n";
    return 0;
}
