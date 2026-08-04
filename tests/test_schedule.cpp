#include <gtest/gtest.h>
#include "opm/schedule.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace opm;

namespace {
std::string tmpRegistry(const char* tag) {
    return std::string("/tmp/opm_sched_") + tag + "_" +
           std::to_string(::getpid()) + "_" + std::to_string(std::rand()) + ".conf";
}
} // namespace

TEST(ScheduleTest, AddListFindRemoveRoundTrip) {
    std::string reg = tmpRegistry("roundtrip");
    {
        ScheduleEntry e;
        e.name = "nightly"; e.minute = "0"; e.hour = "2";
        e.dom = "*"; e.month = "*"; e.dow = "*";
        e.command = "/usr/bin/opm backup create /dev/sda /backup/img";
        std::string err;
        ASSERT_TRUE(e.valid(err)) << err;
        Result r = scheduleAdd(e, reg);
        EXPECT_TRUE(r.success()) << r.message;
    }
    {
        // Duplicate must be rejected.
        ScheduleEntry e;
        e.name = "nightly"; e.minute = "30"; e.hour = "3";
        e.dom = "*"; e.month = "*"; e.dow = "*"; e.command = "x";
        Result r = scheduleAdd(e, reg);
        EXPECT_TRUE(r.failed());
    }
    {
        std::vector<ScheduleEntry> entries;
        Result r = scheduleList(entries, reg);
        ASSERT_TRUE(r.success()) << r.message;
        ASSERT_EQ(entries.size(), 1UL);
        EXPECT_EQ(entries[0].name, "nightly");
        EXPECT_EQ(entries[0].hour, "2");
        // cron line generated correctly with 5 fields + command.
        EXPECT_EQ(entries[0].cronLine(), "0 2 * * * /usr/bin/opm backup create /dev/sda /backup/img");
    }
    {
        ScheduleEntry e;
        EXPECT_TRUE(scheduleFind("nightly", e, reg));
        // Remove
        Result r = scheduleRemove("nightly", reg);
        EXPECT_TRUE(r.success()) << r.message;
        EXPECT_FALSE(scheduleFind("nightly", e, reg));
    }
    std::remove(reg.c_str());
}

TEST(ScheduleTest, ValidationRejectsBadNameAndFields) {
    std::string reg = tmpRegistry("valid");
    {
        ScheduleEntry e; e.name = "bad name!"; e.minute = "0"; e.hour = "0";
        e.dom = "*"; e.month = "*"; e.dow = "*"; e.command = "cmd";
        Result r = scheduleAdd(e, reg);
        EXPECT_TRUE(r.failed()) << "spaces in name must be rejected";
    }
    {
        ScheduleEntry e; e.name = "ok"; e.minute = "99"; e.hour = "0";
        e.dom = "*"; e.month = "*"; e.dow = "*"; e.command = "cmd";
        Result r = scheduleAdd(e, reg);
        EXPECT_TRUE(r.failed()) << "minute 99 must be rejected";
    }
    {
        ScheduleEntry e; e.name = "ok"; e.minute = "*/15"; e.hour = "0";
        e.dom = "*"; e.month = "*"; e.dow = "*"; e.command = "cmd";
        Result r = scheduleAdd(e, reg);
        EXPECT_TRUE(r.success()) << "*/15 step is valid";
    }
    {
        ScheduleEntry e; e.name = "ok2"; e.minute = "0"; e.hour = "0";
        e.dom = "1"; e.month = "*"; e.dow = "0"; e.command = "cmd";
        Result r = scheduleAdd(e, reg);
        EXPECT_TRUE(r.success()) << "0 (Sunday) dow is valid";
    }
    std::remove(reg.c_str());
}

TEST(ScheduleTest, EscapedPipeInCommandSurvivesRegistry) {
    std::string reg = tmpRegistry("pipe");
    ScheduleEntry e;
    e.name = "pipejob"; e.minute = "*"; e.hour = "*";
    e.dom = "*"; e.month = "*"; e.dow = "*";
    e.command = "/bin/sh -c 'opm backup create /dev/sda img | logger'";
    Result r = scheduleAdd(e, reg);
    ASSERT_TRUE(r.success()) << r.message;
    ScheduleEntry out;
    ASSERT_TRUE(scheduleFind("pipejob", out, reg));
    EXPECT_EQ(out.command, e.command) << "pipe characters must survive round-trip";
    std::remove(reg.c_str());
}

TEST(ScheduleTest, SystemdUnitGeneration) {
    ScheduleEntry e;
    e.name = "weekly"; e.minute = "30"; e.hour = "4";
    e.dom = "*"; e.month = "*"; e.dow = "1";  // Monday
    e.command = "opm backup create /dev/sda /backup/img";
    std::string err;
    ASSERT_TRUE(e.valid(err)) << err;
    auto inst = scheduleInstallSystemd(e);
    // Units must be written to the user systemd dir.
    EXPECT_FALSE(inst.written_units.empty());
    // Check the timer file content contains the right calendar (Monday 04:30).
    std::string timer_path = std::string(getenv("HOME")) + "/.config/systemd/user/opm-backup-weekly.timer";
    std::ifstream f(timer_path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("OnCalendar=1 *-*-* 4:30:00"), std::string::npos) << content;
    // Service file must reference the command.
    std::string service_path = std::string(getenv("HOME")) + "/.config/systemd/user/opm-backup-weekly.service";
    std::ifstream sf(service_path);
    std::string scontent((std::istreambuf_iterator<char>(sf)), std::istreambuf_iterator<char>());
    EXPECT_NE(scontent.find("opm backup create /dev/sda /backup/img"), std::string::npos) << scontent;
    // cleanup
    std::remove(timer_path.c_str());
    std::remove(service_path.c_str());
}