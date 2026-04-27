#!/usr/bin/env bash
# Debug: send a request and print the full raw BSON response
set -euo pipefail

BUILD="/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/build"
DAG="/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/src/dandelion/dag"
PORT=8080
FOLDER="/tmp/dandelion_debug"

mkdir -p "$FOLDER"
rm -f /dev/shm/shm_* 2>/dev/null || true

PROCESS_WORKER_PATH="$BUILD/mmu_worker" \
"$BUILD/dandelion_server" \
    --bin-preload-path "$DAG/mono_preload.json" \
    --port "$PORT" \
    --folder-path "$FOLDER" \
    > "$FOLDER/server.log" 2>&1 &
DN_PID=$!

for i in $(seq 1 30); do
    grep -q -i "server start" "$FOLDER/server.log" 2>/dev/null && break
    sleep 0.3
done

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

# Register composition
dsl = open("/mnt/d/desktop_shortcut/Martin/1 polimi/dandelion/dandelion-multimodal-benchmark/src/dandelion/dag/monolithic.dc").read()
reg_body = _bson_doc([(0x02, 'composition', _bson_string(dsl))])
urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:8080/register/composition',
    data=reg_body, headers={'Content-Type': 'application/octet-stream'}), timeout=5)

# Send request
payload = json.dumps({"id":"t001","modality":"text","input":"I want to book a table for two"},
                     separators=(',',':')).encode()
item = _bson_doc([(0x02,'identifier',_bson_string('')),(0x10,'key',_bson_int32(0)),(0x05,'data',_bson_binary(payload))])
set_doc = _bson_doc([(0x02,'identifier',_bson_string('')),(0x04,'items',_bson_array([item]))])
body = _bson_doc([(0x02,'name',_bson_string('mono_pipeline')),(0x04,'sets',_bson_array([set_doc]))])

resp = urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:8080/hot/compute',
    data=body, headers={'Content-Type': 'application/octet-stream'}), timeout=10)
raw = resp.read()

print(f"Response: {len(raw)} bytes")
print(f"Full hex: {raw.hex()}")

# Manually parse sets array
def decode_doc(data, off=0):
    size = struct.unpack_from('<i', data, off)[0]
    end = off + size
    off += 4
    items = {}
    while off < end - 1:
        t = data[off]; off += 1
        ke = data.index(0, off); key = data[off:ke].decode(); off = ke + 1
        if t == 0x02:
            sl = struct.unpack_from('<i', data, off)[0]; off += 4
            val = data[off:off+sl-1].decode('utf-8','replace'); off += sl
        elif t in (0x03, 0x04):
            ds = struct.unpack_from('<i', data, off)[0]
            sub, _ = decode_doc(data, off); off += ds
            val = ([sub[str(i)] for i in range(len(sub))] if t==0x04 else sub)
        elif t == 0x05:
            bl = struct.unpack_from('<i', data, off)[0]; off += 5
            val = data[off:off+bl]; off += bl
        elif t == 0x10:
            val = struct.unpack_from('<i', data, off)[0]; off += 4
        elif t == 0x12:
            val = struct.unpack_from('<q', data, off)[0]; off += 8
        else:
            print(f"  Unknown type {t} at offset {off}")
            break
        items[key] = val
    return items, end

doc, _ = decode_doc(raw)
print(f"\nTop-level keys: {list(doc.keys())}")
sets = doc.get('sets', [])
print(f"Sets count: {len(sets)}")
for i, s in enumerate(sets):
    print(f"  Set {i}: identifier={s.get('identifier')!r}")
    items_list = s.get('items', [])
    print(f"    Items count: {len(items_list)}")
    for j, it in enumerate(items_list):
        d = it.get('data', b'')
        print(f"    Item {j}: identifier={it.get('identifier')!r} key={it.get('key')} data_len={len(d)} data_hex={d[:40].hex()!r}")
        if d:
            try: print(f"    data_str: {d.decode('utf-8','replace').rstrip(chr(0))!r}")
            except: pass
PYEOF

kill $DN_PID 2>/dev/null || true
