#!/usr/bin/env python3
"""
Exports a MiniLM sentence-transformer model to ONNX for use via the
ONNX Runtime C API. All three baselines load the same .onnx file.
"""
import argparse
import pathlib
import sys

def main():
    parser = argparse.ArgumentParser(description="Export intent classifier to ONNX")
    parser.add_argument("--model", default="sentence-transformers/all-MiniLM-L6-v2",
                        help="HuggingFace model ID")
    parser.add_argument("--output", default="models/intent_classifier.onnx",
                        help="Output .onnx file path")
    parser.add_argument("--seq-len", type=int, default=64,
                        help="Fixed sequence length for the export")
    args = parser.parse_args()

    try:
        import torch
        from transformers import AutoTokenizer, AutoModel
    except ImportError:
        print("ERROR: Install dependencies first:  pip install torch transformers", file=sys.stderr)
        sys.exit(1)

    out = pathlib.Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)

    print(f"Loading model: {args.model}")
    tokenizer = AutoTokenizer.from_pretrained(args.model)
    model = AutoModel.from_pretrained(args.model).eval()

    dummy = tokenizer(
        "book a table for two",
        return_tensors="pt",
        padding="max_length",
        max_length=args.seq_len,
        truncation=True,
    )

    print(f"Exporting to: {out}")
    with torch.no_grad():
        torch.onnx.export(
            model,
            (dummy["input_ids"], dummy["attention_mask"]),
            f=str(out),
            input_names=["input_ids", "attention_mask"],
            output_names=["last_hidden_state"],
            dynamic_axes={
                "input_ids":      {0: "batch", 1: "seq"},
                "attention_mask": {0: "batch", 1: "seq"},
            },
            opset_version=17,
        )

    size_kb = out.stat().st_size // 1024
    print(f"Success: {out}  ({size_kb} KB)")

    # Verify the exported model
    try:
        import onnx
        onnx.checker.check_model(str(out))
        print("ONNX model check passed.")
    except ImportError:
        print("Note: install 'onnx' package to validate the export.")

if __name__ == "__main__":
    main()
