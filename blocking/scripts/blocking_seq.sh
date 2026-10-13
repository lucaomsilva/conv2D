#!/bin/bash

# Resolve blocking and root directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BLOCKING_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$BLOCKING_DIR/.." && pwd)"

BINARY="$BLOCKING_DIR/bin/blocking_seq"
DOCS_DIR="$ROOT_DIR/docs"

if [ ! -f "$BINARY" ]; then
  echo "Error: Binary $BINARY not found. Please compile first using 'make build-seq' or 'make build'."
  exit 1
fi

mkdir -p "$DOCS_DIR/data/blocking"

SIZES=(1024 2048 4096)
BLOCK_SIZES=(8 16 32 64)

echo "=========================================================="
echo " Starting Benchmarks for blocking_seq (Isolated Tiling)   "
echo "=========================================================="

for N in "${SIZES[@]}"; do
  for B in "${BLOCK_SIZES[@]}"; do
    CSV_FILE="$DOCS_DIR/data/blocking/blocking_seq_${N}_b${B}.csv"

    # Remove existing CSV file for a fresh start with clean header
    rm -f "$CSV_FILE"

    echo "----------------------------------------------------------"
    echo " Configuration : Matrix ${N}x${N} | Block Size: ${B}x${B}"
    echo " CSV Output    : ${CSV_FILE}"
    echo "----------------------------------------------------------"

    PROFILER="$BLOCKING_DIR/scripts/profile_metrics.py"
    sudo python3 "$PROFILER" --binary "$BINARY" --size "$N" --block-size "$B" --output-csv "$CSV_FILE" --runs 10 --warmup 5
  done
done

echo "=========================================================="
echo " All blocking_seq benchmarks completed successfully!       "
echo "=========================================================="
