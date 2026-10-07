#!/bin/bash

mkdir -p docs/tools
OUTPUT="docs/tools/tools.md"

echo "Generating tools documentation..."

GCC_VER=$(gcc --version | head -n 1 2>/dev/null || echo "Not found")
GPP_VER=$(g++ --version | head -n 1 2>/dev/null || echo "Not found")
PERF_VER=$(perf --version 2>/dev/null || echo "Not installed/available")
POWERCAP_VER=$(powercap-info 2>&1 | head -n 1 || echo "Not installed/available")
LIKWID_VER=$(likwid-powermeter -v 2>/dev/null || echo "Not installed/available")
OPENCV_VER=$(pkg-config --modversion opencv4 2>/dev/null || echo "Not installed/available")

cat <<EOF > "$OUTPUT"
# Tools and Software Versions

This document lists the toolchain and energy measurement interfaces available for the project.

## Compilers
- **C Compiler (gcc):** $GCC_VER
- **C++ Compiler (g++):** $GPP_VER

## Profiling and Energy Measurement Tools
Tools that can be used to measure performance and energy consumption (e.g., RAPL events, CPI, Cache Misses):

- **perf:** $PERF_VER
- **powercap:** $POWERCAP_VER
- **likwid-powermeter:** $LIKWID_VER
- **OpenCV (pkg-config):** $OPENCV_VER
EOF

echo "Done generating docs/tools/tools.md"
