# Dandelion Multimodal Elasticity Benchmark

## Abstract
*To be written after Phase 7.*

## What this project is not
- Not a production restaurant booking agent
- Not a claim that Dandelion universally beats AWS Lambda
- Not a full LLM agent
- Not a zero-copy implementation (unless explicitly measured and noted)

## Motivation
*To be filled in Phase 7.*

## Research Questions
1. At what request-rate and payload-size crossover does the Dandelion DAG overhead become negligible compared to cold-start latency?
2. How does DAG granularity (fine vs coarse vs monolithic) affect end-to-end latency and peak memory under bursty traffic?
3. How does Dandelion's scale-to-zero cost compare to AWS Lambda Provisioned Concurrency for flash-crowd workloads?

## Architecture
### Pipeline Overview
*To be filled in Phase 2–3.*

### Dandelion DAG
*To be filled in Phase 3.*

### AWS Lambda Baseline
*To be filled in Phase 4.*

### Local Monolith Baseline
*To be filled in Phase 2.*

## Experimental Setup
### Hardware
*To be filled in Phase 6.*

### Models Used
| Model | Task | Format | Size |
|-------|------|--------|------|
| whisper.cpp tiny | Speech-to-text (ASR) | ggml | ~39 MB |
| all-MiniLM-L6-v2 | Intent classification | ONNX | ~23 MB |
| Tesseract 5.x | OCR | system library | — |

### Dataset
| Modality | Samples | Description |
|----------|---------|-------------|
| Text | 30+ | Restaurant booking utterances, 5 intent classes |
| Audio | 10+ | 5–10 s WAV clips at 16 kHz mono |
| Image | 10+ | PNG screenshots with printed reservation text |

## Metrics Schema
Each node emits one JSON line to stderr:
```json
{
  "request_id": "t001",
  "system": "monolith",
  "granularity": "monolithic",
  "node_name": "classifier",
  "modality": "text",
  "cold_start": false,
  "payload_bytes": 128,
  "model_load_us": 0,
  "preprocess_us": 312,
  "inference_us": 4821,
  "serialize_us": 45,
  "total_us": 5178,
  "peak_mem_kb": 48320,
  "wall_clock_ns": 1714123456789000
}
```

## Results
*To be filled in Phase 7.*

### E1: Dummy Classifier (Pure Overhead)
### E2: Text-Only Path
### E3: Image / OCR
### E4: Audio / ASR
### E5: Burst Traffic (Idle → Spike)

## Discussion
*To be filled in Phase 7.*

## Limitations
*To be filled in Phase 7.*

## Future Work
*To be filled in Phase 7.*

## Reproducing the Experiments

### Prerequisites
- Linux (Ubuntu 22.04 recommended) or WSL2
- CMake ≥ 3.22
- GCC ≥ 11 or Clang ≥ 14
- Python ≥ 3.10
- Tesseract 5.x (`apt install libtesseract-dev`)
- ONNX Runtime 1.17.x (see CI workflow for download)
- ffmpeg (for audio sample generation)

### Build
```bash
git clone --recurse-submodules https://github.com/YOUR_USERNAME/dandelion-multimodal-benchmark
cd dandelion-multimodal-benchmark
cmake -B build -DONNXRUNTIME_DIR=/path/to/onnxruntime
cmake --build build --parallel
```

### Generate Models
```bash
bash scripts/model_conversion/download_models.sh
pip install torch transformers
python scripts/model_conversion/convert_to_onnx.py
sha256sum models/*.onnx models/*.bin >> models/MANIFEST.md
```

### Generate Audio Samples
```bash
pip install gTTS
python scripts/model_conversion/gen_audio_samples.py
```

## Citation
*To be added.*
