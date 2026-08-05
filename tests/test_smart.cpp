#include <gtest/gtest.h>
#include "opm/smart.hpp"
#include <cstring>
#include <vector>

using namespace opm;

namespace {

void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = v & 0xFF;
    b[off + 1] = (v >> 8) & 0xFF;
}
void put64(std::vector<uint8_t>& b, size_t off, uint64_t v) {
    for (int i = 0; i < 8; i++) b[off + i] = (v >> (8 * i)) & 0xFF;
}

std::vector<uint8_t> sampleLog() {
    std::vector<uint8_t> b(512, 0);
    b[0] = 0x01;                    // critical warning: spare low
    put16(b, 1, 300);               // 300 K = 27 C
    b[3] = 95;                      // available spare 95%
    b[4] = 10;                      // threshold 10%
    b[5] = 12;                      // percentage used 12%
    put64(b, 32, 123456789);        // data units read
    put64(b, 40, 987654321);        // data units written
    put64(b, 48, 1000);             // host reads
    put64(b, 56, 2000);             // host writes
    put64(b, 64, 60);               // controller busy 60 min
    put64(b, 72, 42);               // power cycles
    put64(b, 80, 8760);             // power on hours
    put64(b, 88, 7);                // unsafe shutdowns
    put64(b, 96, 0);                // media errors
    put64(b, 104, 3);               // error log entries
    // bytes 112+ left zero: warning/critical temp time
    return b;
}

} // namespace

TEST(NvmeSmartTest, ParseFullLog) {
    auto b = sampleLog();
    NvmeSmartInfo info;
    ASSERT_TRUE(parseNvmeSmartLog(b.data(), b.size(), info));

    EXPECT_TRUE(info.warn_spare);
    EXPECT_FALSE(info.warn_temp);
    EXPECT_FALSE(info.warn_reliability);
    EXPECT_FALSE(info.warn_read_only);
    EXPECT_FALSE(info.warn_backup);
    EXPECT_EQ(info.critical_warning_raw, 0x01);

    EXPECT_EQ(info.temperature_kelvin, 300);
    EXPECT_EQ(info.temperature_celsius(), 27);
    EXPECT_EQ(info.available_spare, 95);
    EXPECT_EQ(info.available_spare_threshold, 10);
    EXPECT_EQ(info.percentage_used, 12);

    EXPECT_EQ(info.data_units_read, 123456789ULL);
    EXPECT_EQ(info.data_units_written, 987654321ULL);
    EXPECT_EQ(info.host_reads, 1000ULL);
    EXPECT_EQ(info.host_writes, 2000ULL);
    EXPECT_EQ(info.controller_busy_min, 60ULL);
    EXPECT_EQ(info.power_cycles, 42ULL);
    EXPECT_EQ(info.power_on_hours, 8760ULL);
    EXPECT_EQ(info.unsafe_shutdowns, 7ULL);
    EXPECT_EQ(info.media_errors, 0ULL);
    EXPECT_EQ(info.error_log_entries, 3ULL);
    EXPECT_EQ(info.warn_temp_time_min, 0u);
    EXPECT_EQ(info.crit_temp_time_min, 0u);
}

TEST(NvmeSmartTest, AllWarningsAndCounters) {
    std::vector<uint8_t> b = sampleLog();
    b[0] = 0x1F;  // all five warning bits
    put64(b, 96, 999);   // media errors
    put64(b, 104, 1);    // error log entries
    NvmeSmartInfo info;
    ASSERT_TRUE(parseNvmeSmartLog(b.data(), b.size(), info));

    EXPECT_TRUE(info.warn_spare);
    EXPECT_TRUE(info.warn_temp);
    EXPECT_TRUE(info.warn_reliability);
    EXPECT_TRUE(info.warn_read_only);
    EXPECT_TRUE(info.warn_backup);
    EXPECT_EQ(info.critical_warning_raw, 0x1F);
    EXPECT_EQ(info.media_errors, 999ULL);
    EXPECT_EQ(info.error_log_entries, 1ULL);
}

TEST(NvmeSmartTest, RejectsShortBuffer) {
    std::vector<uint8_t> b(511, 0);
    NvmeSmartInfo info;
    EXPECT_FALSE(parseNvmeSmartLog(b.data(), b.size(), info));
    EXPECT_FALSE(parseNvmeSmartLog(nullptr, 512, info));
}

TEST(NvmeSmartTest, ZeroTemperatureMeansNoSensor) {
    std::vector<uint8_t> b = sampleLog();
    put16(b, 1, 0);
    NvmeSmartInfo info;
    ASSERT_TRUE(parseNvmeSmartLog(b.data(), b.size(), info));
    EXPECT_EQ(info.temperature_celsius(), 0);
}

TEST(NvmeSmartTest, RenderLines) {
    auto b = sampleLog();
    NvmeSmartInfo info;
    ASSERT_TRUE(parseNvmeSmartLog(b.data(), b.size(), info));
    auto lines = nvmeSmartLines(info);
    ASSERT_GT(lines.size(), 5u);
    // spot-check the key lines
    bool found_temp = false, found_used = false, found_poh = false;
    for (const auto& l : lines) {
        if (l.find("Temperature: 27 C") != std::string::npos) found_temp = true;
        if (l.find("Percentage used: 12%") != std::string::npos) found_used = true;
        if (l.find("Power-on hours: 8760") != std::string::npos) found_poh = true;
    }
    EXPECT_TRUE(found_temp);
    EXPECT_TRUE(found_used);
    EXPECT_TRUE(found_poh);
}
