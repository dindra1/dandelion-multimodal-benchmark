#pragma once
#include "metrics.h"
#include <string>

// Normalize raw text: lowercase, strip punctuation, collapse whitespace.
std::string normalize_text(const std::string& raw);

// Full text pipeline: normalize → classify. Fills timing fields in metrics.
std::string run_text_pipeline(const std::string& input, NodeMetrics& m);
