#include <gtest/gtest.h>
#include "opm/json.hpp"
#include <sstream>

using namespace opm;

TEST(JsonTest, BasicObject) {
    std::ostringstream ss;
    JsonWriter w(ss);
    w.beginObject();
    w.field("name", "hello");
    w.field("count", uint64_t(42));
    w.field("ok", true);
    w.endObject();
    w.finish();
    EXPECT_EQ(ss.str(), "{\"name\":\"hello\",\"count\":42,\"ok\":true}\n");
}

TEST(JsonTest, ArrayOfObjects) {
    std::ostringstream ss;
    JsonWriter w(ss);
    w.beginObject();
    w.field("count", uint64_t(2));
    w.key("items");
    w.beginArray();
    w.beginObject(); w.field("id", int64_t(1)); w.endObject();
    w.beginObject(); w.field("id", int64_t(2)); w.endObject();
    w.endArray();
    w.endObject();
    w.finish();
    EXPECT_EQ(ss.str(), "{\"count\":2,\"items\":[{\"id\":1},{\"id\":2}]}\n");
}

TEST(JsonTest, StringEscaping) {
    std::ostringstream ss;
    JsonWriter w(ss);
    w.beginObject();
    w.field("path", "C:\\temp\\file.txt");
    w.field("quote", "say \"hi\"");
    w.field("newline", "line1\nline2");
    w.endObject();
    w.finish();
    EXPECT_EQ(ss.str(), "{\"path\":\"C:\\\\temp\\\\file.txt\",\"quote\":\"say \\\"hi\\\"\",\"newline\":\"line1\\nline2\"}\n");
}

TEST(JsonTest, ParseableByPython) {
    // Structural sanity: the emitted JSON must be a single object with balanced
    // braces (independently validated by the CLI E2E via python json.load).
    std::ostringstream ss;
    JsonWriter w(ss);
    w.beginObject();
    w.key("devices");
    w.beginArray();
    for (int i = 0; i < 3; i++) {
        w.beginObject();
        w.field("path", "/dev/sd" + std::string(1, char('a' + i)));
        w.field("size", uint64_t(1000 + i));
        w.field("ssd", i == 1);
        w.endObject();
    }
    w.endArray();
    w.endObject();
    w.finish();
    std::string out = ss.str();
    int depth = 0;
    bool in_string = false;
    for (size_t i = 0; i < out.size(); i++) {
        char c = out[i];
        if (c == '"' && (i == 0 || out[i-1] != '\\')) in_string = !in_string;
        if (in_string) continue;
        if (c == '{' || c == '[') depth++;
        if (c == '}' || c == ']') depth--;
    }
    EXPECT_EQ(depth, 0) << "balanced JSON structure";
}
