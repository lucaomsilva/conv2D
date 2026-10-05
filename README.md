# 2D Convolution (Stencil) Performance and Energy Efficiency Optimization

This repository contains the implementation of a 2D Convolution (Stencil) filter aimed at evaluating and optimizing computational performance and energy efficiency. The project explores the isolated and combined impacts of **memory affinity (cache optimization)**, **SIMD vectorization**, and **memory tiling (blocking)**, concluding with a comparison against highly optimized libraries like OpenCV.

This project was developed as part of the High-Performance Computing/Energy Efficiency coursework (PPAD - PPGCC).

## Project Structure and Exercises

The project is divided into four main exercises to systematically analyze performance and energy metrics:

### 1. Memory Affinity (Loop Reorganization) - Baseline
- **Goal:** Understand the impact of memory access patterns.
- **Details:** Implements a baseline 2D convolution (e.g., 3x3 Gaussian Blur or Sobel) in C/C++. Evaluates two traversal orders:
  - **Row-Major:** Contiguous memory access.
  - **Column-Major:** Non-contiguous memory access.
- **Configuration:** Compiler vectorization is explicitly disabled (e.g., `-fno-tree-vectorize`).
- **Measurements:** Evaluated on matrix sizes $1024 \times 1024$, $2048 \times 2048$, and $4096 \times 4096$. Metrics collected include total processing time, Cache Misses (L1, L2, L3), MFLOPS, CPI, average power (Watts), and energy efficiency (MFLOPS/Watt or Joules).

### 2. Isolated Vectorization (SIMD Pragmas)
- **Goal:** Measure performance gains from SIMD instructions without altering loop tiling structures.
- **Details:** Uses the most efficient loop organization from Exercise 1 and applies SIMD vectorization via pragmas (e.g., `#pragma omp simd` or `#pragma vector`) and memory alignment (`__attribute__((aligned(32)))` or `aligned_alloc`).
- **Configuration:** Compiler vectorization flags enabled (e.g., `-O3 -mavx2` or `-fopenmp-simd`).
- **Measurements:** Compares metrics against the non-vectorized baseline for the same matrix sizes.

### 3. Memory Tiling (Blocking) - Isolated and Combined
- **Goal:** Optimize cache usage via loop tiling and combine it with vectorization.
- **Details:** 
  - **Isolated Tiling:** Applies tiling to the sequential baseline with block sizes of $8, 16, 32$, and $64$. Identifies the most efficient block size for the processor's cache hierarchy.
  - **Combined (Tiling + Vectorization):** Integrates the SIMD pragmas from Exercise 2 with the optimal tiled structure.
- **Measurements:** Evaluates the energy efficiency (MFLOPS/Watt) and total consumption (Watts) compared to previous versions. Ensures safe boundary conditions.

### 4. Comparison with Optimized Libraries
- **Goal:** Benchmark the custom implementation against a production-ready, highly optimized library.
- **Details:** Implements the same 2D convolution utilizing a library like **OpenCV** (`cv::filter2D`) or Intel IPP.
- **Measurements:** Analyzes hardware resource utilization and energy consumption improvements achieved by the library compared to the custom "tiled + vectorized" version.

## Compilation and Execution

*(Instructions to be added based on the specific compiler and flags used, e.g., GCC/Clang, OpenCV linking.)*

### Prerequisites
- C++ Compiler (GCC/Clang) supporting C++11 or higher.
- OpenMP (for SIMD pragmas).
- OpenCV (for Exercise 4).
- Performance measurement tools (e.g., `perf`, `likwid-powermeter`, or RAPL interfaces).

### Example Compilation for OpenCV (Exercise 4)
```bash
g++ -O3 script_opencv.cpp -o script_opencv `pkg-config --cflags --libs opencv4`
```

## Deliverables
- Source code for all versions: Baseline (reorganized), Vectorized, Tiled, and Library-based.
- A comprehensive report containing comparative analysis, charts, and tables for performance and energy efficiency metrics across all experiments.
