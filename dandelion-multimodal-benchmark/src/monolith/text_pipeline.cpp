#include "text_pipeline.h"
#include "classifier.h"
#include "metrics.h"
#include <algorithm>
#include <cctype>
#include <sstream>

std::string normalize_text(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    bool prev_space = true;
    for (unsigned char c : raw) {
        if (std::isalpha(c)) {
            out += (char)std::tolower(c);
            prev_space = false;
        } else if (std::isspace(c) || std::ispunct(c)) {
            if (!prev_space) { out += ' '; prev_space = true; }
        }
    }
    // Trim trailing space
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string run_text_pipeline(const std::string& input, NodeMetrics& m) {
    m.payload_bytes = (int64_t)input.size();

    int64_t t0 = now_ns();
    std::string normalized = normalize_text(input);
    int64_t t1 = now_ns();

    std::string intent = classify_intent(normalized);
    int64_t t2 = now_ns();

    m.preprocess_us = (t1 - t0) / 1000;
    m.inference_us  = (t2 - t1) / 1000;
    m.total_us      = (t2 - t0) / 1000;
    m.peak_mem_kb   = 0; // keyword classifier uses negligible heap

    return intent;
}
