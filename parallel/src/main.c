#include "./conv/conv2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <omp.h>
#include <unistd.h>

// Minimum duration of the measured loop. RAPL updates about every 1 ms, so a
// single fast kernel call is too short to measure energy.
#define TARGET_TIME_S 1.0

#define RAPL_DIR "/sys/class/powercap/intel-rapl:0"

typedef struct {
    double time_s;
    double energy_j;
} measure_t;

// Generic timing wrapper function
static inline double func_time(const float *__restrict__ input,
                               float *__restrict__ output, int N,
                               const float kernel[3][3]) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    conv2d(input, output, N, kernel);
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
static inline measure_t func_energy(const float *input, float *output, int N,
                                    const float kernel[3][3], int reps) {
    struct timespec start, end;
    measure_t measure = {0.0, -1.0};

    perf_control("enable");
    long long energy_start_uj = rapl_read_uj();
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < reps; i++) {
        conv2d(input, output, N, kernel);
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

static const char *schedule_name(omp_sched_t kind) {
    switch (kind & ~omp_sched_monotonic) {
    case omp_sched_static:
        return "static";
    case omp_sched_dynamic:
        return "dynamic";
    case omp_sched_guided:
        return "guided";
    default:
        return "auto";
    }
}

int main(int argc, char *argv[]) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int T = (argc > 2) ? atoi(argv[2]) : omp_get_max_threads();
    if (N <= 2 || T < 1) {
        fprintf(stderr, "Usage: %s <N> <threads>\n", argv[0]);
        return 1;
    }
    omp_set_num_threads(T);

    omp_sched_t sched_kind;
    int sched_chunk;
    omp_get_schedule(&sched_kind, &sched_chunk);

    printf("===========================================\n");
    printf("  2D Convolution OpenMP + SIMD Aligned     \n");
    printf("===========================================\n");
    printf("Matrix Size : %d x %d\n", N, N);
    printf("Threads     : %d\n", T);
    printf("Schedule    : %s,%d\n", schedule_name(sched_kind), sched_chunk);

    size_t raw_bytes = (size_t)N * N * sizeof(float);
    size_t bytes = ((raw_bytes + 31) / 32) * 32;

    float *input = (float *)aligned_alloc(32, bytes);
    float *output = (float *)aligned_alloc(32, bytes);
    if (!input || !output) {
        fprintf(stderr, "Error: Memory allocation failed!\n");
        return 1;
    }
    memset(output, 0, bytes);

    // Same input as AP1: random values 0-255, fixed seed
    srand(42);
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

    // Untimed call: page faults, caches and OpenMP thread pool creation
    conv2d(input, output, N, GAUSSIAN_KERNEL);

    // Calibration call: choose the repetitions so the loop lasts TARGET_TIME_S
    double single = func_time(input, output, N, GAUSSIAN_KERNEL);
    int reps = (int)(TARGET_TIME_S / single) + 1;

    measure_t m = func_energy(input, output, N, GAUSSIAN_KERNEL, reps);
    double total_s = m.time_s;
    double energy_j = m.energy_j;

    printf("Repetitions   : %d\n", reps);
    printf("Execution Time: %.6f seconds (per call)\n", total_s / reps);
    printf("Total Time    : %.6f seconds\n", total_s);
    printf("Total Energy  : %.6f J\n", energy_j);
    printf("===========================================\n");
    printf("JSON_METRICS:{\"kernel_time_s\": %.9f, \"reps\": %d, "
           "\"total_time_s\": %.6f, \"energy_j\": %.6f, \"threads\": %d, "
           "\"schedule\": \"%s\", \"chunk\": %d}\n",
           total_s / reps, reps, total_s, energy_j, T,
           schedule_name(sched_kind), sched_chunk);

    free(input);
    free(output);
    return 0;
}
