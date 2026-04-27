#include "json_writer.h"
#include <cstring>

void JsonWriter::begin_object() { buf_ += '{'; first_ = true; }
void JsonWriter::end_object()   { buf_ += '}'; }

void JsonWriter::comma_if_needed() {
    if (!first_) buf_ += ',';
    first_ = false;
}

std::string JsonWriter::escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

void JsonWriter::field(const std::string& key, const std::string& val) {
    comma_if_needed();
    buf_ += '"'; buf_ += escape(key); buf_ += "\":\"";
    buf_ += escape(val); buf_ += '"';
}

void JsonWriter::field(const std::string& key, int64_t val) {
    comma_if_needed();
    buf_ += '"'; buf_ += escape(key); buf_ += "\":";
    buf_ += std::to_string(val);
}

void JsonWriter::field(const std::string& key, bool val) {
    comma_if_needed();
    buf_ += '"'; buf_ += escape(key); buf_ += "\":";
    buf_ += val ? "true" : "false";
}

std::string JsonWriter::str() const { return buf_; }
