#include "opm/schedule.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#endif

namespace opm {

namespace {

bool hasWildcard(const std::string& f) { return f == "*"; }
bool isStep(const std::string& f) { return f.size() >= 3 && f[0] == '*' && f[1] == '/'; }

bool validField(const std::string& f, int lo, int hi, bool allow_step) {
    if (f == "*") return true;
    if (allow_step && isStep(f)) {
        std::string step = f.substr(2);
        if (step.empty()) return false;
        for (char c : step) if (c < '0' || c > '9') return false;
        return true;
    }
    if (f.empty()) return false;
    for (char c : f) if (c < '0' || c > '9') return false;
    long v = std::strtol(f.c_str(), nullptr, 10);
    return v >= lo && v <= hi;
}

// Home dir from environment or passwd (portable).
std::string homeDir() {
    const char* h = std::getenv("HOME");
    if (h && *h) return std::string(h);
#ifndef _WIN32
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return std::string(pw->pw_dir);
#endif
    return std::string(".");
}

// Replace "~" or "$HOME" at the start of a command so units/crontab work
// regardless of the invoking shell.
std::string expandCommand(const std::string& cmd) {
    std::string out = cmd;
    if (out.rfind("~/", 0) == 0) out = homeDir() + out.substr(1);
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// ScheduleEntry
// ---------------------------------------------------------------------------

std::string ScheduleEntry::cronLine() const {
    return minute + " " + hour + " " + dom + " " + month + " " + dow + " " + expandCommand(command);
}

std::string ScheduleEntry::describe() const {
    std::string when;
    if (month != "*") {
        when += "in month " + month + " ";
    }
    if (dom != "*") {
        when += "on day " + dom + " ";
    } else if (dow != "*") {
        when += "on weekday " + dow + " ";
    } else {
        when += "every day ";
    }
    when += "at " + (hour == "*" ? std::string("every hour") :
                     (minute == "*" ? std::string("every minute past hour ") + hour :
                                      hour + ":" + (minute.size() == 1 ? "0" + minute : minute)));
    return when;
}

bool ScheduleEntry::valid(std::string& err) const {
    if (name.empty()) { err = "schedule name is empty"; return false; }
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            err = "schedule name may only contain [a-zA-Z0-9_-]";
            return false;
        }
    }
    if (command.empty()) { err = "schedule command is empty"; return false; }
    if (!validField(minute, 0, 59, true)) { err = "invalid minute field"; return false; }
    if (!validField(hour, 0, 23, true)) { err = "invalid hour field"; return false; }
    if (!validField(dom, 1, 31, false)) { err = "invalid day-of-month field"; return false; }
    if (!validField(month, 1, 12, false)) { err = "invalid month field"; return false; }
    if (!validField(dow, 0, 7, false)) { err = "invalid day-of-week field"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

std::string scheduleRegistryPath() {
    return homeDir() + "/.config/opm/schedules.conf";
}

static std::string registryDir(const std::string& reg) {
    std::string p = reg.empty() ? scheduleRegistryPath() : reg;
    auto slash = p.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

static bool writeRegistry(const std::string& path, const std::vector<ScheduleEntry>& entries, Result& r) {
    std::string dir = registryDir(path);
    std::string mkdir = std::string("mkdir -p ") + dir;
    if (std::system(mkdir.c_str()) != 0) {
        r = Result::error("cannot create registry directory " + dir);
        return false;
    }
    std::ofstream f(path, std::ios::trunc);
    if (!f) { r = Result::error("cannot open registry " + path + " for writing"); return false; }
    for (const auto& e : entries) {
        // Escape '|' inside the command so the line format stays unambiguous.
        std::string cmd = e.command;
        std::string esc;
        for (char c : cmd) {
            if (c == '|') esc += "\\|";
            else esc += c;
        }
        f << e.name << '|' << e.minute << '|' << e.hour << '|' << e.dom << '|'
          << e.month << '|' << e.dow << '|' << esc << "\n";
    }
    f.close();
    r = Result::ok();
    return true;
}

static bool readRegistry(const std::string& path, std::vector<ScheduleEntry>& out, Result& r) {
    out.clear();
    std::ifstream f(path);
    if (!f) { r = Result::ok(); return true; }  // missing file = empty registry
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        ScheduleEntry e;
        std::string cmd;
        // Parse exactly 6 '|' separators; a '\|' inside the command is literal.
        std::vector<std::string> fields;
        std::string cur;
        bool escaped = false;
        for (char c : line) {
            if (escaped) { cur += c; escaped = false; }
            else if (c == '\\') escaped = true;
            else if (c == '|') { fields.push_back(cur); cur.clear(); }
            else cur += c;
        }
        fields.push_back(cur);
        if (fields.size() != 7) continue;
        e.name = fields[0]; e.minute = fields[1]; e.hour = fields[2];
        e.dom = fields[3]; e.month = fields[4]; e.dow = fields[5];
        e.command = fields[6];
        out.push_back(e);
    }
    r = Result::ok();
    return true;
}

Result scheduleAdd(const ScheduleEntry& entry, const std::string& registry) {
    std::string err;
    if (!entry.valid(err)) return Result::error(err);
    std::string path = registry.empty() ? scheduleRegistryPath() : registry;
    std::vector<ScheduleEntry> entries;
    Result r;
    if (!readRegistry(path, entries, r)) return r;
    for (const auto& e : entries) {
        if (e.name == entry.name) {
            return Result::error("schedule '" + entry.name + "' already exists (remove it first)");
        }
    }
    entries.push_back(entry);
    if (!writeRegistry(path, entries, r)) return r;
    return Result::ok();
}

Result scheduleList(std::vector<ScheduleEntry>& out, const std::string& registry) {
    std::string path = registry.empty() ? scheduleRegistryPath() : registry;
    Result r;
    if (!readRegistry(path, out, r)) return r;
    return Result::ok();
}

Result scheduleRemove(const std::string& name, const std::string& registry) {
    std::string path = registry.empty() ? scheduleRegistryPath() : registry;
    std::vector<ScheduleEntry> entries;
    Result r;
    if (!readRegistry(path, entries, r)) return r;
    bool found = false;
    std::vector<ScheduleEntry> keep;
    for (const auto& e : entries) {
        if (e.name == name) { found = true; continue; }
        keep.push_back(e);
    }
    if (!found) return Result::error("schedule '" + name + "' not found");
    if (!writeRegistry(path, keep, r)) return r;
    return Result::ok();
}

bool scheduleFind(const std::string& name, ScheduleEntry& out, const std::string& registry) {
    std::vector<ScheduleEntry> entries;
    Result r;
    if (!readRegistry(registry.empty() ? scheduleRegistryPath() : registry, entries, r)) return false;
    for (const auto& e : entries) {
        if (e.name == name) { out = e; return true; }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Systemd units
// ---------------------------------------------------------------------------

static std::string systemdCalendar(const ScheduleEntry& e) {
    // Convert the common cron subset to a systemd OnCalendar spec.
    std::string minute = e.minute, hour = e.hour;
    std::string day = e.dom, month = e.month, dow = e.dow;
    // */N -> */N (supported by systemd for minute/hour; keep as-is)
    if (month != "*") {
        return "*-" + month + "-" + (day == "*" ? "1" : day) + " " + hour + ":" + minute + ":00";
    }
    if (day != "*") {
        return "*-*-" + day + " " + hour + ":" + minute + ":00";
    }
    if (dow != "*") {
        std::string w = dow;
        if (w == "7") w = "0";
        return w + " *-*-* " + hour + ":" + minute + ":00";
    }
    return "*-*-* " + hour + ":" + minute + ":00";
}

ScheduleInstallResult scheduleInstallSystemd(const ScheduleEntry& entry) {
    ScheduleInstallResult out;
    std::string err;
    if (!entry.valid(err)) {
        out.status = Result::error(err);
        return out;
    }
    std::string dir = homeDir() + "/.config/systemd/user";
    std::string mkdir = "mkdir -p " + dir;
    if (std::system(mkdir.c_str()) != 0) {
        out.status = Result::error("cannot create systemd user dir " + dir);
        return out;
    }
    std::string cal = systemdCalendar(entry);
    std::string exec = expandCommand(entry.command);
    std::string service_path = dir + "/opm-backup-" + entry.name + ".service";
    std::string timer_path = dir + "/opm-backup-" + entry.name + ".timer";

    {
        std::ofstream f(service_path);
        if (!f) { out.status = Result::error("cannot write " + service_path); return out; }
        f << "[Unit]\nDescription=Open Partition Manager backup '" << entry.name << "'\n"
          << "[Service]\nType=oneshot\nExecStart=" << exec << "\n";
    }
    {
        std::ofstream f(timer_path);
        if (!f) { out.status = Result::error("cannot write " + timer_path); return out; }
        f << "[Unit]\nDescription=Open Partition Manager backup timer '" << entry.name << "'\n"
          << "[Timer]\nOnCalendar=" << cal << "\nPersistent=true\n"
          << "[Install]\nWantedBy=timers.target\n";
    }
    out.written_units = service_path + " " + timer_path;
    // Best-effort live install; a missing bus is honest-noted, not fatal.
    std::string reload = "systemctl --user daemon-reload >/dev/null 2>&1 && "
                         "systemctl --user enable --now opm-backup-" + entry.name + ".timer >/dev/null 2>&1";
    int rc = std::system(reload.c_str());
    if (rc == 0) {
        out.status = Result::ok();
        out.note = "units written and enabled: " + out.written_units;
    } else {
        out.status = Result::ok();
        out.note = "units written to " + out.written_units +
                   " but the systemd user bus is not reachable from this session; "
                   "enable them at next login with: systemctl --user enable --now "
                   "opm-backup-" + entry.name + ".timer";
    }
    return out;
}

ScheduleInstallResult scheduleInstallCrontab(const ScheduleEntry& entry) {
    ScheduleInstallResult out;
    std::string err;
    if (!entry.valid(err)) { out.status = Result::error(err); return out; }
    // Read current crontab (if any), drop any existing entry with our marker,
    // append the new line, and write it back through crontab(1).
    std::string get_cmd = "crontab -l 2>/dev/null | grep -v '# OPM-SCHEDULE:" + entry.name + "' ; "
                          "echo '" + entry.cronLine() + " # OPM-SCHEDULE:" + entry.name + "'";
    std::string install = get_cmd + " | crontab - 2>&1";
    int rc = std::system(install.c_str());
    if (rc != 0) {
        out.status = Result::error(
            "crontab(1) is not available or the user is not permitted to use it "
            "(see /etc/cron.allow). The schedule is saved in the registry; install "
            "manually with: crontab -e  # add: " + entry.cronLine());
        out.note = "registry entry kept; crontab install rejected";
        return out;
    }
    out.status = Result::ok();
    out.note = "installed in user crontab: " + entry.cronLine();
    return out;
}

} // namespace opm