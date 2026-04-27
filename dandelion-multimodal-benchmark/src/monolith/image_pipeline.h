#pragma once
#include "metrics.h"
#include <string>

// Forward-declare Tesseract handle so callers don't need tesseract_capi.h.
struct TessBaseAPI;

// Full image pipeline: load image → Tesseract OCR → normalize → classify.
// tess must be an initialised TessBaseAPI* (created once at startup).
std::string run_image_pipeline(const std::string& img_path,
                                TessBaseAPI* tess,
                                NodeMetrics& m);
