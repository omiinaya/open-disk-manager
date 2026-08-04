#pragma once
// Shared helpers for the CLI (declared in main.cpp, used across commands.cpp).
#include <string>
#include <vector>
#include <algorithm>

namespace opm {
namespace cli {

inline bool wantsJson(const std::vector<std::string>& args) {
    return std::find(args.begin(), args.end(), "--json") != args.end();
}

// Strip the --json flag from an args vector (so sub-commands like fsync/backup
// info don't mis-read it as a positional argument).
inline std::vector<std::string> withoutFlag(const std::vector<std::string>& args,
                                            const std::string& flag) {
    std::vector<std::string> out;
    for (const auto& a : args)
        if (a != flag) out.push_back(a);
    return out;
}

} // namespace cli
} // namespace opm