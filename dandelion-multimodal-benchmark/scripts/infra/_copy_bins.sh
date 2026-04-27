#!/usr/bin/env bash
BUILD="/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/build"
mkdir -p "$BUILD"
cp ~/dandelion-bench/dandelion/target/release/dandelion_server "$BUILD/"
cp ~/dandelion-bench/dandelion/target/release/mmu_worker "$BUILD/"
ls -lh "$BUILD/dandelion_server" "$BUILD/mmu_worker"
echo "OK"
