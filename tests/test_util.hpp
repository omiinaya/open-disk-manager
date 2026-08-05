#pragma once
// Shared helpers for the OPM test suite.
#include <string>
#include <filesystem>

// Portable temp directory for test artifacts. On Linux this is /tmp; on
// Windows this resolves through %TEMP% (the drive-absolute form), avoiding the
// "/tmp maps to the current drive root" trap that broke native Windows CI.
inline std::string test_tmp_dir() {
    std::error_code ec;
    auto p = std::filesystem::temp_directory_path(ec);
    if (ec) return ".";
    std::filesystem::create_directories(p / "opm-tests", ec);
    return (p / "opm-tests").string();
}
