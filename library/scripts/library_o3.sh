#!/bin/bash

# Resolve library and root directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBRARY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$LIBRARY_DIR/.." && pwd)"

BINARY="$LIBRARY_DIR/bin/library_o3"
DOCS_DIR="$ROOT_DIR/docs"

if [ ! -f "$BINARY" ]; then
  echo "Error: Binary $BINARY not found. Please compile first using 'make build'."
  exit 1
fi

mkdir -p "$DOCS_DIR/data/library"

SIZES=(1024 2048 4096)

echo "=========================================================="
echo " Starting Benchmarks for library_o3 (C++ Default)         "
echo "=========================================================="

for N in "${SIZES[@]}"; do
  CSV_FILE="$DOCS_DIR/data/library/library_o3_${N}.csv"

  # Remove existing CSV file for a fresh start with clean header
  rm -f "$CSV_FILE"

  echo "----------------------------------------------------------"
  echo " Configuration : Matrix ${N}x${N}"
  echo " CSV Output    : ${CSV_FILE}"
  echo "----------------------------------------------------------"

  PROFILER="$LIBRARY_DIR/scripts/profile_metrics.py"
  sudo python3 "$PROFILER" --binary "$BINARY" --size "$N" --output-csv "$CSV_FILE" --runs 10
done

echo "=========================================================="
echo " All library_o3 benchmarks completed successfully!        "
echo "=========================================================="
