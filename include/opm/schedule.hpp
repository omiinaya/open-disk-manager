#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace opm {

// ============================================================================
// Backup scheduling
//
// Schedules are stored in a plain-text registry at ~/.config/opm/schedules.conf
// (one entry per line: name|min|hour|dom|month|dow|command). From an entry the
// tool can emit:
//   - a crontab line        (scheduleToCron)
//   - a systemd user timer  (scheduleToSystemdUnits)
//
// The registry operations (add/list/remove) are fully self-contained and
// testable. Installing the schedule into the live system is best-effort:
//   - systemd user timers are written to ~/.config/systemd/user/ and
//     `systemctl --user daemon-reload` is attempted; if the user bus is not
//     reachable the units are still written and an honest note is returned.
//   - crontab(1) is invoked only when --crontab is requested; if the user is
//     not permitted (cron.allow), an honest error is returned.
// ============================================================================

struct ScheduleEntry {
    std::string name;      // unique id, [a-zA-Z0-9_-]
    std::string minute;    // cron field: * | */N | 0-59
    std::string hour;      // cron field: * | */N | 0-23
    std::string dom;       // day of month: * | 1-31
    std::string month;     // month: * | 1-12
    std::string dow;       // day of week: * | 0-7 (0/7 = Sunday)
    std::string command;   // full command line executed by the schedule

    std::string cronLine() const;          // "min hour dom month dow command"
    std::string describe() const;          // human-readable recurrence
    bool valid(std::string& err) const;    // field validation
};

// --- Registry -------------------------------------------------------------

// Path of the registry file (defaults to ~/.config/opm/schedules.conf;
// overridable for tests).
std::string scheduleRegistryPath();

Result scheduleAdd(const ScheduleEntry& entry, const std::string& registry = "");
Result scheduleList(std::vector<ScheduleEntry>& out, const std::string& registry = "");
Result scheduleRemove(const std::string& name, const std::string& registry = "");

// Find a single entry by name (false if absent).
bool scheduleFind(const std::string& name, ScheduleEntry& out, const std::string& registry = "");

// --- Live backend (best-effort) --------------------------------------------

struct ScheduleInstallResult {
    Result status;
    std::string written_units;  // paths written
    std::string note;           // human-readable note about what happened
};

// Write systemd user timer+service units for the entry and try daemon-reload.
ScheduleInstallResult scheduleInstallSystemd(const ScheduleEntry& entry);

// Try to install the entry into the user's crontab.
ScheduleInstallResult scheduleInstallCrontab(const ScheduleEntry& entry);

} // namespace opm