#!/usr/bin/env bash
set -euo pipefail

export PATH="$HOME/.local/bin:$PATH"

SDK_INSTALL="$HOME/dandelion-bench/sdk_build/dandelion_sdk"
NODES_SRC="/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/src/dandelion/nodes"
NODES_BUILD="$HOME/dandelion-bench/nodes_build"
ELF_DIR="/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/build/dandelion_elfs"

mkdir -p "$NODES_BUILD" "$ELF_DIR"

echo "=== Configuring compute ELFs ==="
cd "$NODES_BUILD"
cmake "$NODES_SRC" \
    -DDANDELION_SDK_INSTALL="$SDK_INSTALL" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_BUILD_TYPE=Release

echo "=== Building ELFs ==="
cmake --build . --parallel "$(nproc)"

echo "=== Results ==="
for elf in ingress normalize classify audio_asr image_ocr format_output \
           coarse_text coarse_audio coarse_image mono_all; do
    f="$ELF_DIR/${elf}.elf"
    if [ -f "$f" ]; then
        echo "  OK  ${elf}.elf  ($(stat -c%s "$f") bytes)"
    else
        echo "  MISSING  ${elf}.elf"
    fi
done
