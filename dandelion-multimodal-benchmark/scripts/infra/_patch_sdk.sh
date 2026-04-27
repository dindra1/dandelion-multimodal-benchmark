#!/usr/bin/env bash
set -euo pipefail

SDK_INSTALL="$HOME/dandelion-bench/sdk_build/dandelion_sdk"

# Create dummy create-compiler.sh
echo '#!/bin/bash' > "$SDK_INSTALL/create-compiler.sh"
chmod +x "$SDK_INSTALL/create-compiler.sh"

# Copy system clang builtins to the expected location
mkdir -p "$SDK_INSTALL/lib/generic"
cp /usr/lib/llvm-14/lib/clang/14.0.0/lib/linux/libclang_rt.builtins-x86_64.a \
   "$SDK_INSTALL/lib/generic/libclang_rt.builtins-x86_64.a"

# Fix: replace CMAKE_CURRENT_SOURCE_DIR with CMAKE_CURRENT_LIST_DIR in the SDK CMakeLists.txt
# so it works correctly when include()'d from another project
sed -i 's/CMAKE_CURRENT_SOURCE_DIR/CMAKE_CURRENT_LIST_DIR/g' "$SDK_INSTALL/CMakeLists.txt"

echo "Patched CMakeLists.txt:"
cat "$SDK_INSTALL/CMakeLists.txt"
