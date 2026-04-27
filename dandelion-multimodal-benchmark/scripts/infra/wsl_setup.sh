#!/usr/bin/env bash
# wsl_setup.sh — installs build dependencies and clones ETH EASL repos.
#
# Run from WSL2 AS ROOT (avoids sudo password prompts):
#   wsl -u root -- bash /path/to/wsl_setup.sh
#
# Or if sudo is configured NOPASSWD for your user:
#   bash /path/to/wsl_setup.sh

set -euo pipefail

# ── privilege check ──────────────────────────────────────────────────────
if [ "$(id -u)" -eq 0 ]; then
    SUDO=""          # running as root, no sudo needed
    USER_HOME="/root"
else
    SUDO="sudo"      # running as normal user — sudo must be NOPASSWD
    USER_HOME="$HOME"
fi

echo "[1/6] apt update..."
$SUDO apt-get update -qq 2>/dev/null

echo "[2/6] Installing build deps (cmake, clang, tesseract, llvm, python3)..."
$SUDO DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  build-essential cmake clang lld git curl pkg-config \
  libssl-dev libtesseract-dev libleptonica-dev \
  tesseract-ocr tesseract-ocr-eng \
  python3-pip llvm 2>/dev/null

echo "[3/6] Installing Rust (for current user)..."
# Rust must be installed as the target user, not root.
if [ "$(id -u)" -eq 0 ]; then
    # Install for root — cargo goes to /root/.cargo
    if ! command -v rustc &>/dev/null && ! [ -f /root/.cargo/bin/rustc ]; then
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y \
            --default-toolchain stable --no-modify-path 2>/dev/null
    fi
    source /root/.cargo/env 2>/dev/null || true
else
    if ! command -v rustc &>/dev/null && ! [ -f "$HOME/.cargo/bin/rustc" ]; then
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y \
            --default-toolchain stable --no-modify-path 2>/dev/null
    fi
    source "$HOME/.cargo/env" 2>/dev/null || true
fi

echo "[4/6] Cloning eth-easl/dandelion..."
mkdir -p "$USER_HOME/dandelion-bench"
cd "$USER_HOME/dandelion-bench"
if [ ! -d dandelion ]; then
    git clone --depth 1 https://github.com/eth-easl/dandelion.git 2>&1 | tail -3
else
    echo "  already cloned"
fi

echo "[5/6] Cloning eth-easl/dandelionSDK..."
if [ ! -d dandelionSDK ]; then
    git clone --depth 1 https://github.com/eth-easl/dandelionSDK.git 2>&1 | tail -3
else
    echo "  already cloned"
fi

echo "[6/6] Verifying tool versions..."
source "$USER_HOME/.cargo/env" 2>/dev/null || true
cmake  --version   | head -1
clang  --version   | head -1
tesseract --version 2>&1 | head -1
rustc  --version   2>/dev/null || echo "  rustc: open a new shell to source ~/.cargo/env"
echo "  dandelion:    $(ls $USER_HOME/dandelion-bench/dandelion 2>/dev/null && echo cloned || echo MISSING)"
echo "  dandelionSDK: $(ls $USER_HOME/dandelion-bench/dandelionSDK 2>/dev/null && echo cloned || echo MISSING)"
echo "=== SETUP_COMPLETE ==="
