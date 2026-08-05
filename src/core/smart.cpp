#include "opm/smart.hpp"
#include <cstring>
#include <sstream>

namespace opm {

namespace {

uint64_t le64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= uint64_t(p[i]) << (8 * i);
    return v;
}
uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t le16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

} // anonymous namespace

bool parseNvmeSmartLog(const uint8_t* log, size_t len, NvmeSmartInfo& out) {
    if (!log || len < 512) return false;
    std::memset(&out, 0, sizeof(out));

    out.critical_warning_raw = log[0];
    out.warn_spare = (log[0] & 0x01) != 0;
    out.warn_temp = (log[0] & 0x02) != 0;
    out.warn_reliability = (log[0] & 0x04) != 0;
    out.warn_read_only = (log[0] & 0x08) != 0;
    out.warn_backup = (log[0] & 0x10) != 0;

    out.temperature_kelvin = le16(log + 1);
    out.available_spare = log[3];
    out.available_spare_threshold = log[4];
    out.percentage_used = log[5];

    out.data_units_read = le64(log + 32);
    out.data_units_written = le64(log + 40);
    out.host_reads = le64(log + 48);
    out.host_writes = le64(log + 56);
    out.controller_busy_min = le64(log + 64);
    out.power_cycles = le64(log + 72);
    out.power_on_hours = le64(log + 80);
    out.unsafe_shutdowns = le64(log + 88);
    out.media_errors = le64(log + 96);
    out.error_log_entries = le64(log + 104);
    out.warn_temp_time_min = le32(log + 112);
    out.crit_temp_time_min = le32(log + 116);
    return true;
}

std::vector<std::string> nvmeSmartLines(const NvmeSmartInfo& info) {
    std::vector<std::string> lines;
    std::ostringstream os;

    os << "Critical warning: 0x" << std::hex
       << static_cast<int>(info.critical_warning_raw) << std::dec;
    if (info.critical_warning_raw == 0) os << " (none)";
    lines.push_back(os.str());
    os.str("");

    os << "Temperature: " << info.temperature_celsius() << " C"
       << " (" << info.temperature_kelvin << " K)";
    lines.push_back(os.str());
    os.str("");

    os << "Available spare: " << static_cast<int>(info.available_spare)
       << "% (threshold " << static_cast<int>(info.available_spare_threshold) << "%)";
    lines.push_back(os.str());
    os.str("");

    os << "Percentage used: " << static_cast<int>(info.percentage_used) << "%";
    lines.push_back(os.str());
    os.str("");

    os << "Data units read: " << info.data_units_read
       << " (x1000 x512B = " << (info.data_units_read * 512000ULL) << " bytes)";
    lines.push_back(os.str());
    os.str("");

    os << "Data units written: " << info.data_units_written
       << " (x1000 x512B = " << (info.data_units_written * 512000ULL) << " bytes)";
    lines.push_back(os.str());
    os.str("");

    os << "Host read commands: " << info.host_reads;
    lines.push_back(os.str());
    os.str("");

    os << "Host write commands: " << info.host_writes;
    lines.push_back(os.str());
    os.str("");

    os << "Controller busy time: " << info.controller_busy_min << " min";
    lines.push_back(os.str());
    os.str("");

    os << "Power cycles: " << info.power_cycles;
    lines.push_back(os.str());
    os.str("");

    os << "Power-on hours: " << info.power_on_hours;
    lines.push_back(os.str());
    os.str("");

    os << "Unsafe shutdowns: " << info.unsafe_shutdowns;
    lines.push_back(os.str());
    os.str("");

    os << "Media and data integrity errors: " << info.media_errors;
    lines.push_back(os.str());
    os.str("");

    os << "Error information log entries: " << info.error_log_entries;
    lines.push_back(os.str());
    os.str("");

    os << "Warning composite temperature time: " << info.warn_temp_time_min << " min";
    lines.push_back(os.str());
    os.str("");

    os << "Critical composite temperature time: " << info.crit_temp_time_min << " min";
    lines.push_back(os.str());

    return lines;
}

} // namespace opm
