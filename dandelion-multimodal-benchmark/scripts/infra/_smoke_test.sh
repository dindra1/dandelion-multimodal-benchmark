#!/usr/bin/env bash
# Quick smoke test: start server with mono_all, register composition, send one request.
set -euo pipefail

REPO="/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark"
BUILD="$REPO/build"
DAG="$REPO/src/dandelion/dag"
SERVER="$BUILD/dandelion_server"
PORT=8080
FOLDER="/tmp/dandelion_smoke"

mkdir -p "$FOLDER"
rm -f "$FOLDER/server.log"

echo "=== Starting dandelion_server ==="
PROCESS_WORKER_PATH="$BUILD/mmu_worker" \
"$SERVER" \
    --bin-preload-path "$DAG/mono_preload.json" \
    --port "$PORT" \
    --folder-path "$FOLDER" \
    > "$FOLDER/server.log" 2>&1 &
DN_PID=$!
echo "  PID=$DN_PID"

# Wait for server to start (look for "Server start" in log)
for i in $(seq 1 30); do
    if grep -q -i "server start\|listening\|started" "$FOLDER/server.log" 2>/dev/null; then
        echo "  Server ready"
        break
    fi
    sleep 0.5
done

echo "=== Registering mono_pipeline composition ==="
python3 - <<'PYEOF'
import urllib.request, struct

def _cstring(s): return s.encode() + b'\x00'
def _bson_string(s):
    enc = s.encode() + b'\x00'
    return struct.pack('<i', len(enc)) + enc
def _bson_doc(fields):
    body = b''.join(bytes([t]) + _cstring(k) + v for t, k, v in fields) + b'\x00'
    return struct.pack('<i', 4+len(body)) + body

dsl = open("/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/src/dandelion/dag/monolithic.dc").read()
body = _bson_doc([(0x02, 'composition', _bson_string(dsl))])
req = urllib.request.Request('http://127.0.0.1:8080/register/composition', data=body,
    headers={'Content-Type': 'application/octet-stream'})
resp = urllib.request.urlopen(req, timeout=5)
print("  Registration response:", resp.read().decode())
PYEOF

echo "=== Sending test request ==="
python3 - <<'PYEOF'
import urllib.request, struct, json, time

def _cstring(s): return s.encode() + b'\x00'
def _bson_string(s):
    enc = s.encode() + b'\x00'
    return struct.pack('<i', len(enc)) + enc
def _bson_int32(v): return struct.pack('<i', v)
def _bson_binary(d): return struct.pack('<i', len(d)) + b'\x00' + d
def _bson_doc(fields):
    body = b''.join(bytes([t]) + _cstring(k) + v for t, k, v in fields) + b'\x00'
    return struct.pack('<i', 4+len(body)) + body
def _bson_array(docs):
    return _bson_doc([(0x03, str(i), d) for i, d in enumerate(docs)])

payload = json.dumps({"id":"smoke_01","modality":"text","input":"I want to book a table for two"},
                     separators=(',',':')).encode()
item_doc = _bson_doc([(0x02,'identifier',_bson_string('')),(0x10,'key',_bson_int32(0)),(0x05,'data',_bson_binary(payload))])
set_doc = _bson_doc([(0x02,'identifier',_bson_string('')),(0x04,'items',_bson_array([item_doc]))])
body = _bson_doc([(0x02,'name',_bson_string('mono_pipeline')),(0x04,'sets',_bson_array([set_doc]))])

t0 = time.perf_counter()
req = urllib.request.Request('http://127.0.0.1:8080/hot/compute', data=body,
    headers={'Content-Type': 'application/octet-stream'})
resp = urllib.request.urlopen(req, timeout=10)
raw = resp.read()
elapsed = (time.perf_counter() - t0)*1000
print(f"  Response ({elapsed:.1f}ms): {len(raw)} bytes raw")
print(f"  First bytes: {raw[:40].hex()}")

# Try to parse
def bson_decode(data, offset=0):
    size = struct.unpack_from('<i', data, offset)[0]
    end = offset + size; offset += 4; result = {}
    while offset < end - 1:
        t = data[offset]; offset += 1
        ke = data.index(0, offset); key = data[offset:ke].decode(); offset = ke+1
        if t == 0x02:
            sl = struct.unpack_from('<i', data, offset)[0]; offset += 4
            v = data[offset:offset+sl-1].decode('utf-8','replace'); offset += sl
        elif t in (0x03, 0x04):
            ds = struct.unpack_from('<i', data, offset)[0]
            v, _ = bson_decode(data, offset); offset += ds
            if t == 0x04: v = [v[str(i)] for i in range(len(v))]
        elif t == 0x05:
            bl = struct.unpack_from('<i', data, offset)[0]; offset += 5
            v = data[offset:offset+bl]; offset += bl
        elif t == 0x10:
            v = struct.unpack_from('<i', data, offset)[0]; offset += 4
        else: break
        result[key] = v
    return result, end

doc, _ = bson_decode(raw)
sets = doc.get('sets', [])
if sets:
    items = sets[0].get('items', [])
    if items:
        d = items[0].get('data', b'')
        print(f"  Result JSON: {d.decode('utf-8','replace').rstrip(chr(0))}")
PYEOF

echo "=== Stopping server ==="
kill $DN_PID 2>/dev/null || true
echo "Done. Server log at $FOLDER/server.log"
cat "$FOLDER/server.log" | head -20
