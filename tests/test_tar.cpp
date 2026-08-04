#include <gtest/gtest.h>
#include "opm/tar.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#endif

using namespace opm;

namespace {

#ifdef _WIN32
// The self-contained tar module and systemd unit generation are documented as
// Linux-focused (honest "not supported on this build" returns, not silent
// no-ops). Skip their platform-specific tests when cross-compiling for Windows.
#define OPM_SKIP_PLATFORM_SPECIFIC() GTEST_SKIP() << "Linux-only feature (documented); skipped on Windows"
#else
#define OPM_SKIP_PLATFORM_SPECIFIC() ((void)0)
#endif

std::string tmpDir(const char* tag) {
    std::string d = std::string("/tmp/opm_tar_") + tag + "_" +
                    std::to_string(::getpid()) + "_" + std::to_string(std::rand());
    std::string mk = "mkdir -p " + d;
    std::system(mk.c_str());
    return d;
}

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
    f.close();
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool pathExists(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (f) { std::fclose(f); return true; }
    return false;
}

} // namespace

TEST(TarTest, CreateListExtractRoundTrip) {
    OPM_SKIP_PLATFORM_SPECIFIC();
    std::string src = tmpDir("src");
    std::string arc = src + ".tar";
    std::string dst = tmpDir("dst");

    writeFile(src + "/hello.txt", "hello world\n");
    std::system(("mkdir -p " + src + "/nested").c_str());
    writeFile(src + "/nested/data.bin", std::string(3000, 'x'));  // crosses block boundary
    writeFile(src + "/unicode-\xE2\x98\x83.txt", "snowman\n");

    Result r = tarCreate(src, arc);
    ASSERT_TRUE(r.success()) << r.message;

    // Archive must be readable by GNU tar (independent validation).
    std::string list_cmd = "tar -tf " + arc + " 2>/dev/null | wc -l";
    std::FILE* p = popen(list_cmd.c_str(), "r");
    ASSERT_TRUE(p);
    char buf[64] = {0};
    if (fgets(buf, sizeof(buf), p)) {}
    pclose(p);
    int entries = atoi(buf);
    EXPECT_GE(entries, 3) << "GNU tar should see at least 3 entries";

    std::vector<TarEntryInfo> list;
    r = tarList(arc, list);
    ASSERT_TRUE(r.success()) << r.message;
    bool found_hello = false, found_dir = false, found_data = false;
    for (const auto& e : list) {
        if (e.name == "hello.txt") found_hello = true;
        if (e.is_dir && e.name == "nested") found_dir = true;
        if (e.name == "nested/data.bin" && e.size == 3000) found_data = true;
    }
    EXPECT_TRUE(found_hello);
    EXPECT_TRUE(found_dir);
    EXPECT_TRUE(found_data);

    r = tarExtract(arc, dst);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(readFile(dst + "/hello.txt"), "hello world\n");
    EXPECT_EQ(readFile(dst + "/nested/data.bin"), std::string(3000, 'x'));
    EXPECT_EQ(readFile(dst + "/unicode-\xE2\x98\x83.txt"), "snowman\n");

    // cleanup
    std::system(("rm -rf " + src + " " + dst + " " + arc).c_str());
}

TEST(TarTest, RejectsUnsafeExtractionPaths) {
    OPM_SKIP_PLATFORM_SPECIFIC();
    std::string src = tmpDir("src2");
    std::string arc = src + ".tar";
    std::string dst = tmpDir("dst2");
    writeFile(src + "/ok.txt", "fine");

    Result r = tarCreate(src, arc);
    ASSERT_TRUE(r.success()) << r.message;

    // Corrupt the archive to contain a ../ traversal entry: locate the first
    // header and overwrite the name field. Simpler: craft the unsafe path in a
    // fresh archive by replacing the header bytes.
    std::string data = readFile(arc);
    ASSERT_GE(data.size(), 512u);
    // name is bytes 0..99 of the first 512-byte header; set it to "../evil"
    std::string evil = "../evil";
    data.replace(0, evil.size(), evil);
    std::string evil_arc = src + "_evil.tar";
    {
        std::ofstream f(evil_arc, std::ios::binary);
        f << data;
    }
    r = tarExtract(evil_arc, dst);
    EXPECT_TRUE(r.failed()) << "traversal path must be refused";
    EXPECT_FALSE(pathExists(dst + "/evil")) << "no file may be written outside dest";

    std::system(("rm -rf " + src + " " + dst + " " + arc + " " + evil_arc).c_str());
}

TEST(TarTest, SymlinkRoundTrip) {
    OPM_SKIP_PLATFORM_SPECIFIC();
    std::string src = tmpDir("src3");
    std::string arc = src + ".tar";
    std::string dst = tmpDir("dst3");
    writeFile(src + "/target.txt", "link target");
    std::system(("ln -s target.txt " + src + "/alias").c_str());

    Result r = tarCreate(src, arc);
    ASSERT_TRUE(r.success()) << r.message;

    std::vector<TarEntryInfo> list;
    r = tarList(arc, list);
    ASSERT_TRUE(r.success()) << r.message;
    bool found_link = false;
    for (const auto& e : list) {
        if (e.name == "alias" && e.is_symlink) { found_link = true; EXPECT_EQ(e.linkname, "target.txt"); }
    }
    EXPECT_TRUE(found_link) << "symlink entry must be archived";

    r = tarExtract(arc, dst);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(readFile(dst + "/alias"), "link target") << "extracted symlink must resolve";

    std::system(("rm -rf " + src + " " + dst + " " + arc).c_str());
}

TEST(TarTest, HardLinkRoundTrip) {
    OPM_SKIP_PLATFORM_SPECIFIC();
#ifndef _WIN32
    std::string src = tmpDir("srchl");
    std::string arc = src + ".tar";
    std::string dst = tmpDir("dsthl");
    writeFile(src + "/orig.txt", "shared content");
    // hard link: same inode, different name
    std::system(("ln " + src + "/orig.txt " + src + "/hard.txt").c_str());
    // ensure we actually made a hard link (nlink == 2)
    {
        struct stat st;
        ASSERT_EQ(::stat((src + "/orig.txt").c_str(), &st), 0);
        ASSERT_EQ(st.st_nlink, 2);
    }

    Result r = tarCreate(src, arc);
    ASSERT_TRUE(r.success()) << r.message;

    std::vector<TarEntryInfo> list;
    r = tarList(arc, list);
    ASSERT_TRUE(r.success()) << r.message;
    // Exactly one entry is a data file, the other a hard-link entry pointing
    // at it. Which name is the "live" file depends on traversal order, so
    // assert that one hardlink exists with a valid linkname target.
    std::string hard_target;
    int hard_count = 0, reg_count = 0;
    for (const auto& e : list) {
        if (e.is_hardlink) { hard_count++; hard_target = e.linkname; EXPECT_EQ(e.size, 0u); }
        else if (!e.is_dir && !e.is_symlink) { reg_count++; EXPECT_EQ(e.size, std::string("shared content").size()); }
    }
    EXPECT_EQ(hard_count, 1) << "second hard link must be archived as TYPE_HARDLINK";
    EXPECT_EQ(reg_count, 1) << "first hard link archived as the data file";
    // The hard-link target must reference the archived data file.
    EXPECT_FALSE(hard_target.empty());
    {
        bool target_exists = false;
        for (const auto& e : list)
            if (e.name == hard_target) target_exists = true;
        EXPECT_TRUE(target_exists) << "hard-link target '" << hard_target << "' must exist in archive";
    }

    r = tarExtract(arc, dst);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(readFile(dst + "/orig.txt"), "shared content");
    EXPECT_EQ(readFile(dst + "/hard.txt"), "shared content") << "hard link restored";
    // Verify it's a real hard link (same inode), not a copy.
    {
        struct stat a, b;
        ASSERT_EQ(::stat((dst + "/orig.txt").c_str(), &a), 0);
        ASSERT_EQ(::stat((dst + "/hard.txt").c_str(), &b), 0);
        EXPECT_EQ(a.st_ino, b.st_ino) << "extracted entries must share an inode";
    }

    std::system(("rm -rf " + src + " " + dst + " " + arc).c_str());
#endif
}

TEST(TarTest, DeviceNodeRoundTrip) {
    OPM_SKIP_PLATFORM_SPECIFIC();
#ifndef _WIN32
    // Creating a device node needs mknod privileges (CAP_MKNOD). If we can't,
    // skip rather than fake success.
    if (::geteuid() != 0) GTEST_SKIP() << "needs root for mknod";
    std::string src = tmpDir("srcdev");
    std::string arc = src + ".tar";
    std::string dst = tmpDir("dstdev");
    // null char device: major 1, minor 3
    if (::mknod((src + "/null").c_str(), S_IFCHR | 0666, ::makedev(1, 3)) != 0)
        GTEST_SKIP() << "cannot create device node";

    Result r = tarCreate(src, arc);
    ASSERT_TRUE(r.success()) << r.message;

    std::vector<TarEntryInfo> list;
    r = tarList(arc, list);
    ASSERT_TRUE(r.success()) << r.message;
    bool found_dev = false;
    for (const auto& e : list) {
        if (e.name == "null" && e.is_device) {
            found_dev = true;
            EXPECT_FALSE(e.is_blockdev);
            EXPECT_EQ(e.devmajor, 1u);
            EXPECT_EQ(e.devminor, 3u);
        }
    }
    EXPECT_TRUE(found_dev) << "char device must be archived";

    r = tarExtract(arc, dst);
    ASSERT_TRUE(r.success()) << r.message;
    {
        struct stat st;
        ASSERT_EQ(::stat((dst + "/null").c_str(), &st), 0);
        EXPECT_TRUE(S_ISCHR(st.st_mode));
        EXPECT_EQ(::major(st.st_rdev), 1u);
        EXPECT_EQ(::minor(st.st_rdev), 3u);
    }

    std::system(("rm -rf " + src + " " + dst + " " + arc).c_str());
#endif
}