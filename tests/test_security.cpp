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
    EXPECT_TRUE(findTool("sh", path));
    EXPECT_FALSE(findTool("opm-definitely-not-a-real-tool-xyz", path));
}

TEST(SecurityTest, ResetLinuxPasswordReplacesHashAtomically) {
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
TEST(SecurityTest, ToolWrappersReportMissingToolHonestly) {
    std::string out;
    Result r = luksOpen("/dev/null", "test");
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("cryptsetup"), std::string::npos);
    EXPECT_NE(r.message.find("install"), std::string::npos);

    r = bitlockerUnlock("/dev/null", "/mnt/x");
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("dislocker"), std::string::npos);

    r = uefiListEntries(out);
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("efibootmgr"), std::string::npos);

    r = windowsResetPassword("/tmp/sam", "Administrator");
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("chntpw"), std::string::npos);
}
