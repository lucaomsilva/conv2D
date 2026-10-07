#!/bin/bash

mkdir -p docs/hardware
OUTPUT="docs/hardware/hardware.md"

echo "Generating hardware documentation..."

# Get CPU Info reliably from /proc/cpuinfo
VENDOR=$(grep -m 1 'vendor_id' /proc/cpuinfo | awk '{print $3}')
MODEL=$(grep -m 1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^[ \t]*//')
THREADS=$(grep -c '^processor' /proc/cpuinfo)
ARCH=$(uname -m)

# Get RAM
RAM=$(free -h | awk '/^Mem:/ {print $2}')

# Parse Vector Instructions
# This reads the flags line which is standard across Intel and AMD
FLAGS=$(grep -m 1 '^flags' /proc/cpuinfo)

# Find all AVX-related flags
AVX_FLAGS=$(echo "$FLAGS" | grep -o -i -E '\b(avx|avx2|avx512[^ \t]*)\b' | tr '\n' ' ' | sed 's/ $//')
# Find all SSE-related flags (including AMD specific like sse4a)
SSE_FLAGS=$(echo "$FLAGS" | grep -o -i -E '\b(sse|sse2|sse3|ssse3|sse4_1|sse4_2|sse4a)\b' | tr '\n' ' ' | sed 's/ $//')

# Write to markdown
cat <<EOF > "$OUTPUT"
# Hardware Specifications

This document captures the hardware details required for the performance and energy efficiency evaluation.

## CPU
- **Vendor:** $VENDOR
- **Model:** $MODEL
- **Architecture:** $ARCH
- **Total Threads/Cores:** $THREADS

## Memory
- **Total RAM:** $RAM

## Vector Instruction Support
Relevant SIMD instruction sets available on this machine:
- **AVX Support:** ${AVX_FLAGS:-None}
- **SSE Support:** ${SSE_FLAGS:-None}
EOF

echo "Done generating docs/hardware/hardware.md"
