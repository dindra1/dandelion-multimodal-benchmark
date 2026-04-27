#pragma once
#include <string>

// Five intent classes used across all pipelines and benchmark suites.
static constexpr const char* INTENTS[] = {
    "book_reservation",
    "cancel_reservation",
    "modify_reservation",
    "ask_about_menu",
    "unknown"
};
static constexpr int N_INTENTS = 5;

// Keyword-based intent classifier. Fast (<1 ms), no external deps.
// Used as-is for E1/E2 suites; replaced by ONNX model for E3-E5.
std::string classify_intent(const std::string& normalized_text);
