#!/usr/bin/env bash
# Vibesynth — one-shot build script
# Usage: chmod +x build.sh && ./build.sh
set -e

BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "==> Configuring (CMake will fetch JUCE on first run — takes a minute)..."
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"   # remove on Linux/Windows

echo "==> Building..."
cmake --build . --config Release -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

echo ""
echo "==> Done!"
echo "VST3: $(find . -name '*.vst3' -o -name '*.VST3' | head -1)"
echo "Standalone: $(find . -name 'Vibesynth' -type f | head -1)"
