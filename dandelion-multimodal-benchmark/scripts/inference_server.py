#!/usr/bin/env python3
"""
inference_server.py — sidecar HTTP server for Dandelion DAG nodes.

Exposes two endpoints consumed by audio_asr / image_ocr compute functions:
  POST /asr   body: path to WAV file      → {"transcript": "..."}
  POST /ocr   body: path to image file    → {"text": "..."}

Run in WSL2 before launching the Dandelion server:
  python3 scripts/inference_server.py \
      --whisper-model models/ggml-tiny.bin \
      --tessdata /usr/share/tesseract-ocr/5/tessdata \
      --port 8765
"""

import argparse
import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

# ── optional imports (fail at runtime, not import time) ──────────────────────
try:
    import whisper as _whisper_openai  # openai-whisper (pip)
    _HAVE_OPENAI_WHISPER = True
except ImportError:
    _HAVE_OPENAI_WHISPER = False

try:
    import pytesseract
    from PIL import Image as PILImage
    _HAVE_TESSERACT = True
except ImportError:
    _HAVE_TESSERACT = False

# ── globals set at startup ─────────────────────────────────────────────────
_whisper_model = None
_tessdata_dir  = None
_port          = 8765


# ── ASR helper ────────────────────────────────────────────────────────────
def transcribe_wav(wav_path: str) -> str:
    """Return transcript string for a WAV file."""
    if _HAVE_OPENAI_WHISPER and _whisper_model is not None:
        result = _whisper_model.transcribe(wav_path, fp16=False)
        return result.get("text", "").strip()

    # Fallback: try subprocess whisper.cpp CLI if available
    import subprocess, tempfile
    cli = os.path.join(os.path.dirname(__file__), "..", "build",
                       "whisper_cli", "whisper-cli")
    if not os.path.exists(cli):
        return ""
    args = [cli, "-m", os.environ.get("WHISPER_MODEL", "models/ggml-tiny.bin"),
            "-f", wav_path, "-otxt", "-of", "/tmp/wsp_out"]
    subprocess.run(args, capture_output=True)
    txt_path = "/tmp/wsp_out.txt"
    if os.path.exists(txt_path):
        with open(txt_path) as f:
            return f.read().strip()
    return ""


# ── OCR helper ────────────────────────────────────────────────────────────
def ocr_image(img_path: str) -> str:
    """Return OCR text from an image file."""
    if not _HAVE_TESSERACT:
        return ""
    try:
        cfg = f"--tessdata-dir {_tessdata_dir}" if _tessdata_dir else ""
        img = PILImage.open(img_path).convert("L")  # grayscale
        text = pytesseract.image_to_string(img, config=cfg)
        return text.strip()
    except Exception as e:
        print(f"[ocr] error: {e}", file=sys.stderr)
        return ""


# ── HTTP handler ──────────────────────────────────────────────────────────
class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        # emit compact JSON log line to stderr
        print(json.dumps({"ts": time.time(), "msg": fmt % args}),
              file=sys.stderr, flush=True)

    def _read_body(self) -> bytes:
        length = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(length) if length else b""

    def _send_json(self, obj: dict, status: int = 200):
        body = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        body = self._read_body().decode(errors="replace").strip()

        if self.path == "/asr":
            # body may be:  "path/to/file.wav"  OR  "url\npath"
            wav_path = body.split("\n")[-1].strip()
            t0 = time.perf_counter()
            transcript = transcribe_wav(wav_path)
            elapsed_ms = (time.perf_counter() - t0) * 1000
            self._send_json({"transcript": transcript,
                             "elapsed_ms": round(elapsed_ms, 1)})

        elif self.path == "/ocr":
            img_path = body.split("\n")[-1].strip()
            t0 = time.perf_counter()
            text = ocr_image(img_path)
            elapsed_ms = (time.perf_counter() - t0) * 1000
            self._send_json({"text": text,
                             "elapsed_ms": round(elapsed_ms, 1)})

        elif self.path == "/health":
            self._send_json({"status": "ok",
                             "whisper": _HAVE_OPENAI_WHISPER,
                             "tesseract": _HAVE_TESSERACT})
        else:
            self._send_json({"error": f"unknown endpoint: {self.path}"}, 404)


# ── main ──────────────────────────────────────────────────────────────────
def main():
    global _whisper_model, _tessdata_dir, _port

    ap = argparse.ArgumentParser(description="Dandelion inference sidecar")
    ap.add_argument("--whisper-model", default="models/ggml-tiny.bin",
                    help="Path to openai-whisper or ggml model")
    ap.add_argument("--tessdata", default="",
                    help="Tesseract tessdata directory (empty = system default)")
    ap.add_argument("--port", type=int, default=8765)
    args = ap.parse_args()

    _port        = args.port
    _tessdata_dir = args.tessdata if args.tessdata else None

    # Load whisper model eagerly so first request isn't penalised.
    if _HAVE_OPENAI_WHISPER:
        model_name = os.path.basename(args.whisper_model).replace("ggml-", "").replace(".bin", "")
        print(json.dumps({"event": "loading_whisper", "model": model_name}),
              file=sys.stderr, flush=True)
        try:
            _whisper_model = _whisper_openai.load_model(model_name)
        except Exception as e:
            print(json.dumps({"event": "whisper_load_failed", "error": str(e)}),
                  file=sys.stderr, flush=True)
            _whisper_model = None
    else:
        print(json.dumps({"event": "whisper_unavailable",
                          "hint": "pip install openai-whisper"}),
              file=sys.stderr, flush=True)

    if not _HAVE_TESSERACT:
        print(json.dumps({"event": "tesseract_unavailable",
                          "hint": "pip install pytesseract pillow; apt install tesseract-ocr"}),
              file=sys.stderr, flush=True)

    server = HTTPServer(("127.0.0.1", _port), Handler)
    print(json.dumps({"event": "server_ready", "port": _port}),
          file=sys.stderr, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
