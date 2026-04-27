#pragma once
#include "metrics.h"
#include <string>

// Forward-declare whisper_context so callers don't need whisper.h.
struct whisper_context;

// Full audio pipeline: load WAV → whisper ASR → normalize → classify.
// wctx must be a live whisper_context* loaded at startup (model load is
// measured separately in main via model_load_us).
std::string run_audio_pipeline(const std::string& wav_path,
                                whisper_context* wctx,
                                NodeMetrics& m);
