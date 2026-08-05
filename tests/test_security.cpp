#include <gtest/gtest.h>
#include "opm/security.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace opm;

namespace fs = std::filesystem;

TEST(SecurityTest, FindToolFindsExistingAndMissing) {
    std::string path;
    // Positive case must hold for at least one tool present on the host
    // (Linux: /bin/sh; Windows native CI: none of the /usr/bin candidates
    // exist and the PATH split is ';', so skip the strict positive check).
    bool found_known = false;
    for (const char* name : { "sh", "bash", "ls", "cmd" }) {
        if (findTool(name, path)) { found_known = true; break; }
    }
#ifndef _WIN32
    EXPECT_TRUE(found_known) << "at least one common tool must be found";
#endif
    EXPECT_FALSE(findTool("opm-definitely-not-a-real-tool-xyz", path));
}

TEST(SecurityTest, ResetLinuxPasswordReplacesHashAtomically) {
    // The whole test exercises the openssl hash pipeline; skip gracefully
    // where openssl is unavailable (e.g. cross-compiled Windows under Wine).
    FILE* probe_p = ::popen("openssl version 2>/dev/null", "r");
    if (!probe_p) {
        GTEST_SKIP() << "openssl not available on this platform";
    }
    char probe_buf[64];
    bool have_openssl = ::fgets(probe_buf, sizeof(probe_buf), probe_p) != nullptr;
    ::pclose(probe_p);
    if (!have_openssl) {
        GTEST_SKIP() << "openssl not available on this platform";
    }

    fs::path dir = fs::temp_directory_path() /
                   ("opm_sec_" + std::to_string(::getpid()) + "_" +
                    std::to_string(std::rand()));
    fs::create_directories(dir);
    fs::path shadow = dir / "shadow";
    {
        std::ofstream out(shadow);
        out << "root:x:0:0:root:/root:/bin/bash\n"
            << "alice:oldhash123:1000:1000::/home/alice:/bin/bash\n"
            << "bob:!locked:1001:1001::/home/bob:/bin/bash\n";
    }

    Result r = resetLinuxPassword(shadow.string(), "alice", "NewPass!42");
    ASSERT_TRUE(r.success()) << r.message;

    std::string content;
    {
        std::ifstream in(shadow);
        std::stringstream ss;
        ss << in.rdbuf();
        content = ss.str();
    }

    // Alice's hash changed to a $6$ (SHA-512) hash.
    EXPECT_NE(content.find("alice:$6$"), std::string::npos);
    EXPECT_EQ(content.find("oldhash123"), std::string::npos);
    // Other users untouched.
    EXPECT_NE(content.find("root:x:0:0"), std::string::npos);
    EXPECT_NE(content.find("bob:!locked:1001"), std::string::npos);

    // Verify the new hash actually validates against the password.
    std::string hash_line = content.substr(content.find("alice:"));
    std::string hash = hash_line.substr(6, hash_line.find(':', 6) - 6);
    // Recompute with the same salt: openssl -6 with the embedded salt.
    // Extract salt from $6$<salt>$<hash>
    std::string salt = hash.substr(3, hash.find('$', 3) - 3);
    std::string cmd = "printf 'NewPass!42\\n' | openssl passwd -6 -salt " + salt +
                      " -stdin 2>/dev/null";
    FILE* p = ::popen(cmd.c_str(), "r");
    ASSERT_TRUE(p);
    char buf[256];
    std::string recomputed;
    while (std::fgets(buf, sizeof(buf), p)) recomputed += buf;
    ::pclose(p);
    while (!recomputed.empty() &&
           (recomputed.back() == '\n' || recomputed.back() == '\r')) recomputed.pop_back();
    EXPECT_EQ(recomputed, hash);

    fs::remove_all(dir);
}

TEST(SecurityTest, ResetLinuxPasswordUnknownUserFails) {
    fs::path dir = fs::temp_directory_path() /
                   ("opm_sec_" + std::to_string(::getpid()));
    fs::create_directories(dir);
    fs::path shadow = dir / "shadow";
    {
        std::ofstream out(shadow);
        out << "root:x:0:0:root:/root:/bin/bash\n";
    }
    Result r = resetLinuxPassword(shadow.string(), "ghost", "pw");
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("not found"), std::string::npos);
    fs::remove_all(dir);
}

TEST(SecurityTest, ResetLinuxPasswordMissingFileFails) {
    Result r = resetLinuxPassword("/nonexistent/opm/shadow", "root", "pw");
    EXPECT_TRUE(r.failed());
}

// The external tool wrappers must produce honest, actionable errors when the
// tool is not installed (none of cryptsetup/dislocker/efibootmgr/chntpw are
// present on this test box).
// The wrappers must always return a failure that names the tool they tried to
// use — either "not installed; install it" (tool absent) or "failed" (tool
// present but the operation couldn't complete). This is environment-agnostic,
// so it holds whether or not cryptsetup/dislocker/efibootmgr/chntpw are
// installed on the host (e.g. the GitHub Ubuntu runner ships several of them).
// The wrappers must either succeed (tool genuinely installed and the op works)
// or fail with a message that names the tool they tried to use. This holds in
// any environment: when the tool is absent the failure MUST name it and say
// how to install it; when the tool is present the call may legitimately
// succeed or fail for another reason. We assert the tool-absent behavior by
// checking findTool() and, when absent, requiring the actionable message.
TEST(SecurityTest, ToolWrappersReportMissingToolHonestly) {
    std::string out, tool;

    Result r = luksOpen("/dev/null", "test");
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("cryptsetup"), std::string::npos)
        << "message must name cryptsetup: " << r.message;
    if (!findTool("cryptsetup", tool)) {
        EXPECT_NE(r.message.find("install"), std::string::npos)
            << "when cryptsetup is absent the message must say how to install it";
    }

    r = bitlockerUnlock("/dev/null", "/mnt/x");
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("dislocker"), std::string::npos)
        << "message must name dislocker: " << r.message;
    if (!findTool("dislocker", tool)) {
        EXPECT_NE(r.message.find("install"), std::string::npos)
            << "when dislocker is absent the message must say how to install it";
    }

    r = uefiListEntries(out);
    // On machines with efibootmgr installed this may legitimately succeed
    // (returns a list) or fail for another reason; on machines without it, it
    // must fail and name the tool. Only the absent case is strict.
    if (!findTool("efibootmgr", tool)) {
        EXPECT_TRUE(r.failed());
        EXPECT_NE(r.message.find("efibootmgr"), std::string::npos)
            << "when efibootmgr is absent the message must name it: " << r.message;
        EXPECT_NE(r.message.find("install"), std::string::npos)
            << "and say how to install it";
    }

    r = windowsResetPassword("/tmp/sam", "Administrator");
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("chntpw"), std::string::npos)
        << "message must name chntpw: " << r.message;
    if (!findTool("chntpw", tool)) {
        EXPECT_NE(r.message.find("install"), std::string::npos)
            << "when chntpw is absent the message must say how to install it";
    }
}
