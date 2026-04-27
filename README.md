# Dandelion Multimodal Elasticity Benchmark

> **Does DAG granularity matter for the agentic AI workloads taking over startup land?**
> This benchmark runs the same restaurant-booking intent pipeline at three levels of decomposition on the real [Dandelion](https://github.com/eth-easl/dandelion) serverless runtime and measures the cost of each function boundary.

---

## Why this exists

Nearly **50% of startups in the latest Y Combinator batch are building agentic AI products** ([PitchBook, 2024](https://pitchbook.com/news/articles/y-combinator-is-going-all-in-on-ai-agents-making-up-nearly-50-of-latest-batch)). A significant share of those are automating the kind of task that every restaurant, clinic, or service business faces every day: handling booking confirmations, cancellations, and modifications through a conversational interface.

These workloads have a distinctive traffic pattern. Requests do not arrive at a steady rate. They arrive in bursts — concentrated in the hour before lunch, before dinner, on Friday afternoons. Outside those windows, traffic drops to near zero. This burst-and-idle shape is precisely the scenario that serverless architectures were designed for.

The dominant serverless choice today is **AWS Lambda**. It works, but it has a non-trivial cost model: you pay for idle provisioned concurrency to avoid cold starts, and you pay per invocation once traffic arrives. For lightweight tasks — parsing a sentence and classifying an intent — a large fraction of the bill is platform overhead, not actual compute.

**Dandelion** (ETH Zürich EASL, [github.com/eth-easl/dandelion](https://github.com/eth-easl/dandelion)) takes a different approach. Instead of containerizing each function in a separate process or VM, it isolates functions using hardware MMU page-table switching. Context switches happen at the CPU level in microseconds rather than at the container level in milliseconds. The key question is: does this make a measurable difference for the small, bursty, intent-classification workloads that agentic startups are actually shipping?

This benchmark measures exactly that.

---

## What was built

A three-stage NLP pipeline that processes restaurant-related natural language requests and classifies their intent (`book_reservation`, `cancel_reservation`, `modify_reservation`, `ask_about_menu`, `unknown`). The pipeline is compiled as **bare-metal ELF binaries** using the DandelionSDK (`x86_64-none-elf`, clang, no libc, no OS syscalls) and run under real MMU isolation by the Dandelion server.

The same logical pipeline is deployed at **three levels of DAG granularity**:

| Variant | Nodes | What each node does |
|---------|-------|---------------------|
| **Fine** | 3 | `ingress` → `normalize` → `classify` |
| **Coarse** | 2 | `ingress` → `coarse_text` (normalize + classify merged) |
| **Monolithic** | 1 | `mono_all` (everything in one ELF) |

The input/output sets flow through the Dandelion server's memory broker at each boundary. Every crossing means: run the ELF under MMU isolation, collect output from the shared memory arena, resolve the next DAG dependency, map the next function's input. That round-trip is the overhead under measurement.

---

## Results

**30 text samples × 3 variants = 90 executions**, all on the same machine (WSL2 Ubuntu 22.04, Intel x86_64).

| Variant | Nodes | Avg latency | vs. monolithic |
|---------|-------|-------------|----------------|
| Monolithic | 1 | **5.7 ms** | baseline |
| Coarse | 2 | **12.7 ms** | +7.0 ms (+123%) |
| Fine | 3 | **15.8 ms** | +10.1 ms (+177%) |

Each additional DAG function boundary costs approximately **3–7 ms** in this setup. The actual compute (keyword matching on a short string) takes on the order of microseconds. Everything else is Dandelion's MMU machinery, BSON serialization, and HTTP round-trip through the server.

All 90 samples were classified correctly. The latency ordering is strict: fine > coarse > monolithic, with no overlap between distributions after the first warm-up invocation.

### What this means for agentic workloads

For a task like restaurant booking intent classification:

- The **monolithic variant at ~5.7 ms** is already competitive with fast Lambda invocations for warm functions, and does so without a per-invocation pricing model or idle concurrency charge.
- The **fine-grained variant at ~15.8 ms** shows that naive microservice decomposition — splitting the pipeline into small units for modularity — nearly triples the latency even when the underlying compute is identical.
- The boundary overhead is **proportional to the number of nodes, not the payload size**. This matters for lightweight agentic tasks where the inference is cheap: the platform overhead dominates, so granularity choices dominate cost.

The practical implication: if you are building a booking-automation agent that runs thousands of intent classifications per day concentrated in a few peak hours, a Dandelion-style platform with a monolithic or coarse-grained DAG can deliver lower latency than a fine-grained Lambda decomposition — while paying for compute only when requests actually arrive.

---

## Repository structure

```
dandelion-multimodal-benchmark/
├── src/dandelion/
│   ├── nodes/               # C source for all 10 compute ELFs
│   │   ├── ingress.c        # Routes request by modality (text/audio/image)
│   │   ├── normalize.c      # Text normalization (lowercase, strip punctuation)
│   │   ├── classify.c       # Intent classification by keyword matching
│   │   ├── coarse_text.c    # Merged normalize + classify (coarse variant)
│   │   ├── mono_all.c       # Full pipeline in one ELF (monolithic variant)
│   │   └── ...              # Audio ASR, image OCR, format_output stubs
│   └── dag/
│       ├── fine.dc          # 3-node DAG composition (dparser DSL)
│       ├── coarse.dc        # 2-node DAG composition
│       ├── monolithic.dc    # 1-node DAG composition
│       ├── fine_preload.json     # Function registry for fine variant
│       ├── coarse_preload.json   # Function registry for coarse variant
│       └── mono_preload.json     # Function registry for monolithic variant
├── datasets/text/
│   └── samples.jsonl        # 30 labeled restaurant NLP samples, 5 intent classes
├── scripts/
│   ├── run_dandelion_bench.py    # Benchmark harness (pure Python, no deps)
│   └── infra/               # WSL2 build scripts for server + ELFs
├── results/raw/
│   ├── dandelion_fine/results.jsonl
│   ├── dandelion_coarse/results.jsonl
│   └── dandelion_monolithic/results.jsonl
└── build/                   # (git-ignored) compiled binaries
    ├── dandelion_server
    ├── mmu_worker
    └── dandelion_elfs/
```

---

## How to run

**Requirements:** WSL2 Ubuntu 22.04, Rust 1.88.0, clang-14, cmake ≥ 3.23, python3.

The Dandelion server is Linux-only (uses `ptrace` and MMU syscalls). All commands below run inside WSL2.

### 1. Build the Dandelion server and worker

```bash
# Clone the ETH EASL runtime
git clone https://github.com/eth-easl/dandelion
cd dandelion

# Install Rust 1.88.0 (required — later versions trigger a compiler ICE on this codebase)
rustup toolchain install 1.88.0

# Build server and mmu_worker
cargo +1.88.0 build --release --features mmu,reqwest_io,timestamp \
    -p dandelion_server -p mmu_worker

cp target/release/dandelion_server /path/to/benchmark/build/
cp target/release/mmu_worker       /path/to/benchmark/build/
```

### 2. Build the DandelionSDK and compile ELFs

```bash
# Install DandelionSDK (requires clang-14, cmake ≥ 3.23)
git clone https://github.com/eth-easl/dandelionSDK
cd dandelionSDK
cmake -B build -DDANDELION_PLATFORM=mmu_linux -DCMAKE_C_COMPILER=clang
cmake --build build --target install

# Compile the 10 compute ELFs for this benchmark
cd /path/to/benchmark
cmake -B build_elfs src/dandelion \
    -DDANDELION_SDK_DIR=/path/to/dandelionSDK/install \
    -DDANDELION_PLATFORM=mmu_linux
cmake --build build_elfs
```

### 3. Run the benchmark

```bash
# Clean up any stale shared memory from a previous run
rm -f /dev/shm/shm_*

# Run all three variants (starts/stops the server once per variant)
python3 scripts/run_dandelion_bench.py \
    --repo-root "/mnt/d/path/to/dandelion-multimodal-benchmark"
```

Results are written to `results/raw/dandelion_{fine,coarse,monolithic}/results.jsonl`. Each line:
```json
{"id": "t001", "intent": "book_reservation", "modality": "text", "elapsed_ms": 15.96, "system": "dandelion_fine"}
```

---

## How Dandelion works (brief)

Dandelion isolates compute functions using **hardware MMU page-table switching** rather than containers or VMs. Each function is compiled as a bare-metal ELF (no OS, no libc, no syscalls). The server maps input data into a 32 MB memory arena, sets up a page table that makes only that arena accessible to the function, and runs it inside `mmu_worker`. If the function accesses out-of-bounds memory, the CPU raises a page fault and the worker is killed — hardware-enforced isolation at the cost of a page-table flush, not a container restart.

Communication between functions in a DAG goes through the server's BSON-encoded memory broker. At each node boundary, the server collects the output buffer, resolves the next DAG dependency, and maps the data as input to the next function. This is what produces the ~3–7 ms per-boundary overhead measured here.

The wire protocol is **BSON over HTTP** on every route (`/register/composition`, `/hot/compute`). The benchmark harness implements a minimal BSON encoder/decoder in pure Python (stdlib only) to avoid external dependencies.

---

## Dataset

30 restaurant NLP sentences across 5 intent classes, designed to be unambiguous for keyword-based classification:

| Class | Count | Example |
|-------|-------|---------|
| `book_reservation` | 9 | "I'd like to book a table for two this Friday at 7pm" |
| `cancel_reservation` | 6 | "Cancel my reservation for tomorrow night please" |
| `modify_reservation` | 6 | "Can I change my booking from 6pm to 8pm" |
| `ask_about_menu` | 6 | "Do you have vegetarian options on your menu" |
| `unknown` | 6 | Sentences outside all keyword categories |

The classification is intentionally simple (keyword matching after normalization) so that compute time is negligible and platform overhead dominates — making the DAG boundary cost clearly visible in the measurements.

---

## Limitations

- **Text modality only** in the Dandelion benchmark. Audio and image paths require Dandelion's two-phase `reqwest_io` mechanism (the function emits an HTTP request descriptor in Phase A and receives the response in Phase B), which adds a different kind of latency that is not comparable to the single-phase text path.
- **Single machine, no concurrency.** The benchmark sends requests sequentially. Real burst-traffic behavior (many concurrent requests at meal-time peaks) would reveal different bottlenecks (worker pool saturation, shared memory contention).
- **No AWS Lambda comparison yet.** The equivalent Lambda measurements are a planned next step.
- **Keyword classification, not a real model.** Using a neural intent classifier (e.g., all-MiniLM-L6-v2 via ONNX Runtime) would make the compute component non-negligible and shift where the crossover point is.

---

## References

- ETH EASL Dandelion runtime: https://github.com/eth-easl/dandelion
- ETH EASL DandelionSDK: https://github.com/eth-easl/dandelionSDK
- Y Combinator agentic AI batch (PitchBook, 2025): https://pitchbook.com/news/articles/y-combinator-is-going-all-in-on-ai-agents-making-up-nearly-50-of-latest-batch
