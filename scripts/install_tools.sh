#!/bin/bash
set -e

echo "=== Installing Performance & Energy Profiling Tools (DNF) ==="

# Check if dnf is available
if ! command -v dnf &> /dev/null; then
    echo "Error: dnf package manager was not found on this system."
    exit 1
fi

sudo dnf check-update || true

# Core Build Tools & Utilities
echo "Installing compilers and build tools..."
sudo dnf install -y \
    gcc \
    gcc-c++ \
    make \
    bc \
    pkgconf-pkg-config \
    python3-pip

# Profiling & Energy Tools
echo "Installing perf, powercap, and OpenCV..."
sudo dnf install -y --disablerepo="*lionheartp*" --skip-unavailable \
    perf \
    powercap \
    opencv-devel || echo "Note: Some optional profiling packages skipped."

# Optional: Try installing likwid if available in repos
sudo dnf install -y likwid likwid-devel 2>/dev/null || echo "LIKWID not in standard repos; can be compiled from source if needed."

# Install CodeCarbon via pip
echo "Installing CodeCarbon (Python)..."
pip3 install codecarbon 2>/dev/null || pip install codecarbon 2>/dev/null || echo "CodeCarbon pip install skipped."

# Configure Non-Root Permissions for perf and powercap
echo "Configuring permissions for perf and powercap..."
sudo sysctl -w kernel.perf_event_paranoid=-1 || true
if [ -f /etc/sysctl.d/99-perf.conf ]; then
    echo "kernel.perf_event_paranoid = -1" | sudo tee /etc/sysctl.d/99-perf.conf > /dev/null
fi

sudo chmod -R a+r /sys/class/powercap/intel-rapl/ 2>/dev/null || true

echo "=== Installation & Setup Complete! ==="
