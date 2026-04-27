#include "text_pipeline.h"
#include "audio_pipeline.h"
#include "image_pipeline.h"
#include "metrics.h"
#include "tesseract_capi.h"
#include "whisper.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

// Silence whisper.cpp's verbose init logs so stderr stays clean JSON lines.
static void whisper_log_suppress(enum ggml_log_level, const char*, void*) {}


// ---------------------------------------------------------------------------
// Minimal argument parser
// ---------------------------------------------------------------------------
static std::string get_arg(int argc, char* argv[], const char* flag,
                             const std::string& def = "") {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return argv[i + 1];
    return def;
}
static bool has_flag(int argc, char* argv[], const char* flag) {
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], flag) == 0) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Model initialisation helpers
// ---------------------------------------------------------------------------
static whisper_context* init_whisper(const std::string& model_path,
                                      int64_t& load_us) {
    int64_t t0 = now_ns();
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(),
                                                               cparams);
    load_us = (now_ns() - t0) / 1000;
    return ctx;
}

static TessBaseAPI* init_tesseract(const std::string& tessdata_dir,
                                    int64_t& load_us) {
    int64_t t0  = now_ns();
    TessBaseAPI* api = TessBaseAPICreate();
    const char* data = tessdata_dir.empty() ? nullptr : tessdata_dir.c_str();
    if (TessBaseAPIInit3(api, data, "eng") != 0) {
        fprintf(stderr, "{\"error\":\"Tesseract init failed — check tessdata path\"}\n");
        TessBaseAPIDelete(api);
        return nullptr;
    }
    load_us = (now_ns() - t0) / 1000;
    return api;
}

// ---------------------------------------------------------------------------
// Process one request
// ---------------------------------------------------------------------------
static void process_request(const std::string& request_id,
                              const std::string& modality,
                              const std::string& input,
                              whisper_context* wctx,
                              TessBaseAPI* tess,
                              int64_t whisper_load_us,
                              int64_t tess_load_us) {
    NodeMetrics m{};
    m.request_id  = request_id;
    m.system      = "monolith";
    m.granularity = "monolithic";
    m.modality    = modality;
    m.cold_start  = false;
    m.wall_clock_ns = now_ns();

    std::string intent;

    if (modality == "text") {
        m.node_name     = "text_pipeline";
        m.model_load_us = 0; // keyword classifier has no model
        intent = run_text_pipeline(input, m);

    } else if (modality == "audio") {
        m.node_name     = "audio_pipeline";
        m.model_load_us = whisper_load_us;
        if (!wctx) {
            fprintf(stderr, "{\"error\":\"whisper context not initialised\"}\n");
            return;
        }
        intent = run_audio_pipeline(input, wctx, m);

    } else if (modality == "image") {
        m.node_name     = "image_pipeline";
        m.model_load_us = tess_load_us;
        if (!tess) {
            fprintf(stderr, "{\"error\":\"tesseract not initialised\"}\n");
            return;
        }
        intent = run_image_pipeline(input, tess, m);

    } else {
        fprintf(stderr, "{\"error\":\"unknown modality: %s\"}\n", modality.c_str());
        return;
    }

    // Emit metrics line to stderr (collected by harness)
    emit_metrics(m);

    // Emit result to stdout
    printf("{\"id\":\"%s\",\"modality\":\"%s\",\"intent\":\"%s\",\"total_us\":%lld}\n",
           request_id.c_str(), modality.c_str(), intent.c_str(),
           (long long)m.total_us);
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// Batch mode: read JSON lines from stdin
// Each line: {"id":"r001","modality":"text","input":"..."}
// ---------------------------------------------------------------------------
static void run_batch(whisper_context* wctx, TessBaseAPI* tess,
                       int64_t whisper_load_us, int64_t tess_load_us) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Minimal JSON field extractor (no library dependency).
        auto extract = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            auto pos = line.find(needle);
            if (pos == std::string::npos) return "";
            pos += needle.size();
            auto end = line.find('"', pos);
            return end == std::string::npos ? "" : line.substr(pos, end - pos);
        };

        std::string id       = extract("id");
        std::string modality = extract("modality");
        std::string input    = extract("input");

        if (id.empty() || modality.empty() || input.empty()) {
            fprintf(stderr, "{\"warning\":\"skipping malformed line: %s\"}\n",
                    line.c_str());
            continue;
        }
        process_request(id, modality, input, wctx, tess,
                        whisper_load_us, tess_load_us);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Default model paths (override with flags)
    std::string whisper_model = get_arg(argc, argv, "--whisper-model",
                                         "models/ggml-tiny.bin");
    std::string tessdata_dir  = get_arg(argc, argv, "--tessdata",
                                         "C:\\Program Files\\Tesseract-OCR\\tessdata");
    bool batch_mode = has_flag(argc, argv, "--batch");

    // Single-request mode args
    std::string modality   = get_arg(argc, argv, "--modality");
    std::string input      = get_arg(argc, argv, "--input");
    std::string request_id = get_arg(argc, argv, "--request-id", "r000");

    // Suppress whisper/ggml verbose logs — our JSON metrics go to stderr too.
    whisper_log_set(whisper_log_suppress, nullptr);

    // --- Initialise models (cold start) ---
    int64_t whisper_load_us = 0, tess_load_us = 0;

    whisper_context* wctx = init_whisper(whisper_model, whisper_load_us);
    if (!wctx) {
        fprintf(stderr, "{\"error\":\"failed to load whisper model: %s\"}\n",
                whisper_model.c_str());
        // Continue — audio requests will fail gracefully, text/image still work.
    }

    TessBaseAPI* tess = init_tesseract(tessdata_dir, tess_load_us);
    // tess may be null — image requests will fail gracefully.

    // --- Run ---
    if (batch_mode) {
        run_batch(wctx, tess, whisper_load_us, tess_load_us);
    } else {
        if (modality.empty() || input.empty()) {
            fprintf(stderr,
                "Usage: monolith_pipeline --modality <text|audio|image>"
                " --input <text|wav_path|img_path>"
                " [--request-id <id>]"
                " [--whisper-model <path>]"
                " [--tessdata <dir>]"
                " [--batch]\n");
            if (wctx) whisper_free(wctx);
            if (tess) { TessBaseAPIEnd(tess); TessBaseAPIDelete(tess); }
            return 1;
        }
        process_request(request_id, modality, input, wctx, tess,
                        whisper_load_us, tess_load_us);
    }

    if (wctx) whisper_free(wctx);
    if (tess) { TessBaseAPIEnd(tess); TessBaseAPIDelete(tess); }
    return 0;
}
