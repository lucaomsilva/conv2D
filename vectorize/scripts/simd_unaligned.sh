#!/bin/bash

# Resolve vectorize and root directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VECTORIZE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$VECTORIZE_DIR/.." && pwd)"

BINARY="$VECTORIZE_DIR/bin/simd_unaligned"
DOCS_DIR="$ROOT_DIR/docs"

if [ ! -f "$BINARY" ]; then
  echo "Error: Binary $BINARY not found. Please compile first using 'make build-unaligned' or 'make build'."
  exit 1
fi

mkdir -p "$DOCS_DIR/data/vectorize"

SIZES=(1024 2048 4096)
MODE=0 # Row-Major only for vectorization

echo "=========================================================="
echo " Starting Benchmarks for simd_unaligned (Pragma SIMD)     "
echo "=========================================================="

for N in "${SIZES[@]}"; do
  CSV_FILE="$DOCS_DIR/data/vectorize/simd_unaligned_${N}.csv"

  # Remove existing CSV file for a fresh start with clean header
  rm -f "$CSV_FILE"

  echo "----------------------------------------------------------"
  echo " Configuration : Matrix ${N}x${N} | Mode: Row-Major (SIMD Unaligned)"
  echo " CSV Output    : ${CSV_FILE}"
  echo "----------------------------------------------------------"

  PROFILER="$VECTORIZE_DIR/scripts/profile_metrics.py"
  sudo python3 "$PROFILER" --binary "$BINARY" --size "$N" --mode "$MODE" --output-csv "$CSV_FILE" --runs 10 --warmup 5
done

echo "=========================================================="
echo " All simd_unaligned benchmarks completed successfully!     "
echo "=========================================================="
