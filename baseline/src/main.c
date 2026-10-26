#include "./conv/conv2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

// Minimum duration of the measured loop. RAPL updates about every 1 ms, so a
// single fast kernel call is too short to measure energy.
#define TARGET_TIME_S 1.0

#define RAPL_DIR "/sys/class/powercap/intel-rapl:0"

typedef struct {
    double time_s;
    double energy_j;
} measure_t;

// Function pointer type matching convolution signature:
// void func(const float* input, float* output, int N, const float kernel[3][3])
typedef void (*conv_func_t)(const float *, float *, int, const float[3][3]);

// Generic timing wrapper function
static inline double func_time(conv_func_t conv_fn, const float *input,
                               float *output, int N, const float kernel[3][3]) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    conv_fn(input, output, N, kernel);
    clock_gettime(CLOCK_MONOTONIC, &end);

    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
}

// Read one integer value from a sysfs file. Returns -1 if not readable.
static inline long long read_sysfs_ll(const char *path) {
    long long value = -1;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%lld", &value) != 1) {
            value = -1;
        }
        fclose(f);
    }
    return value;
}

// Send a command ("enable" / "disable") to perf stat through the control FIFO
// and wait for its ack.
static inline void perf_control(const char *cmd) {
    const char *ctl_path = getenv("PERF_CTL_FIFO");
    const char *ack_path = getenv("PERF_ACK_FIFO");
    if (!ctl_path || !ack_path) {
        return;
    }

    int ctl_fd = open(ctl_path, O_WRONLY);
    int ack_fd = open(ack_path, O_RDONLY);
    if (ctl_fd >= 0 && ack_fd >= 0) {
        char ack[8];
        dprintf(ctl_fd, "%s\n", cmd);
        if (read(ack_fd, ack, sizeof(ack)) <= 0) {
            fprintf(stderr, "Warning: no ack from perf for '%s'\n", cmd);
        }
    }
    if (ctl_fd >= 0) {
        close(ctl_fd);
    }
    if (ack_fd >= 0) {
        close(ack_fd);
    }
}

// Returns -1 if not readable (root only on most systems)
static inline long long rapl_read_uj(void) {
    return read_sysfs_ll(RAPL_DIR "/energy_uj");
}

// Energy wrapper function: runs the kernel `reps` times and measures time,
// RAPL energy and perf counters around exactly the same loop
static inline measure_t func_energy(conv_func_t conv_fn, const float *input,
                                    float *output, int N,
                                    const float kernel[3][3], int reps) {
    struct timespec start, end;
    measure_t measure = {0.0, -1.0};

    perf_control("enable");
    long long energy_start_uj = rapl_read_uj();
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < reps; i++) {
        conv_fn(input, output, N, kernel);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    long long energy_end_uj = rapl_read_uj();
    perf_control("disable");

    measure.time_s =
        (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    // RAPL counter wraps around at max_energy_range_uj
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
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int mode = (argc > 2) ? atoi(argv[2]) : 0; // 0 = row-major, 1 = col-major

    printf("===========================================\n");
    printf("  2D Convolution Baseline Execution        \n");
    printf("===========================================\n");
    printf("Matrix Size : %d x %d\n", N, N);
    printf("Access Mode : %s\n", (mode == 0) ? "Row-Major (Contiguous)"
                                             : "Column-Major (Non-Contiguous)");

    float *input = (float *)malloc(N * N * sizeof(float));
    float *output = (float *)calloc(N * N, sizeof(float));

    if (!input || !output) {
        fprintf(stderr, "Error: Memory allocation failed!\n");
        return 1;
    }

    // Initialize matrix with random values
    srand(42);
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    // Select convolution function pointer based on mode
    conv_func_t conv_fn = (mode == 0) ? conv2d_row_major : conv2d_col_major;

    // Untimed call: touches every page of both buffers and warms the caches
    conv_fn(input, output, N, GAUSSIAN_KERNEL);

    // Calibration call: choose the repetitions so the loop lasts TARGET_TIME_S
    double single = func_time(conv_fn, input, output, N, GAUSSIAN_KERNEL);
    int reps = (int)(TARGET_TIME_S / single) + 1;

    measure_t m = func_energy(conv_fn, input, output, N, GAUSSIAN_KERNEL, reps);
    double total_s = m.time_s;
    double energy_j = m.energy_j;

    printf("Repetitions   : %d\n", reps);
    printf("Execution Time: %.6f seconds (per call)\n", total_s / reps);
    printf("Total Time    : %.6f seconds\n", total_s);
    printf("Total Energy  : %.6f J\n", energy_j);
    printf("===========================================\n");
    printf("JSON_METRICS:{\"kernel_time_s\": %.9f, \"reps\": %d, "
           "\"total_time_s\": %.6f, \"energy_j\": %.6f}\n",
           total_s / reps, reps, total_s, energy_j);

    free(input);
    free(output);
    return 0;
}
