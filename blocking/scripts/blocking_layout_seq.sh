#!/bin/bash

# Resolve blocking and root directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BLOCKING_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$BLOCKING_DIR/.." && pwd)"

BINARIES=(blocking_layout_seq blocking_layout_seq_hp)
DOCS_DIR="$ROOT_DIR/docs"

for BIN in "${BINARIES[@]}"; do
  if [ ! -f "$BLOCKING_DIR/bin/$BIN" ]; then
    echo "Error: Binary $BLOCKING_DIR/bin/$BIN not found. Please compile first using 'make build-layout' or 'make build'."
    exit 1
  fi
done

mkdir -p "$DOCS_DIR/data/blocking"

SIZES=(1024 2048 4096)
BLOCK_SIZES=(8 16 32 64)

echo "=========================================================="
echo " Starting Benchmarks for blocked layout (Seq, 4K / HP)    "
echo "=========================================================="

for BIN in "${BINARIES[@]}"; do
  BINARY="$BLOCKING_DIR/bin/$BIN"
  for N in "${SIZES[@]}"; do
    for B in "${BLOCK_SIZES[@]}"; do
      CSV_FILE="$DOCS_DIR/data/blocking/${BIN}_${N}_b${B}.csv"

      # Remove existing CSV file for a fresh start with clean header
      rm -f "$CSV_FILE"

      echo "----------------------------------------------------------"
      echo " Binary        : ${BIN}"
      echo " Configuration : Matrix ${N}x${N} | Block Size: ${B}x${B}"
      echo " CSV Output    : ${CSV_FILE}"
      echo "----------------------------------------------------------"

      PROFILER="$BLOCKING_DIR/scripts/profile_metrics.py"
      sudo python3 "$PROFILER" --binary "$BINARY" --size "$N" --block-size "$B" --output-csv "$CSV_FILE" --runs 10
    done
  done
done

echo "=========================================================="
echo " All blocked layout (Seq) benchmarks completed!           "
echo "=========================================================="
