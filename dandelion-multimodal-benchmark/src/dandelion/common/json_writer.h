#pragma once
#include <string>
#include <cstdint>

// Minimal JSON object serializer — no external dependencies.
class JsonWriter {
public:
    void begin_object();
    void end_object();
    void field(const std::string& key, const std::string& val);
    void field(const std::string& key, int64_t val);
    void field(const std::string& key, bool val);
    std::string str() const;

private:
    std::string buf_;
    bool first_ = true;

    void comma_if_needed();
    static std::string escape(const std::string& s);
};
