# 2D Convolution (Stencil) Performance and Energy Efficiency Optimization

This repository contains a 3x3 2D convolution (Gaussian blur stencil) used to evaluate and optimize computational performance and energy efficiency. It has two coursework activities (PPAD - PPGCC):

- **AP1:** isolated and combined impacts of **memory affinity (cache optimization)**, **SIMD vectorization** and **memory tiling (blocking)**, plus a comparison with **OpenCV**.
- **AP2:** **multi-thread parallelization with OpenMP** of the best AP1 version, with performance and energy measured as the thread count increases.

Target CPU: Intel Core i5-12500H (hybrid: 4 P-cores with HT = CPUs 0-7, 8 E-cores = CPUs 8-15; AVX2 + FMA, no AVX-512).

## Project Structure

Each exercise is a self-contained module with its own `Makefile`, `src/`, `tests/`, `scripts/` and `bin/`:

| Module | Activity | Exercise | Language | Binaries |
|---|---|---|---|---|
| `baseline/` | AP1 | 1. Row-major vs column-major, vectorization off | C11 | `baseline_o3`, `baseline_no_o3` |
| `vectorize/` | AP1 | 2. SIMD with pragmas and alignment | C11 | `simd_autovec`, `simd_unaligned`, `simd_aligned` |
| `blocking/` | AP1 | 3. Loop tiling, alone and combined with SIMD | C11 | `blocking_seq`, `blocking_simd` |
| `library/` | AP1 | 4. OpenCV `cv::filter2D` | C++17 | `library_o3` |
| `parallel/` | AP2 | OpenMP threads + SIMD (best AP1 version) | C11 | `parallel_omp` |

Other folders:

| Path | Content |
|---|---|
| `scripts/compile_data.py` | Merges all benchmark CSVs into `docs/data/compiled/` |
| `scripts/analysis/` | Tables and graphs (`compare.py`, `plot_results.py` for AP1; `ap2_compare.py`, `ap2_plots.py` for AP2) |
| `docs/report/ap1/`, `docs/report/ap2/` | Report of each activity (tables, figures) |
| `docs/report/methodology_notes.md` | Measurement method and the reasons for each choice |
| `docs/project/` | Activity statements (PDF) |

## AP1: Memory Affinity, Vectorization and Tiling

### 1. Memory Affinity (Loop Reorganization) - Baseline
- **Goal:** Understand the impact of memory access patterns.
- **Details:** Baseline 3x3 Gaussian blur in C with two traversal orders:
  - **Row-Major:** Contiguous memory access.
  - **Column-Major:** Non-contiguous memory access.
- **Configuration:** Compiler vectorization disabled (`-fno-tree-vectorize`).
- **Measurements:** Matrix sizes $1024 \times 1024$, $2048 \times 2048$ and $4096 \times 4096$. Metrics: processing time, cache misses (L1, L2, L3), MFLOPS, CPI, average power (W), energy (J) and energy efficiency (MFLOPS/W).

### 2. Isolated Vectorization (SIMD Pragmas)
- **Goal:** Measure the gain of SIMD instructions without loop tiling.
- **Details:** Row-major loop with `#pragma omp simd`, `aligned(...: 32)`, `__restrict__` and `aligned_alloc`.
- **Configuration:** `-O3 -mavx2 -mfma -fopenmp-simd`.

### 3. Memory Tiling (Blocking) - Isolated and Combined
- **Isolated Tiling:** Block sizes $8, 16, 32$ and $64$ on the sequential version.
- **Combined (Tiling + Vectorization):** SIMD pragmas from Exercise 2 inside the tiled loops.

### 4. Comparison with Optimized Libraries
- **Details:** Same convolution with OpenCV `cv::filter2D`, fixed to 1 thread (same condition as the custom kernels).

## AP2: OpenMP Parallelization with Energy Efficiency

- **Goal:** Parallelize the best AP1 version with OpenMP and measure scalability and energy efficiency as the thread count increases.
- **Base version:** `simd_aligned` (AP1 Exercise 2). Tiling is not used because it was not significantly faster.
- **Pragmas:** `#pragma omp parallel for schedule(runtime) firstprivate(k)` on the rows + `#pragma omp simd aligned(input, output: 32)` on the columns. Flags: `-O3 -mavx2 -mfma -fopenmp`.
- **Experiments** (`parallel/scripts/parallel_omp.sh`, N = 1024, 2048, 4096, 10 runs each):

| Experiment | Configuration | Threads |
|---|---|---|
| `reference` | `simd_aligned` and OpenCV, 1 thread on CPU 2 (same session) | 1 |
| `main` | Placement `p-ht-e` (P-cores, then HT siblings, then E-cores), `static` | 1-16 |
| `places` | Placements `ht-p-e`, `p-e-ht`, `none` (no binding), `static` | 1, 2, 4, ..., 16 |
| `schedules` | Schedules `dynamic,1`, `dynamic,16`, `guided`, placement `p-ht-e` | 1, 2, 4, ..., 16 |

- **Metrics:** total time, speedup ($T_1 / T_p$), parallel efficiency ($S / p$), L1/L2/L3 misses (P-cores + E-cores), MFLOPS, average power (W), energy (J) and MFLOPS/W.
- OpenCV `cv::filter2D` does not use more than one core for this filter, so it stays at 1 thread (see `docs/report/methodology_notes.md`).

## How to Run

### Prerequisites
- Fedora (DNF) with GCC/G++, `make`, `perf`, OpenCV (`opencv-devel`) and Python 3. Install them with:
  ```bash
  make install/tools
  ```
- Python libraries for the analysis (`requirements.txt`: matplotlib), installed in a local `.venv`:
  ```bash
  make install/analysis-tools     # create .venv and pip install -r requirements.txt
  make uninstall/analysis-tools   # remove .venv
  ```
  The benchmark profilers run with `sudo python3` and use only the standard library, so they do not need the `.venv`.
- `sudo` access: the benchmarks read RAPL energy (root only) and AP2 switches the `tuned` power profile.

### Build and Test
```bash
make build          # all modules
make test           # all unit tests
make <module>-build # one module, e.g. make parallel-build
make <module>-test  # one module, e.g. make parallel-test
```

Binary CLI:

| Module | Command |
|---|---|
| `baseline` | `bin/<binary> <N> <mode>` (0 = row, 1 = col) |
| `vectorize`, `library` | `bin/<binary> <N>` |
| `blocking` | `bin/<binary> <N> <B>` |
| `parallel` | `bin/parallel_omp <N> <threads>` (placement and schedule from `OMP_PLACES`, `OMP_PROC_BIND`, `OMP_SCHEDULE`) |

Each binary prints a `JSON_METRICS:{...}` line with the time per call, repetitions and energy.

### AP1 Benchmarks
```bash
make baseline-run vectorize-run blocking-run library-run
make analysis/ap1   # compile data + docs/report/ap1/resume.md + docs/report/ap1/figures/
```

### AP2 Benchmarks
The AP2 script switches the `tuned` profile to `throughput-performance` (governor, EPP and `platform_profile` = performance) and restores the previous profile at the end, also on Ctrl+C.

```bash
make parallel-build vectorize-build-aligned library-build

# Quick check (~1 min): look for "Power profile : tuned=throughput-performance" and energy > 0
EXPERIMENTS=main SIZES=1024 THREADS="1 16" RUNS=2 make parallel-run

# Full run: 216 configurations x 10 runs (~2 h 25)
make parallel-run

make analysis/ap2   # compile data + docs/report/ap2/resume.md + docs/report/ap2/figures/
```

`make analysis` runs both analyses. `make analysis/data` only merges the CSVs into `docs/data/compiled/`.

Environment variables to change the AP2 run: `EXPERIMENTS` (`reference main places schedules`), `SIZES`, `THREADS` (main experiment), `CMP_THREADS` (places and schedules), `RUNS`, `ALT_PLACES`, `ALT_SCHEDULES`.

### Measurement Method
Each binary does one untimed call, calibrates the repetitions so the measured loop lasts about 1 s, and measures time, RAPL energy and `perf` counters around the same loop. Before each run, the profiler waits until the CPU package is at or below 60 °C. AP1 runs are pinned to CPU 2 (a P-core); AP2 places the threads with `OMP_PLACES`. Details: `docs/report/methodology_notes.md`.

## Deliverables
- **AP1:** source code of all versions (baseline, vectorized, tiled, library) and the report in `docs/report/ap1/`.
- **AP2:** source code of the base version (`vectorize/`, `simd_aligned`) and of the parallel version (`parallel/`), the input data set (generated in the program with a fixed seed: `srand(42)`, values 0-255), and the report in `docs/report/ap2/`.
