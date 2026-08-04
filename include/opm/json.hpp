#pragma once

// ============================================================================
// Minimal JSON output helper for the CLI --json mode.
//
// Deliberately dependency-free (the project ships self-contained SHA-256, tar,
// RLE, etc. rather than pulling external libs that may not exist on the MinGW
// cross toolchain). Emits a single JSON object to an ostream:
//
//   JsonWriter w(std::cout);
//   w.beginObject();
//   w.key("devices"); w.beginArray();
//   w.beginObject(); w.key("path"); w.value("/dev/sda"); w.endObject();
//   w.endArray();
//   w.endObject();
//
// Produces compact, correctly-escaped JSON with a trailing newline.
// ============================================================================

#include <iostream>
#include <string>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace opm {

class JsonWriter {
public:
    explicit JsonWriter(std::ostream& out) : out_(out) {}

    void beginObject() {
        maybeSeparator();
        out_ << '{';
        stack_.push_back(Frame{State::Object, 0});
    }

    void endObject() {
        out_ << '}';
        stack_.pop_back();
    }

    void beginArray() {
        maybeSeparator();
        out_ << '[';
        stack_.push_back(Frame{State::Array, 0});
    }

    void endArray() {
        out_ << ']';
        stack_.pop_back();
    }

    // Emit an object key (only valid inside an object).
    void key(const std::string& k) {
        Frame& s = stack_.back();
        if (s.count > 0) out_ << ',';
        s.count++;
        out_ << '"' << escape(k) << "\":";
    }

    // Emit a bare string value (inside arrays/objects as a value).
    void value(const std::string& v) {
        maybeSeparator();
        out_ << '"' << escape(v) << '"';
    }

    void value(const char* v) { value(std::string(v)); }

    void value(int64_t v) { maybeSeparator(); out_ << v; }
    void value(uint64_t v) { maybeSeparator(); out_ << v; }
    void value(bool v) { maybeSeparator(); out_ << (v ? "true" : "false"); }
    void value(double v) { maybeSeparator(); out_ << v; }

    // key + value convenience (object context)
    void field(const std::string& k, const std::string& v) { key(k); value(v); }
    void field(const std::string& k, const char* v) { key(k); value(std::string(v)); }
    void field(const std::string& k, int64_t v) { key(k); value(v); }
    void field(const std::string& k, uint64_t v) { key(k); value(v); }
    void field(const std::string& k, bool v) { key(k); value(v); }

    void finish() { out_ << '\n'; }

private:
    enum class State { Object, Array };
    struct Frame { State kind; size_t count; };

    static std::string escape(const std::string& s) {
        std::string r;
        r.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"':  r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\b': r += "\\b";  break;
                case '\f': r += "\\f";  break;
                case '\n': r += "\\n";  break;
                case '\r': r += "\\r";  break;
                case '\t': r += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        r += buf;
                    } else {
                        r += c;
                    }
            }
        }
        return r;
    }

    // Add a comma separator between sibling values (arrays) or after keys is
    // handled by key(); here we only need it for bare values inside arrays.
    void maybeSeparator() {
        if (!stack_.empty() && stack_.back().kind == State::Array) {
            if (stack_.back().count > 0) out_ << ',';
            stack_.back().count++;
        }
    }

    std::ostream& out_;
    std::vector<Frame> stack_;
};

} // namespace opm
