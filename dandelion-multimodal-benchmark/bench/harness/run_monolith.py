#!/usr/bin/env python3
"""
run_monolith.py — runs monolith_pipeline against all three modalities and
                  writes results to results/raw/monolith/.

Usage (from repo root, after building monolith_pipeline):
  python bench/harness/run_monolith.py \
      --binary build/monolith_pipeline.exe \
      --whisper-model models/ggml-tiny.bin \
      [--tessdata "C:/Program Files/Tesseract-OCR/tessdata"]
"""

import argparse
import json
import os
import subprocess
import sys
import time

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))


def find_dataset(rel_path: str) -> list[dict]:
    path = os.path.join(REPO, rel_path)
    if not os.path.exists(path):
        return []
    out = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                out.append(json.loads(line))
    return out


def run_request(binary: str, args: list[str],
                request_id: str, modality: str, inp: str) -> dict | None:
    env = os.environ.copy()
    dll_dirs = [os.path.dirname(binary), r"C:\Program Files\Tesseract-OCR"]
    env["PATH"] = ";".join(dll_dirs) + ";" + env.get("PATH", "")

    cmd = [binary,
           "--request-id", request_id,
           "--modality", modality,
           "--input", inp] + args

    t0 = time.perf_counter()
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60, env=env)
        elapsed_ms = (time.perf_counter() - t0) * 1000
    except subprocess.TimeoutExpired:
        print(f"  TIMEOUT {request_id}", file=sys.stderr)
        return None

    # stdout: result JSON line; stderr: metrics JSON line
    stdout = result.stdout.strip()
    stderr = result.stderr.strip()

    record = {"elapsed_wall_ms": round(elapsed_ms, 2), "system": "monolith"}
    if stdout:
        try:
            record.update(json.loads(stdout))
        except json.JSONDecodeError:
            record["raw_stdout"] = stdout

    # Parse metrics from stderr (may have multiple lines; pick the JSON ones)
    for line in stderr.splitlines():
        line = line.strip()
        if line.startswith("{"):
            try:
                record.update(json.loads(line))
            except json.JSONDecodeError:
                pass

    return record


def run_batch(binary: str, extra_args: list[str],
              all_samples: list[tuple]) -> list[dict]:
    """Run all samples in one process invocation (models loaded once)."""
    stdin_lines = []
    meta = {}
    for sid, modality, inp, label in all_samples:
        # separators=(',',':') ensures no spaces — matches the C++ naive parser
        stdin_lines.append(json.dumps(
            {"id": sid, "modality": modality, "input": inp},
            separators=(',', ':')
        ))
        meta[sid] = (modality, label)

    # Ensure DLL directories are in PATH (Tesseract, whisper.cpp, etc.)
    env = os.environ.copy()
    dll_dirs = [
        os.path.dirname(binary),                      # beside the exe
        r"C:\Program Files\Tesseract-OCR",
    ]
    env["PATH"] = ";".join(dll_dirs) + ";" + env.get("PATH", "")

    cmd = [binary, "--batch"] + extra_args
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, env=env)
    stdout, stderr = proc.communicate(input="\n".join(stdin_lines), timeout=300)

    results = []
    for line in stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except json.JSONDecodeError:
            continue
        sid = r.get("id", "?")
        if sid in meta:
            r["label"] = meta[sid][1]
        r["system"] = "monolith"
        results.append(r)

    # Merge per-request metrics emitted to stderr
    metrics_by_id: dict[str, dict] = {}
    for line in stderr.splitlines():
        line = line.strip()
        if line.startswith("{"):
            try:
                m = json.loads(line)
                rid = m.get("request_id", "")
                if rid:
                    metrics_by_id[rid] = m
            except json.JSONDecodeError:
                pass

    for r in results:
        sid = r.get("id", "")
        if sid in metrics_by_id:
            # Merge metrics (don't overwrite fields already in r)
            for k, v in metrics_by_id[sid].items():
                if k not in r:
                    r[k] = v
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", default=os.path.join(REPO, "build", "Release", "monolith_pipeline.exe"))
    ap.add_argument("--whisper-model", default=os.path.join(REPO, "models", "ggml-tiny.bin"))
    ap.add_argument("--tessdata", default=r"C:\Program Files\Tesseract-OCR\tessdata")
    ap.add_argument("--no-batch", action="store_true",
                    help="Spawn a new process per request (measures cold start)")
    args = ap.parse_args()

    # Try both Release and Debug build locations
    binary = args.binary
    if not os.path.exists(binary):
        alt = binary.replace("Release", "Debug")
        if os.path.exists(alt):
            binary = alt
        else:
            print(f"ERROR: binary not found: {binary}", file=sys.stderr)
            sys.exit(1)

    extra_args = [
        "--whisper-model", args.whisper_model,
        "--tessdata", args.tessdata,
    ]

    out_dir = os.path.join(REPO, "results", "raw", "monolith")
    os.makedirs(out_dir, exist_ok=True)

    datasets = [
        ("datasets/text/samples.jsonl",         "text",  "input"),
        ("datasets/audio/audio_manifest.jsonl",  "audio", "input"),
        ("datasets/images/image_manifest.jsonl", "image", "input"),
    ]

    all_samples = []
    for rel, modality, key in datasets:
        for s in find_dataset(rel):
            sid = s.get("id", "?")
            inp = s.get(key, s.get("path", ""))
            all_samples.append((sid, modality, inp, s.get("label", "")))

    if not all_samples:
        print("ERROR: no samples found", file=sys.stderr)
        sys.exit(1)

    if args.no_batch:
        results = []
        for sid, modality, inp, label in all_samples:
            r = run_request(binary, extra_args, sid, modality, inp)
            if r:
                r["label"] = label
                results.append(r)
    else:
        print(f"Running {len(all_samples)} samples in batch mode...")
        results = run_batch(binary, extra_args, all_samples)

    for r in results:
        intent  = r.get("intent", "?")
        ms      = r.get("total_us", 0) / 1000
        label   = r.get("label", "")
        correct = "OK " if label == intent else "ERR"
        print(f"  {correct} {r.get('id','?')}  intent={intent}  {ms:.1f}ms")

    out_path = os.path.join(out_dir, "results.jsonl")
    with open(out_path, "w") as f:
        for r in results:
            f.write(json.dumps(r) + "\n")

    print(f"\nWrote {len(results)} results -> {out_path}")
    correct = sum(1 for r in results if r.get("label") == r.get("intent"))
    print(f"Accuracy: {correct}/{len(results)} = {correct/max(len(results),1)*100:.1f}%")


if __name__ == "__main__":
    main()
