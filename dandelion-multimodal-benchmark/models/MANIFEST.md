# Model Manifest

Checksums are generated after running `scripts/model_conversion/convert_to_onnx.py`
and `scripts/model_conversion/download_models.sh`.

Run to regenerate:
```bash
sha256sum models/*.onnx models/*.bin > models/MANIFEST.md
```

| File | SHA-256 |
|------|---------|
| intent_classifier.onnx | *pending — run convert_to_onnx.py* |
| ggml-tiny.bin | *pending — run download_models.sh* |
