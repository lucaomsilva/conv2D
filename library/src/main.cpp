#include "./conv/conv2d.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <unistd.h>

// RAPL updates about every 1 ms: one fast call is too short to measure energy
#define TARGET_TIME_S 1.0

#define RAPL_DIR "/sys/class/powercap/intel-rapl:0"

typedef struct {
    double time_s;
    double energy_j; // -1 if unknown
} measure_t;

// Generic timing wrapper function
static inline double func_time(const cv::Mat &input, cv::Mat &output,
                               const cv::Mat_<float> &kernel) {

    auto start = std::chrono::high_resolution_clock::now();
    cv::filter2D(input, output, -1, kernel);
    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double>(end - start).count();
}

static inline long long read_sysfs_ll(const char *path) {
    long long value = -1;
    FILE *f = std::fopen(path, "r");
    if (f) {
        if (std::fscanf(f, "%lld", &value) != 1) {
            value = -1;
        }
        std::fclose(f);
    }
    return value;
}

// Enable/disable perf stat counters (perf stat -D -1 --control fifo:ctl,ack)
static inline void perf_control(const char *cmd) {
    const char *ctl_path = std::getenv("PERF_CTL_FIFO");
    const char *ack_path = std::getenv("PERF_ACK_FIFO");
    if (!ctl_path || !ack_path) {
        return;
    }

    int ctl_fd = open(ctl_path, O_WRONLY);
    int ack_fd = open(ack_path, O_RDONLY);
    if (ctl_fd >= 0 && ack_fd >= 0) {
        char ack[8];
        dprintf(ctl_fd, "%s\n", cmd);
        if (read(ack_fd, ack, sizeof(ack)) <= 0) {
            std::fprintf(stderr, "Warning: no ack from perf for '%s'\n", cmd);
        }
    }
    if (ctl_fd >= 0) {
        close(ctl_fd);
    }
    if (ack_fd >= 0) {
        close(ack_fd);
    }
}

static inline long long rapl_read_uj(void) {
    return read_sysfs_ll(RAPL_DIR "/energy_uj");
}

// Energy wrapper function: time, energy and perf counters of the same loop
static inline measure_t func_energy(const cv::Mat &input, cv::Mat &output,
                                    const cv::Mat &kernel, int reps) {
    measure_t measure = {0.0, -1.0};

    perf_control("enable");
    long long energy_start_uj = rapl_read_uj();
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < reps; i++) {
        cv::filter2D(input, output, -1, kernel);
    }

    auto end = std::chrono::high_resolution_clock::now();
    long long energy_end_uj = rapl_read_uj();
    perf_control("disable");

    measure.time_s = std::chrono::duration<double>(end - start).count();

    if (energy_start_uj >= 0 && energy_end_uj >= 0) {
        long long delta_uj = energy_end_uj - energy_start_uj;
        if (delta_uj < 0) {
            delta_uj += read_sysfs_ll(RAPL_DIR "/max_energy_range_uj");
        }
        measure.energy_j = delta_uj / 1e6;
    }

    return measure;
}

int main(int argc, char *argv[]) {
    int N = (argc > 1) ? std::atoi(argv[1]) : 1024;

    // Same condition as the custom kernels: one thread on one core
    cv::setNumThreads(1);

    std::printf("===========================================\n");
    std::printf("  2D Convolution Library Execution        \n");
    std::printf("===========================================\n");
    std::printf("Matrix Size : %d x %d\n", N, N);

    cv::Mat input(N, N, CV_32F);
    cv::Mat output(N, N, CV_32F);

    cv::theRNG().state = 42;
    cv::randu(input, cv::Scalar(0.0f), cv::Scalar(255.0f));

    // Untimed call: page faults and caches
    cv::filter2D(input, output, -1, GAUSSIAN_KERNEL);

    double single = func_time(input, output, GAUSSIAN_KERNEL);
    int reps = (int)(TARGET_TIME_S / single) + 1;

    measure_t m = func_energy(input, output, GAUSSIAN_KERNEL, reps);

    std::printf("Repetitions   : %d\n", reps);
    std::printf("Execution Time: %.6f seconds (per call)\n", m.time_s / reps);
    std::printf("Total Time    : %.6f seconds\n", m.time_s);
    std::printf("Total Energy  : %.6f J\n", m.energy_j);
    std::printf("===========================================\n");
    std::printf("JSON_METRICS:{\"kernel_time_s\": %.9f, \"reps\": %d, "
                "\"total_time_s\": %.6f, \"energy_j\": %.6f}\n",
                m.time_s / reps, reps, m.time_s, m.energy_j);

    return 0;
}
