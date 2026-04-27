#include "image_pipeline.h"
#include "text_pipeline.h"
#include "classifier.h"
#include "metrics.h"

#include "tesseract_capi.h"  // from third_party/tesseract-import/

// stb_image: header-only PNG/JPEG/BMP loader (bundled in third_party)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "stb_image.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

std::string run_image_pipeline(const std::string& img_path,
                                TessBaseAPI* tess,
                                NodeMetrics& m) {
    // --- Preprocess: load image to grayscale uint8 ---
    int64_t t_pre0 = now_ns();

    int width = 0, height = 0, channels = 0;
    // Force 1 channel (grayscale) — Tesseract works best with grayscale.
    unsigned char* img_data = stbi_load(img_path.c_str(),
                                        &width, &height, &channels, 1);
    if (!img_data) {
        fprintf(stderr, "{\"error\":\"failed to load image: %s\"}\n",
                img_path.c_str());
        return "unknown";
    }

    m.payload_bytes = (int64_t)(width * height);
    int64_t t_pre1  = now_ns();
    m.preprocess_us = (t_pre1 - t_pre0) / 1000;

    // --- Inference: run Tesseract OCR ---
    int64_t t_inf0 = now_ns();

    TessBaseAPISetPageSegMode(tess, PSM_AUTO);
    // 1 byte per pixel, stride = width
    TessBaseAPISetImage(tess, img_data, width, height, 1, width);

    char* raw_text = TessBaseAPIGetUTF8Text(tess);
    std::string ocr_result = raw_text ? raw_text : "";
    if (raw_text) TessDeleteText(raw_text);

    TessBaseAPIClear(tess); // release internal page state

    int64_t t_inf1 = now_ns();
    m.inference_us = (t_inf1 - t_inf0) / 1000;

    stbi_image_free(img_data);

    // --- Classify the OCR text (reuse text pipeline) ---
    NodeMetrics dummy{};
    std::string intent = run_text_pipeline(ocr_result, dummy);

    m.total_us    = (t_inf1 - t_pre0) / 1000;
    m.peak_mem_kb = 0;

    return intent;
}
