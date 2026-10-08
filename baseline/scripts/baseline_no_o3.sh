#!/bin/bash

# Resolve baseline and root directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASELINE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$BASELINE_DIR/.." && pwd)"

BINARY="$BASELINE_DIR/bin/baseline_no_o3"
DOCS_DIR="$ROOT_DIR/docs"

if [ ! -f "$BINARY" ]; then
    echo "Error: Binary $BINARY not found. Please compile first using 'make build-no_o3' or 'make build'."
    exit 1
fi

mkdir -p "$DOCS_DIR"

SIZES=(1024 2048 4096)
MODES=(0 1) # 0 = Row-Major, 1 = Column-Major

echo "=========================================================="
echo " Starting Benchmarks for baseline_no_o3 (No -O3 Optimization) "
echo "=========================================================="

for N in "${SIZES[@]}"; do
    for M in "${MODES[@]}"; do
        if [ "$M" -eq 0 ]; then
            MODE_NAME="row"
            MODE_LABEL="Row-Major"
        else
            MODE_NAME="col"
            MODE_LABEL="Column-Major"
        fi

        CSV_FILE="$DOCS_DIR/data/baseline/baseline_no_o3_${N}_${MODE_NAME}.csv"

        # Remove existing CSV file for a fresh start with clean header
        rm -f "$CSV_FILE"

        echo "----------------------------------------------------------"
        echo " Configuration : Matrix ${N}x${N} | Mode: ${MODE_LABEL} (${MODE_NAME})"
        echo " CSV Output    : ${CSV_FILE}"
        echo "----------------------------------------------------------"

        # 1. Warmup: Run binary 5 times without CSV path (not logged to CSV)
        echo " -> Executing 5 warmup runs..."
        for w in {1..5}; do
            "$BINARY" "$N" "$M" > /dev/null 2>&1
        done

        # 2. Benchmark: Run binary 10 times with CSV path (logged to CSV)
        echo " -> Executing 10 benchmark runs..."
        for r in {1..10}; do
            "$BINARY" "$N" "$M" "$CSV_FILE" > /dev/null 2>&1
        done

        echo " -> Finished 10 runs for ${N}x${N} ${MODE_LABEL}."
    done
done

echo "=========================================================="
echo " All baseline_no_o3 benchmarks completed successfully!    "
echo "=========================================================="
