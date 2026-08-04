#include "opm/security.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace opm {

namespace {

namespace fs = std::filesystem;

// Run a command capturing stdout+stderr. Returns the exit status (or -1 on
// spawn failure).
int runCommand(const std::string& cmd, std::string& output) {
    std::string full = cmd + " 2>&1";
    std::string out;
    FILE* p = ::popen(full.c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), p)) {
        out += buf;
    }
    int rc = ::pclose(p);
    output = out;
    return rc;
}

std::string randomSalt(size_t len) {
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, sizeof(alphabet) - 2);
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; i++) s += alphabet[dist(gen)];
    return s;
}

// Generate a SHA-512 crypt hash via `openssl passwd -6 -stdin`.
bool generateHash(const std::string& password, const std::string& salt,
                  std::string& hash_out) {
    // Escape single quotes for the shell pipeline.
    std::string esc;
    esc.reserve(password.size() + 8);
    for (char c : password) {
        if (c == '\'') esc += "'\\''";
        else esc += c;
    }
    std::string full = "printf '%s\\n' '" + esc + "' | openssl passwd -6 -salt " +
                       salt + " -stdin 2>/dev/null";
    std::string out;
    int rc = runCommand(full, out);
    if (rc != 0) return false;
    // Trim trailing newline/whitespace.
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    if (out.rfind("$6$", 0) != 0) return false;
    hash_out = out;
    return true;
}

} // anonymous namespace

bool findTool(const std::string& name, std::string& path) {
    const std::vector<std::string> candidates = {
        "/usr/bin/" + name, "/usr/sbin/" + name, "/bin/" + name, "/sbin/" + name,
        "/usr/local/bin/" + name, "/usr/local/sbin/" + name,
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            path = c;
            return true;
        }
    }
    // PATH search.
    const char* env = std::getenv("PATH");
    if (env) {
        std::stringstream ss(env);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (dir.empty()) continue;
            fs::path p = fs::path(dir) / name;
            if (fs::exists(p)) {
                path = p.string();
                return true;
            }
        }
    }
    return false;
}

// --- LUKS -------------------------------------------------------------------

Result luksOpen(const std::string& device, const std::string& name) {
    std::string tool;
    if (!findTool("cryptsetup", tool)) {
        return Result::error(
            "cryptsetup is not installed; install it (e.g. apt install cryptsetup) "
            "to open LUKS volumes");
    }
    std::string out;
    std::string cmd = tool + " open --type luks '" + device + "' '" + name + "'";
    int rc = runCommand(cmd, out);
    if (rc != 0) {
        return Result::error("cryptsetup failed (" + std::to_string(rc) + "): " + out);
    }
    return Result::ok();
}

Result luksClose(const std::string& name) {
    std::string tool;
    if (!findTool("cryptsetup", tool)) {
        return Result::error(
            "cryptsetup is not installed; install it (e.g. apt install cryptsetup) "
            "to close LUKS mappings");
    }
    std::string out;
    std::string cmd = tool + " close '" + name + "'";
    int rc = runCommand(cmd, out);
    if (rc != 0) {
        return Result::error("cryptsetup failed (" + std::to_string(rc) + "): " + out);
    }
    return Result::ok();
}

Result luksStatus(const std::string& name, std::string& output) {
    std::string tool;
    if (!findTool("cryptsetup", tool)) {
        return Result::error(
            "cryptsetup is not installed; install it (e.g. apt install cryptsetup)");
    }
    std::string out;
    int rc = runCommand(tool + " status '" + name + "'", out);
    if (rc != 0) {
        return Result::error("cryptsetup failed (" + std::to_string(rc) + "): " + out);
    }
    output = out;
    return Result::ok();
}

// --- BitLocker ---------------------------------------------------------------

Result bitlockerUnlock(const std::string& device, const std::string& mount_dir,
                       const std::string& recovery_key) {
    std::string tool;
    if (!findTool("dislocker", tool)) {
        return Result::error(
            "dislocker is not installed; install it (e.g. apt install dislocker) "
            "to unlock BitLocker volumes");
    }
    std::string out;
    std::string cmd = tool + " -V '" + device + "' -O '" + mount_dir + "'";
    if (!recovery_key.empty()) {
        cmd = tool + " -V '" + device + "' -K '" + recovery_key +
              "' -O '" + mount_dir + "'";
    }
    int rc = runCommand(cmd, out);
    if (rc != 0) {
        return Result::error("dislocker failed (" + std::to_string(rc) + "): " + out);
    }
    return Result::ok();
}

Result bitlockerStatus(const std::string& device, std::string& output) {
    std::string tool;
    if (!findTool("dislocker", tool)) {
        return Result::error(
            "dislocker is not installed; install it (e.g. apt install dislocker)");
    }
    std::string out;
    int rc = runCommand(tool + " -V '" + device + "' -i", out);
    if (rc != 0) {
        return Result::error("dislocker failed (" + std::to_string(rc) + "): " + out);
    }
    output = out;
    return Result::ok();
}

// --- UEFI --------------------------------------------------------------------

Result uefiListEntries(std::string& output) {
    std::string tool;
    if (!findTool("efibootmgr", tool)) {
        return Result::error(
            "efibootmgr is not installed; install it (e.g. apt install efibootmgr) "
            "to manage UEFI boot entries");
    }
    std::string out;
    int rc = runCommand(tool + " -v", out);
    if (rc != 0) {
        return Result::error("efibootmgr failed (" + std::to_string(rc) + "): " + out);
    }
    output = out;
    return Result::ok();
}

Result uefiAddEntry(const std::string& label, const std::string& device,
                    const std::string& loader) {
    std::string tool;
    if (!findTool("efibootmgr", tool)) {
        return Result::error(
            "efibootmgr is not installed; install it (e.g. apt install efibootmgr)");
    }
    std::string out;
    std::string cmd = tool + " --create --label '" + label + "' --disk '" +
                      device + "' --loader '" + loader + "'";
    int rc = runCommand(cmd, out);
    if (rc != 0) {
        return Result::error("efibootmgr failed (" + std::to_string(rc) + "): " + out);
    }
    return Result::ok();
}

// --- Windows password reset --------------------------------------------------

Result windowsResetPassword(const std::string& sam_hive_path,
                            const std::string& username) {
    std::string tool;
    if (!findTool("chntpw", tool)) {
        return Result::error(
            "chntpw is not installed; install it (e.g. apt install chntpw) to "
            "reset Windows passwords. The SAM hive must be mounted from the "
            "Windows system partition");
    }
    std::string out;
    std::string cmd = tool + " -u '" + username + "' '" + sam_hive_path + "'";
    int rc = runCommand(cmd, out);
    if (rc != 0) {
        return Result::error("chntpw failed (" + std::to_string(rc) + "): " + out);
    }
    return Result::ok();
}

// --- Linux shadow password reset ----------------------------------------------

Result resetLinuxPassword(const std::string& shadow_path,
                          const std::string& username,
                          const std::string& new_password) {
    if (username.empty() || new_password.empty()) {
        return Result::error("username and password must be non-empty");
    }
    if (!fs::exists(shadow_path)) {
        return Result::error("shadow file not found: " + shadow_path);
    }

    std::ifstream in(shadow_path);
    if (!in) {
        return Result::error("cannot read " + shadow_path);
    }
    std::vector<std::string> lines;
    std::string line;
    bool found = false;
    std::string prefix = username + ":";
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(prefix, 0) == 0) found = true;
        lines.push_back(line);
    }
    in.close();
    if (!found) {
        return Result::error("user '" + username + "' not found in " + shadow_path);
    }

    std::string salt = randomSalt(16);
    std::string hash;
    if (!generateHash(new_password, salt, hash)) {
        return Result::error(
            "failed to generate password hash (openssl is required)");
    }

    // Replace the hash field (2nd colon-separated field) on the user's line.
    for (auto& line : lines) {
        if (line.rfind(prefix, 0) != 0) continue;
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string f;
        while (std::getline(ss, f, ':')) fields.push_back(f);
        if (fields.size() < 2) {
            return Result::error("malformed shadow entry for '" + username + "'");
        }
        fields[1] = hash;
        std::string rebuilt;
        for (size_t i = 0; i < fields.size(); i++) {
            if (i) rebuilt += ":";
            rebuilt += fields[i];
        }
        line = rebuilt;
        break;
    }

    // Atomic write: temp file in the same directory, preserve permissions.
    fs::path p(shadow_path);
    fs::path tmp = p.parent_path() / (p.filename().string() + ".opm-tmp");
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return Result::error("cannot write " + tmp.string());
        }
        for (const auto& l : lines) out << l << "\n";
    }
    std::error_code ec;
    fs::permissions(tmp, fs::status(p).permissions(), ec);
    fs::rename(tmp, p, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return Result::error("failed to replace " + shadow_path + ": " + ec.message());
    }
    return Result::ok();
}

} // namespace opm