#pragma once
#include <cstdint>
#include <string>

struct NodeMetrics {
    std::string request_id;
    std::string system;       // "monolith" | "dandelion" | "lambda"
    std::string granularity;  // "fine" | "coarse" | "monolithic"
    std::string node_name;    // e.g. "asr", "ocr", "classifier"
    std::string modality;     // "text" | "audio" | "image"
    bool        cold_start;
    int64_t     payload_bytes;
    int64_t     model_load_us;
    int64_t     preprocess_us;
    int64_t     inference_us;
    int64_t     serialize_us;
    int64_t     total_us;
    int64_t     peak_mem_kb;
    int64_t     wall_clock_ns;
};

// Writes one JSON line to stderr
void emit_metrics(const NodeMetrics& m);

// Returns monotonic time in nanoseconds
int64_t now_ns();
