#!/usr/bin/env python3
"""
run_dandelion_bench.py — benchmark all three Dandelion DAG variants (text modality).

Usage (inside WSL2):
    python3 scripts/run_dandelion_bench.py --repo-root /mnt/d/...

The script:
1. Starts dandelion_server with all functions preloaded
2. Registers the 3 compositions via /register/composition
3. Sends all 30 text samples through each composition
4. Writes results/raw/dandelion_{fine,coarse,monolithic}/results.jsonl
"""

import argparse
import json
import os
import signal
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Minimal pure-Python BSON encoder/decoder (no external deps)
# ---------------------------------------------------------------------------

def _cstring(s: str) -> bytes:
    return s.encode('utf-8') + b'\x00'

def _bson_string(s: str) -> bytes:
    enc = s.encode('utf-8') + b'\x00'
    return struct.pack('<i', len(enc)) + enc

def _bson_binary(data: bytes) -> bytes:
    return struct.pack('<i', len(data)) + b'\x00' + data

def _bson_int32(v: int) -> bytes:
    return struct.pack('<i', v)

def _bson_doc(fields: list) -> bytes:
    """fields: list of (type_byte, key_str, value_bytes)"""
    body = b''
    for (t, k, v) in fields:
        body += bytes([t]) + _cstring(k) + v
    body += b'\x00'
    return struct.pack('<i', 4 + len(body)) + body

def _bson_array(docs: list) -> bytes:
    """docs: list of complete BSON document bytes (type 0x03 items)"""
    fields = [(0x03, str(i), d) for i, d in enumerate(docs)]
    return _bson_doc(fields)

def bson_encode_request(composition_name: str, payload: bytes) -> bytes:
    """Build a DandelionRequest BSON: {name, sets:[{identifier:'', items:[{identifier:'', key:0, data:payload}]}]}"""
    # key is u32 in the Rust struct but BSON serializes it as int64 (0x12)
    item_doc = _bson_doc([
        (0x02, 'identifier', _bson_string('')),
        (0x12, 'key',        struct.pack('<q', 0)),
        (0x05, 'data',       _bson_binary(payload)),
    ])
    items_arr = _bson_array([item_doc])
    set_doc = _bson_doc([
        (0x02, 'identifier', _bson_string('')),
        (0x04, 'items',      items_arr),
    ])
    sets_arr = _bson_array([set_doc])
    return _bson_doc([
        (0x02, 'name', _bson_string(composition_name)),
        (0x04, 'sets', sets_arr),
    ])

def bson_encode_register_composition(dsl_text: str) -> bytes:
    """Build a RegisterChain BSON: {composition: dsl_text}"""
    return _bson_doc([
        (0x02, 'composition', _bson_string(dsl_text)),
    ])

def bson_decode(data: bytes, offset: int = 0):
    """Decode a BSON document; returns (dict, end_offset)."""
    size = struct.unpack_from('<i', data, offset)[0]
    end = offset + size
    offset += 4
    result = {}
    while offset < end - 1:
        t = data[offset]; offset += 1
        key_end = data.index(0, offset)
        key = data[offset:key_end].decode('utf-8', errors='replace')
        offset = key_end + 1
        if t == 0x02:   # string
            slen = struct.unpack_from('<i', data, offset)[0]; offset += 4
            value = data[offset:offset + slen - 1].decode('utf-8', errors='replace')
            offset += slen
        elif t == 0x03:  # embedded document
            dsz = struct.unpack_from('<i', data, offset)[0]
            value, _ = bson_decode(data, offset); offset += dsz
        elif t == 0x04:  # array
            asz = struct.unpack_from('<i', data, offset)[0]
            arr_doc, _ = bson_decode(data, offset); offset += asz
            value = [arr_doc[str(i)] for i in range(len(arr_doc))]
        elif t == 0x05:  # binary
            blen = struct.unpack_from('<i', data, offset)[0]; offset += 4
            offset += 1  # subtype
            value = data[offset:offset + blen]; offset += blen
        elif t == 0x10:  # int32
            value = struct.unpack_from('<i', data, offset)[0]; offset += 4
        elif t == 0x12:  # int64
            value = struct.unpack_from('<q', data, offset)[0]; offset += 8
        else:
            break
        result[key] = value
    return result, end

def extract_first_item_data(response_bytes: bytes) -> bytes | None:
    """Extract data bytes from the first item of the first set in a DandelionDeserializeResponse."""
    try:
        doc, _ = bson_decode(response_bytes)
        sets = doc.get('sets', [])
        if not sets:
            return None
        items = sets[0].get('items', [])
        if not items:
            return None
        return items[0].get('data')
    except Exception:
        return None

# ---------------------------------------------------------------------------
# Server lifecycle
# ---------------------------------------------------------------------------

def start_server(server_bin: str, preload_path: str, port: int, folder: str, worker_bin: str) -> subprocess.Popen:
    os.makedirs(folder, exist_ok=True)
    cmd = [
        server_bin,
        '--bin-preload-path', preload_path,
        '--port', str(port),
        '--folder-path', folder,
    ]
    env = os.environ.copy()
    env['PROCESS_WORKER_PATH'] = worker_bin
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=env,
    )
    # Wait for "Server start" line
    for line in proc.stdout:
        if 'Server start' in line or 'server start' in line.lower():
            break
        if proc.poll() is not None:
            raise RuntimeError(f"dandelion_server exited early: {line}")
    return proc

def wait_for_server(port: int, timeout: float = 10.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            urllib.request.urlopen(f'http://127.0.0.1:{port}/', timeout=1)
            return
        except Exception:
            time.sleep(0.2)

def register_composition(port: int, dsl_path: str):
    dsl = Path(dsl_path).read_text()
    body = bson_encode_register_composition(dsl)
    req = urllib.request.Request(
        f'http://127.0.0.1:{port}/register/composition',
        data=body,
        headers={'Content-Type': 'application/octet-stream'},
    )
    resp = urllib.request.urlopen(req, timeout=10)
    return resp.read()

# ---------------------------------------------------------------------------
# Benchmark runner
# ---------------------------------------------------------------------------

def run_variant(port: int, composition_name: str, dataset_path: str, out_dir: str, variant: str):
    os.makedirs(out_dir, exist_ok=True)
    url = f'http://127.0.0.1:{port}/hot/compute'
    results = []

    try:
        lines = Path(dataset_path).read_text().splitlines()
    except FileNotFoundError:
        print(f'  [warn] {dataset_path} not found', file=sys.stderr)
        return

    for line in lines:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        sample = json.loads(line)
        sid = sample.get('id', '?')
        payload = json.dumps({'id': sid, 'modality': 'text', 'input': sample.get('input', '')},
                             separators=(',', ':')).encode()

        body = bson_encode_request(composition_name, payload)
        t0 = time.perf_counter()
        try:
            req = urllib.request.Request(url, data=body,
                                         headers={'Content-Type': 'application/octet-stream'})
            resp = urllib.request.urlopen(req, timeout=30)
            raw = resp.read()
            elapsed_ms = (time.perf_counter() - t0) * 1000
            item_data = extract_first_item_data(raw)
            if item_data:
                result = json.loads(item_data.decode('utf-8', errors='replace').rstrip('\x00'))
            else:
                result = {'id': sid, 'intent': 'unknown', 'modality': 'text'}
            result['elapsed_ms'] = round(elapsed_ms, 2)
            result['system'] = f'dandelion_{variant}'
            results.append(result)
            print(f'  {sid}  {result.get("intent","?")}  {elapsed_ms:.1f}ms')
        except Exception as e:
            print(f'  ERROR {sid}: {e}', file=sys.stderr)

    out_file = os.path.join(out_dir, 'results.jsonl')
    with open(out_file, 'w') as f:
        for r in results:
            f.write(json.dumps(r) + '\n')
    print(f'  Wrote {len(results)} results to {out_file}')

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo-root', default=None)
    ap.add_argument('--port', type=int, default=8080)
    args = ap.parse_args()

    if args.repo_root:
        repo = Path(args.repo_root)
    else:
        repo = Path(__file__).parent.parent

    build = repo / 'build'
    dag_dir = repo / 'src' / 'dandelion' / 'dag'
    results_dir = repo / 'results' / 'raw'
    text_dataset = repo / 'datasets' / 'text' / 'samples.jsonl'
    server_bin = str(build / 'dandelion_server')
    folder = '/tmp/dandelion_bench_server'

    VARIANTS = [
        {
            'name':        'fine',
            'preload':     str(dag_dir / 'fine_preload.json'),
            'dc':          str(dag_dir / 'fine.dc'),
            'composition': 'fine_text',
            'out_dir':     str(results_dir / 'dandelion_fine'),
        },
        {
            'name':        'coarse',
            'preload':     str(dag_dir / 'coarse_preload.json'),
            'dc':          str(dag_dir / 'coarse.dc'),
            'composition': 'coarse_text_pipeline',
            'out_dir':     str(results_dir / 'dandelion_coarse'),
        },
        {
            'name':        'monolithic',
            'preload':     str(dag_dir / 'mono_preload.json'),
            'dc':          str(dag_dir / 'monolithic.dc'),
            'composition': 'mono_pipeline',
            'out_dir':     str(results_dir / 'dandelion_monolithic'),
        },
    ]

    for v in VARIANTS:
        print(f'\n=== Variant: {v["name"]} ===')
        proc = None
        try:
            print(f'  Starting server (preload: {os.path.basename(v["preload"])})...')
            proc = start_server(server_bin, v['preload'], args.port, folder,
                                str(build / 'mmu_worker'))
            wait_for_server(args.port, timeout=15.0)
            print(f'  Server ready on :{args.port}')

            print(f'  Registering composition from {os.path.basename(v["dc"])}...')
            register_composition(args.port, v['dc'])
            print(f'  Composition "{v["composition"]}" registered')

            run_variant(args.port, v['composition'], str(text_dataset),
                        v['out_dir'], v['name'])
        except Exception as e:
            print(f'  FAILED: {e}', file=sys.stderr)
        finally:
            if proc and proc.poll() is None:
                proc.send_signal(signal.SIGTERM)
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
            time.sleep(1)

    print('\n=== ALL VARIANTS COMPLETE ===')
    print(f'Results in: {results_dir}')

if __name__ == '__main__':
    main()
