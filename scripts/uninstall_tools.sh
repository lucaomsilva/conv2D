#!/bin/bash
set -e

echo "=== Removing Performance & Energy Profiling Tools (DNF) ==="

# Check if dnf is available
if ! command -v dnf &> /dev/null; then
    echo "Error: dnf package manager was not found on this system."
    exit 1
fi

# Uninstall DNF Packages
echo "Uninstalling perf, powercap, OpenCV, and likwid..."
sudo dnf remove -y \
    perf \
    powercap-utils \
    opencv-devel \
    likwid \
    likwid-devel 2>/dev/null || true

echo "=== Uninstallation Complete! ==="
