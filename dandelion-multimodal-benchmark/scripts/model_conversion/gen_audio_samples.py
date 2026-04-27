#!/usr/bin/env python3
"""
Generates synthetic WAV audio samples for the benchmark dataset.
Requires: pip install gTTS
Requires: ffmpeg on PATH
"""
import os
import sys
import pathlib
import subprocess

SAMPLES = [
    ("a001", "I would like to book a table for two people this Saturday evening"),
    ("a002", "Please cancel my reservation for next Monday"),
    ("a003", "I need to change my reservation from Friday to Saturday"),
    ("a004", "What vegetarian options do you have on the menu"),
    ("a005", "Book a table for four at seven thirty pm on Thursday"),
    ("a006", "Cancel all my upcoming reservations please"),
    ("a007", "Can I modify my booking to add two more people"),
    ("a008", "Do you have any gluten free dishes"),
    ("a009", "I want to reserve a table for a birthday dinner"),
    ("a010", "What time does the kitchen close on weekends"),
]

def main():
    try:
        from gtts import gTTS
    except ImportError:
        print("ERROR: pip install gTTS", file=sys.stderr)
        sys.exit(1)

    out_dir = pathlib.Path(__file__).parent.parent.parent / "datasets" / "audio"
    out_dir.mkdir(parents=True, exist_ok=True)

    for sid, text in SAMPLES:
        mp3_path = out_dir / f"{sid}.mp3"
        wav_path = out_dir / f"{sid}.wav"

        if wav_path.exists():
            print(f"Skipping {sid} (already exists)")
            continue

        print(f"Generating {sid}: {text[:50]}...")
        tts = gTTS(text)
        tts.save(str(mp3_path))

        result = subprocess.run(
            ["ffmpeg", "-i", str(mp3_path), "-ar", "16000", "-ac", "1",
             str(wav_path), "-y", "-loglevel", "quiet"],
            capture_output=True
        )
        if result.returncode != 0:
            print(f"WARNING: ffmpeg failed for {sid}. MP3 saved, WAV skipped.")
        else:
            mp3_path.unlink()  # remove MP3, keep WAV only
            print(f"  -> {wav_path}")

    print(f"\nDone. {len(SAMPLES)} samples in {out_dir}")

if __name__ == "__main__":
    main()
