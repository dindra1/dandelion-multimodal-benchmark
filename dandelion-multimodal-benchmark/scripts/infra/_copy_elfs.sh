#!/usr/bin/env bash
set -euo pipefail

SRC="$HOME/dandelion-bench/nodes_build"
DST="/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/build/dandelion_elfs"

mkdir -p "$DST"

for elf in ingress normalize classify audio_asr image_ocr format_output \
           coarse_text coarse_audio coarse_image mono_all; do
    src_f="$SRC/${elf}.elf"
    if [ -f "$src_f" ]; then
        cp "$src_f" "$DST/${elf}.elf"
        echo "  OK  ${elf}.elf  ($(stat -c%s "$DST/${elf}.elf") bytes)"
    else
        echo "  MISSING  ${elf}.elf"
    fi
done
