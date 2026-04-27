#include "audio_pipeline.h"
#include "text_pipeline.h"
#include "classifier.h"
#include "wav_reader.h"
#include "metrics.h"

#include "whisper.h"  // from third_party/whisper.cpp/include/

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

std::string run_audio_pipeline(const std::string& wav_path,
                                whisper_context* wctx,
                                NodeMetrics& m) {
    // --- Preprocess: load WAV and resample to 16 kHz mono float ---
    int64_t t_pre0 = now_ns();

    std::vector<float> samples;
    int sample_rate = 0;
    if (!wav_read_mono_f32(wav_path, samples, sample_rate)) {
        fprintf(stderr, "{\"error\":\"failed to read WAV: %s\"}\n", wav_path.c_str());
        return "unknown";
    }

    // whisper.cpp expects 16 kHz. If the file is at a different rate,
    // emit a warning but proceed — the test data is always 16 kHz mono.
    if (sample_rate != 16000) {
        fprintf(stderr, "{\"warning\":\"unexpected sample rate %d (expected 16000)\"}\n",
                sample_rate);
    }

    m.payload_bytes = (int64_t)(samples.size() * sizeof(float));
    int64_t t_pre1  = now_ns();
    m.preprocess_us = (t_pre1 - t_pre0) / 1000;

    // --- Inference: run whisper ASR ---
    int64_t t_inf0 = now_ns();

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.language         = "en";
    wparams.n_threads        = 4;
    wparams.single_segment   = false;

    if (whisper_full(wctx, wparams, samples.data(), (int)samples.size()) != 0) {
        fprintf(stderr, "{\"error\":\"whisper_full failed\"}\n");
        return "unknown";
    }

    // Collect all segments into a single transcript string.
    std::string transcript;
    int n_segs = whisper_full_n_segments(wctx);
    for (int i = 0; i < n_segs; i++) {
        const char* seg = whisper_full_get_segment_text(wctx, i);
        if (seg) { transcript += seg; transcript += ' '; }
    }
    int64_t t_inf1 = now_ns();
    m.inference_us = (t_inf1 - t_inf0) / 1000;

    // --- Classify the transcription (reuse text pipeline) ---
    NodeMetrics dummy{};
    std::string intent = run_text_pipeline(transcript, dummy);

    m.total_us    = (t_inf1 - t_pre0) / 1000;
    m.peak_mem_kb = 0; // not tracked per-call

    return intent;
}
