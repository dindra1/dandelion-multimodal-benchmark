# Dandelion Multimodal Elasticity Benchmark

This repository benchmarks multimodal restaurant-style workloads across:

- Local monolithic C++ baseline
- Dandelion DAG-style execution
- AWS Lambda container baseline

## Current status

Phase 1: repository scaffold and toolchain.

## Project structure

- src/monolith: local C++ baseline
- src/dandelion: Dandelion DAG implementation
- src/lambda_baseline: AWS Lambda container baseline
- models: local model files, not committed
- datasets: local test inputs
- bench: benchmarking harness
- results: raw and processed results
- plots: generated figures
