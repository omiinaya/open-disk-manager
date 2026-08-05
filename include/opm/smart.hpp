#pragma once

#include "types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace opm {

// Structured view of an NVMe SMART/Health Information log page (Log Page ID
// 0x02, 512 bytes, NVMe 1.0+ — every NVMe device must support it).
struct NvmeSmartInfo {
    // Byte 0: critical warning bitmask
    bool warn_spare = false;          // bit 0: available spare below threshold
    bool warn_temp = false;           // bit 1: temperature above threshold
    bool warn_reliability = false;    // bit 2: reliability degraded
    bool warn_read_only = false;      // bit 3: media placed in read-only mode
    bool warn_backup = false;         // bit 4: volatile memory backup failed
    uint8_t critical_warning_raw = 0;

    uint16_t temperature_kelvin = 0;  // bytes 1-2 (LE)
    uint8_t available_spare = 0;      // byte 3 (percent)
    uint8_t available_spare_threshold = 0;  // byte 4
    uint8_t percentage_used = 0;      // byte 5

    // 128-bit LE counters (bytes 32..127); we keep the low 64 bits which is
    // what practically matters for lifetime reporting.
    uint64_t data_units_read = 0;     // bytes 32-39
    uint64_t data_units_written = 0;  // bytes 40-47
    uint64_t host_reads = 0;          // bytes 48-55
    uint64_t host_writes = 0;         // bytes 56-63
    uint64_t controller_busy_min = 0; // bytes 64-71
    uint64_t power_cycles = 0;        // bytes 72-79
    uint64_t power_on_hours = 0;      // bytes 80-87
    uint64_t unsafe_shutdowns = 0;    // bytes 88-95
    uint64_t media_errors = 0;        // bytes 96-103
    uint64_t error_log_entries = 0;   // bytes 104-111
    uint32_t warn_temp_time_min = 0;  // bytes 112-115
    uint32_t crit_temp_time_min = 0;  // bytes 116-119

    int16_t temperature_celsius() const {
        if (temperature_kelvin == 0) return 0;
        return static_cast<int16_t>(temperature_kelvin - 273);
    }
};

// Parse a raw 512-byte NVMe SMART/Health log page into the structured view.
// The layout is fixed by the NVMe specification; out-of-range buffers are
// rejected (returns false).
bool parseNvmeSmartLog(const uint8_t* log, size_t len, NvmeSmartInfo& out);

// Human-readable summary lines for the CLI.
std::vector<std::string> nvmeSmartLines(const NvmeSmartInfo& info);

} // namespace opm
