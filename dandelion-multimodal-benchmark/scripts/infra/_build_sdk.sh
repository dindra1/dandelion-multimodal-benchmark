#!/usr/bin/env bash
set -euo pipefail

# Use pip-installed cmake if system cmake is too old
export PATH="$HOME/.local/bin:$PATH"

SDK_SRC="$HOME/dandelion-bench/dandelionSDK"
SDK_BUILD="$HOME/dandelion-bench/sdk_build"
SDK_INSTALL="$SDK_BUILD/dandelion_sdk"

mkdir -p "$SDK_BUILD"
cd "$SDK_BUILD"

echo "=== Configuring DandelionSDK ==="
cmake "$SDK_SRC" \
    -DARCHITECTURE=x86_64 \
    -DDANDELION_PLATFORM=mmu_linux \
    -DNEWLIB=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_INSTALL_PREFIX="$SDK_INSTALL" 2>&1 | tail -10

echo "=== Building and installing DandelionSDK ==="
cmake --build . --target install --parallel "$(nproc)" 2>&1 | tail -10

echo "=== Installed files ==="
ls "$SDK_INSTALL/"
echo "SDK_INSTALL=$SDK_INSTALL"
