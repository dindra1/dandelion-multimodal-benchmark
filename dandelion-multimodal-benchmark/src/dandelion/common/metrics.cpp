#include "metrics.h"
#include "json_writer.h"
#include <cstdio>
#include <ctime>

int64_t now_ns() {
#if defined(_WIN32)
    // Windows: use QueryPerformanceCounter equivalent via timespec
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

void emit_metrics(const NodeMetrics& m) {
    JsonWriter w;
    w.begin_object();
    w.field("request_id",    m.request_id);
    w.field("system",        m.system);
    w.field("granularity",   m.granularity);
    w.field("node_name",     m.node_name);
    w.field("modality",      m.modality);
    w.field("cold_start",    m.cold_start);
    w.field("payload_bytes", m.payload_bytes);
    w.field("model_load_us", m.model_load_us);
    w.field("preprocess_us", m.preprocess_us);
    w.field("inference_us",  m.inference_us);
    w.field("serialize_us",  m.serialize_us);
    w.field("total_us",      m.total_us);
    w.field("peak_mem_kb",   m.peak_mem_kb);
    w.field("wall_clock_ns", m.wall_clock_ns);
    w.end_object();
    fprintf(stderr, "%s\n", w.str().c_str());
}
