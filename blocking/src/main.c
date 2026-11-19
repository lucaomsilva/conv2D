#include "./conv/conv2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HUGEPAGES
#include <sys/mman.h>
#endif

// Minimum duration of the measured loop. RAPL updates about every 1 ms, so a
// single fast kernel call is too short to measure energy.
#define TARGET_TIME_S 1.0

#define RAPL_DIR "/sys/class/powercap/intel-rapl:0"

#ifdef LAYOUT_BLOCKED
#define KERNEL               conv2d_blocked
// Pack/unpack are timed only (no RAPL), so a shorter loop is enough
#define LAYOUT_TARGET_TIME_S 0.25
#else
#define KERNEL conv2d
#endif

typedef struct {
    double time_s;
    double energy_j;
} measure_t;

// Generic timing wrapper function
static inline double func_time(const float *CONV_RESTRICT input,
                               float *CONV_RESTRICT output, int N,
                               const float kernel[3][3], int B) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    KERNEL(input, output, N, kernel, B);
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
                                    const float kernel[3][3], int B, int reps) {
    struct timespec start, end;
    measure_t measure = {0.0, -1.0};

    perf_control("enable");
    long long energy_start_uj = rapl_read_uj();
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < reps; i++) {
        KERNEL(input, output, N, kernel, B);
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

#ifdef LAYOUT_BLOCKED
static size_t g_buf_bytes = 0;

// 32-byte aligned buffer; with HUGEPAGES, 2 MB aligned and backed by THP
static void *alloc_buf(size_t bytes) {
#ifdef HUGEPAGES
    const size_t hp = (size_t)2 << 20;
    bytes = (bytes + hp - 1) / hp * hp;
    void *p = aligned_alloc(hp, bytes);
    if (p && madvise(p, bytes, MADV_HUGEPAGE) != 0) {
        perror("Warning: madvise(MADV_HUGEPAGE)");
    }
#else
    bytes = (bytes + 31) / 32 * 32;
    void *p = aligned_alloc(32, bytes);
#endif
    if (p) {
        memset(p, 0, bytes);
        g_buf_bytes += bytes;
    }
    return p;
}

// THP coverage: AnonHugePages of this process in kB, -1 if not readable
static long long read_anon_huge_kb(void) {
    long long kb = -1;
    char line[256];
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "AnonHugePages: %lld kB", &kb) == 1) {
            break;
        }
    }
    fclose(f);
    return kb;
}

// Mean time per call of pack_input or unpack_output, calibrated loop
static double time_layout_fn(void (*fn)(const float *, float *, int, int),
                             const float *src, float *dst, int N, int B) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    fn(src, dst, N, B);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double single =
        (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    int reps = (int)(LAYOUT_TARGET_TIME_S / single) + 1;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < reps; i++) {
        fn(src, dst, N, B);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return ((end.tv_sec - start.tv_sec) +
            (end.tv_nsec - start.tv_nsec) * 1e-9) /
           reps;
}
#endif

int main(int argc, char *argv[]) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int B = (argc > 2) ? atoi(argv[2]) : 32;

    printf("===========================================\n");
#if defined(LAYOUT_BLOCKED)
    printf("  2D Convolution: Blocked Layout%s%s%s\n",
#ifdef MODE_SIMD
           " + SIMD",
#else
           "",
#endif
#ifdef STORE_NT
           " + NT",
#else
           "",
#endif
#ifdef HUGEPAGES
           " + HP"
#else
           ""
#endif
    );
#elif defined(MODE_SIMD)
    printf("  2D Convolution Combined: Tiling + SIMD  \n");
#else
    printf("  2D Convolution Isolated: Memory Tiling  \n");
#endif
    printf("===========================================\n");
    printf("Matrix Size : %d x %d\n", N, N);
    printf("Block Size  : %d x %d\n", B, B);

    size_t raw_bytes = (size_t)N * N * sizeof(float);
    size_t bytes = ((raw_bytes + 31) / 32) * 32;

#if defined(LAYOUT_BLOCKED)
    float *input = (float *)alloc_buf(bytes);
    float *output = (float *)alloc_buf(bytes);
    float *bin = (float *)alloc_buf(blocked_in_len(N, B) * sizeof(float));
    float *bout = (float *)alloc_buf(blocked_out_len(N, B) * sizeof(float));
    if (!bin || !bout) {
        fprintf(stderr, "Error: Memory allocation failed!\n");
        return 1;
    }
#elif defined(MODE_SIMD)
    float *input = (float *)aligned_alloc(32, bytes);
    float *output = (float *)aligned_alloc(32, bytes);
#else
    float *input = (float *)malloc(bytes);
    float *output = (float *)calloc(N * N, sizeof(float));
#endif

    if (!input || !output) {
        fprintf(stderr, "Error: Memory allocation failed!\n");
        return 1;
    }

#if defined(MODE_SIMD) && !defined(LAYOUT_BLOCKED)
    memset(output, 0, bytes);
#endif

    // Initialize matrix with random values
    srand(42);
    for (int i = 0; i < N * N; i++) {
        input[i] = (float)(rand() % 256);
    }

#ifdef LAYOUT_BLOCKED
    // Input does not change between calls: pack once, outside the timed loop
    pack_input(input, bin, N, B);
    const float *k_in = bin;
    float *k_out = bout;
#else
    const float *k_in = input;
    float *k_out = output;
#endif

    // Untimed call: touches every page of both buffers and warms the caches
    KERNEL(k_in, k_out, N, GAUSSIAN_KERNEL, B);

    // Calibration call: choose the repetitions so the loop lasts TARGET_TIME_S
    double single = func_time(k_in, k_out, N, GAUSSIAN_KERNEL, B);
    int reps = (int)(TARGET_TIME_S / single) + 1;

    measure_t m = func_energy(k_in, k_out, N, GAUSSIAN_KERNEL, B, reps);
    double total_s = m.time_s;
    double energy_j = m.energy_j;

    printf("Repetitions   : %d\n", reps);
    printf("Execution Time: %.6f seconds (per call)\n", total_s / reps);
    printf("Total Time    : %.6f seconds\n", total_s);
    printf("Total Energy  : %.6f J\n", energy_j);
#ifdef LAYOUT_BLOCKED
    long long thp_kb = read_anon_huge_kb();
    long long buf_kb = (long long)(g_buf_bytes / 1024);
    unpack_output(bout, output, N, B);
    double pack_s = time_layout_fn(pack_input, input, bin, N, B);
    double unpack_s = time_layout_fn(unpack_output, bout, output, N, B);
    printf("Pack Time     : %.6f seconds (per call)\n", pack_s);
    printf("Unpack Time   : %.6f seconds (per call)\n", unpack_s);
    printf("Huge Pages    : %lld of %lld kB\n", thp_kb, buf_kb);
    printf("===========================================\n");
    printf("JSON_METRICS:{\"kernel_time_s\": %.9f, \"reps\": %d, "
           "\"total_time_s\": %.6f, \"energy_j\": %.6f, "
           "\"pack_time_s\": %.9f, \"unpack_time_s\": %.9f, "
           "\"thp_anon_kb\": %lld, \"buffers_kb\": %lld}\n",
           total_s / reps, reps, total_s, energy_j, pack_s, unpack_s, thp_kb,
           buf_kb);
    free(bin);
    free(bout);
#else
    printf("===========================================\n");
    printf("JSON_METRICS:{\"kernel_time_s\": %.9f, \"reps\": %d, "
           "\"total_time_s\": %.6f, \"energy_j\": %.6f}\n",
           total_s / reps, reps, total_s, energy_j);
#endif

    free(input);
    free(output);
    return 0;
}
