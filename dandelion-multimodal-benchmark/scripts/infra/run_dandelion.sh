#!/usr/bin/env bash
# run_dandelion.sh — runs all three Dandelion DAG granularity variants
#                    against the benchmark dataset and writes results to
#                    results/raw/<variant>/
#
# Usage (inside WSL2, from repo root):
#   bash scripts/infra/run_dandelion.sh [--skip-server]
#
# Requirements:
#   build/dandelion_server  build/mmu_worker  build/dandelion_elfs/*.elf
#   Python3 with openai-whisper + pytesseract (for sidecar)

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$REPO/build"
SIDECAR_PORT=8765
DANDELION_PORT=9000
SKIP_SERVER=0
[[ "${1:-}" == "--skip-server" ]] && SKIP_SERVER=1

# ── helpers ──────────────────────────────────────────────────────────────
wait_port() {
    local port="$1" label="$2" tries=0
    while ! curl -sf "http://127.0.0.1:${port}/health" &>/dev/null; do
        sleep 0.5
        (( tries++ ))
        if (( tries > 30 )); then
            echo "ERROR: $label did not start on port $port" >&2; exit 1
        fi
    done
    echo "  $label ready on :$port"
}

cleanup() {
    echo "[cleanup] stopping background processes..."
    kill "$SIDECAR_PID" 2>/dev/null || true
    kill "$DN_PID"      2>/dev/null || true
}
trap cleanup EXIT

# ── 1. Start inference sidecar ───────────────────────────────────────────
echo "=== [1] Starting inference sidecar (port $SIDECAR_PORT) ==="
python3 "$REPO/scripts/inference_server.py" \
    --port "$SIDECAR_PORT" \
    --whisper-model "$REPO/models/ggml-tiny.bin" \
    2>>"$REPO/results/raw/sidecar.log" &
SIDECAR_PID=$!
wait_port "$SIDECAR_PORT" "inference sidecar"

# ── 2. Run each variant ───────────────────────────────────────────────────
for VARIANT in fine coarse monolithic; do
    echo ""
    echo "=== [2] Variant: $VARIANT ==="

    PRELOAD="$REPO/src/dandelion/dag/${VARIANT%olithic}_preload.json"
    # monolithic → mono_preload.json
    [[ "$VARIANT" == "monolithic" ]] && PRELOAD="$REPO/src/dandelion/dag/mono_preload.json"

    OUT_DIR="$REPO/results/raw/dandelion_${VARIANT}"
    mkdir -p "$OUT_DIR"

    # Start the Dandelion server with this variant's composition
    "$BUILD/dandelion_server" \
        --config "$PRELOAD" \
        --dc "$REPO/src/dandelion/dag/${VARIANT}.dc" \
        --mmu-worker "$BUILD/mmu_worker" \
        --port "$DANDELION_PORT" \
        2>>"$OUT_DIR/server.log" &
    DN_PID=$!
    sleep 2  # give it a moment to bind

    # Send requests from dataset
    python3 - <<'PYEOF' "$REPO" "$OUT_DIR" "$DANDELION_PORT" "$VARIANT"
import sys, json, time, urllib.request

repo, out_dir, port, variant = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
url = f"http://127.0.0.1:{port}/invoke"

results = []
for ds_file, modality in [
    (f"{repo}/datasets/text/samples.jsonl",        "text"),
    (f"{repo}/datasets/audio/audio_manifest.jsonl", "audio"),
    (f"{repo}/datasets/images/image_manifest.jsonl","image"),
]:
    try:
        lines = open(ds_file).readlines()
    except FileNotFoundError:
        print(f"  [warn] {ds_file} not found, skipping", file=sys.stderr)
        continue

    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        sample = json.loads(line)
        sid    = sample.get("id", "?")
        inp    = sample.get("input", sample.get("path", ""))

        payload = json.dumps({
            "id": sid, "modality": modality, "input": inp
        }).encode()

        t0 = time.perf_counter()
        try:
            req  = urllib.request.Request(url, data=payload,
                                           headers={"Content-Type": "application/json"})
            resp = urllib.request.urlopen(req, timeout=30)
            body = resp.read().decode()
            elapsed_ms = (time.perf_counter() - t0) * 1000
            result = json.loads(body)
            result["elapsed_ms"] = round(elapsed_ms, 2)
            result["system"]     = f"dandelion_{variant}"
            results.append(result)
            print(f"  {sid}  {result.get('intent','?')}  {elapsed_ms:.1f}ms")
        except Exception as e:
            print(f"  ERROR {sid}: {e}", file=sys.stderr)

with open(f"{out_dir}/results.jsonl", "w") as f:
    for r in results:
        f.write(json.dumps(r) + "\n")

print(f"  Wrote {len(results)} results to {out_dir}/results.jsonl")
PYEOF

    # Stop the server for this variant before starting the next
    kill "$DN_PID" 2>/dev/null || true
    wait "$DN_PID" 2>/dev/null || true
    unset DN_PID
    sleep 1
done

echo ""
echo "=== ALL VARIANTS COMPLETE ==="
echo "Results in: $REPO/results/raw/"
ls -lh "$REPO/results/raw/"
