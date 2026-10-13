#include "./conv/conv2d.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// Generic timing wrapper function
static inline double func_time(const cv::Mat &input, cv::Mat &output,
                               const cv::Mat_<float> &kernel) {

    auto start = std::chrono::high_resolution_clock::now();
    cv::filter2D(input, output, -1, kernel);
    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double>(end - start).count();
}

int main(int argc, char *argv[]) {
    int N = (argc > 1) ? std::atoi(argv[1]) : 1024;

    std::printf("===========================================\n");
    std::printf("  2D Convolution Library Execution        \n");
    std::printf("===========================================\n");
    std::printf("Matrix Size : %d x %d\n", N, N);

    cv::Mat input(N, N, CV_32F);
    cv::Mat output(N, N, CV_32F);

    // Initialize matrix with random values
    cv::theRNG().state = 42;
    cv::randu(input, cv::Scalar(0.0f), cv::Scalar(255.0f));

    // Execute convolution inside func_time
    double elapsed = func_time(input, output, GAUSSIAN_KERNEL);

    std::printf("Execution Time: %.6f seconds\n", elapsed);
    std::printf("===========================================\n");
    std::printf("JSON_METRICS:{\"kernel_time_s\": %.6f}\n", elapsed);

    return 0;
}
