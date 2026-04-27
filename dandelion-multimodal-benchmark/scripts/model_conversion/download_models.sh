#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODELS_DIR="$SCRIPT_DIR/../../models"
mkdir -p "$MODELS_DIR"

WHISPER_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin"

if [ ! -f "$MODELS_DIR/ggml-tiny.bin" ]; then
  echo "Downloading whisper.cpp tiny model (~39 MB)..."
  curl -L --progress-bar "$WHISPER_URL" -o "$MODELS_DIR/ggml-tiny.bin"
  echo "Done: $MODELS_DIR/ggml-tiny.bin"
else
  echo "whisper tiny model already present, skipping."
fi

echo ""
echo "Next step: run scripts/model_conversion/convert_to_onnx.py"
