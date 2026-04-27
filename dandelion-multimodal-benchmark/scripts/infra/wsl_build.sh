#!/usr/bin/env bash
# wsl_build.sh — builds Dandelion runtime + compute function ELFs inside WSL2.
#
# Prerequisites (run wsl_setup.sh first or install manually):
#   ~/dandelion-bench/dandelion        (eth-easl/dandelion Rust repo)
#   ~/dandelion-bench/dandelionSDK     (eth-easl/dandelionSDK CMake repo)
#   cmake, clang, lld, cargo
#
# Usage (from WSL2, inside the repo root):
#   bash scripts/infra/wsl_build.sh
#
# Outputs:
#   build/dandelion_server      — Dandelion server binary (Rust)
#   build/mmu_worker            — MMU isolation worker (Rust)
#   build/dandelion_elfs/*.elf  — compute function ELFs (cross-compiled C)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BENCH_HOME="$HOME/dandelion-bench"
DANDELION_SRC="$BENCH_HOME/dandelion"
SDK_SRC="$BENCH_HOME/dandelionSDK"
BUILD_DIR="$REPO_ROOT/build"
ELF_DIR="$BUILD_DIR/dandelion_elfs"

source "$HOME/.cargo/env" 2>/dev/null || true

# ── 1. Build Dandelion server + mmu_worker (Rust) ───────────────────────
echo "=== [1/4] Building Dandelion server ==="
cd "$DANDELION_SRC"

# Check available feature flags from Cargo.toml
if grep -q 'reqwest_io' Cargo.toml 2>/dev/null; then
    FEATURES="mmu,reqwest_io,timestamp"
else
    FEATURES="mmu,timestamp"
fi
echo "  cargo features: $FEATURES"

cargo build --release --bin dandelion_server --features "$FEATURES" 2>&1 | tail -5
cargo build --release --bin mmu_worker --features "mmu" 2>&1 | tail -5

mkdir -p "$BUILD_DIR"
cp target/release/dandelion_server "$BUILD_DIR/dandelion_server"
cp target/release/mmu_worker       "$BUILD_DIR/mmu_worker"
echo "  OK  dandelion_server  mmu_worker"

# ── 2. Build and install DandelionSDK ────────────────────────────────────
echo "=== [2/4] Building DandelionSDK ==="
SDK_BUILD="$BUILD_DIR/sdk_build"
SDK_INSTALL="$SDK_BUILD/dandelion_sdk"

mkdir -p "$SDK_BUILD"
cd "$SDK_BUILD"

cmake "$SDK_SRC" \
    -DARCHITECTURE=x86_64 \
    -DDANDELION_PLATFORM=mmu_linux \
    -DNEWLIB=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDK_INSTALL" \
    2>&1 | tail -10

cmake --build . --target install --parallel "$(nproc)" 2>&1 | tail -10

# Verify the installed toolchain file exists
TOOLCHAIN="$SDK_INSTALL/dandelion-toolchain.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
    # SDK without NEWLIB might not generate the toolchain file — check
    TOOLCHAIN="$(find "$SDK_INSTALL" -name '*.cmake' | head -1)"
    echo "  Toolchain: $TOOLCHAIN"
fi
echo "  OK  DandelionSDK installed to $SDK_INSTALL"

# ── 3. Build compute function ELFs ──────────────────────────────────────
echo "=== [3/4] Building compute function ELFs ==="
NODES_SRC="$REPO_ROOT/src/dandelion/nodes"
NODES_BUILD="$BUILD_DIR/nodes_build"
mkdir -p "$NODES_BUILD" "$ELF_DIR"

cd "$NODES_BUILD"
cmake "$NODES_SRC" \
    -DDANDELION_SDK_INSTALL="$SDK_INSTALL" \
    -DCMAKE_BUILD_TYPE=Release \
    2>&1 | tail -10

cmake --build . --parallel "$(nproc)" 2>&1 | tail -10

# ── 4. Verify all outputs ────────────────────────────────────────────────
echo "=== [4/4] Verification ==="
for bin in dandelion_server mmu_worker; do
    [ -x "$BUILD_DIR/$bin" ] && echo "  OK  $bin" || echo "  MISSING  $bin"
done

for elf in ingress normalize classify audio_asr image_ocr format_output \
           coarse_text coarse_audio coarse_image mono_all; do
    f="$ELF_DIR/${elf}.elf"
    if [ -f "$f" ]; then
        echo "  OK  ${elf}.elf  ($(stat -c%s "$f") bytes)"
    else
        echo "  MISSING  ${elf}.elf"
    fi
done

echo "=== BUILD_COMPLETE ==="
