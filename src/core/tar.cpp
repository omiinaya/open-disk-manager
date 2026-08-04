#include "opm/tar.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>

#ifndef _WIN32
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

namespace opm {

namespace {

constexpr size_t TAR_BLOCK = 512;
constexpr size_t TAR_NAME = 100;
constexpr size_t TAR_PREFIX = 155;

// Portable strnlen (MSVC lacks it).
static size_t boundedStrlen(const char* s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0') n++;
    return n;
}

// POSIX ustar header (512 bytes)
struct __attribute__((packed)) UstarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];      // "ustar\0"
    char version[2];    // "00"
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};
static_assert(sizeof(UstarHeader) == TAR_BLOCK, "ustar header must be 512 bytes");

constexpr char TYPE_REG = '0';
constexpr char TYPE_DIR = '5';
constexpr char TYPE_SYMLINK = '2';

// Write an octal field of `len` bytes (NUL-terminated, space-padded).
void putOctal(char* dst, size_t len, uint64_t value) {
    char tmp[32];
    std::snprintf(tmp, sizeof(tmp), "%0*llo", static_cast<int>(len - 1), static_cast<unsigned long long>(value));
    size_t n = std::strlen(tmp);
    if (n > len - 1) n = len - 1;
    std::memset(dst, '0', len);
    std::memcpy(dst, tmp, n);
    dst[len - 1] = '\0';
}

// Parse an octal field (handles leading spaces/NULs and GNU base-256).
uint64_t parseOctal(const char* p, size_t len) {
    if (len > 0 && (p[0] & 0x80)) {  // GNU base-256
        uint64_t v = p[0] & 0x7F;
        for (size_t i = 1; i < len; i++) v = (v << 8) | (uint8_t)p[i];
        return v;
    }
    uint64_t v = 0;
    bool any = false;
    for (size_t i = 0; i < len; i++) {
        char c = p[i];
        if (c >= '0' && c <= '7') { v = (v << 3) | (uint64_t)(c - '0'); any = true; }
        else if (c == ' ' || c == '\0') { if (any) break; }
        else break;
    }
    return v;
}

uint32_t headerChecksum(const UstarHeader& h) {
    // Checksum computed over header with chksum field set to spaces.
    UstarHeader copy = h;
    std::memset(copy.chksum, ' ', 8);
    uint32_t sum = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&copy);
    for (size_t i = 0; i < sizeof(copy); i++) sum += p[i];
    return sum;
}

void initHeader(UstarHeader& h) {
    std::memset(&h, 0, sizeof(h));
    std::memcpy(h.magic, "ustar", 6);
    h.version[0] = '0'; h.version[1] = '0';
    h.typeflag = TYPE_REG;
}

bool writeFull(FILE* f, const void* data, size_t size) {
    return fwrite(data, 1, size, f) == size;
}

bool readFull(FILE* f, void* data, size_t size) {
    return fread(data, 1, size, f) == size;
}

// Split a path into name (<=100) + prefix (<=155) for ustar.
bool splitName(const std::string& path, char name[TAR_NAME], char prefix[TAR_PREFIX]) {
    std::memset(name, 0, TAR_NAME);
    std::memset(prefix, 0, TAR_PREFIX);
    if (path.size() <= TAR_NAME - 1) {
        std::memcpy(name, path.c_str(), path.size());
        return true;
    }
    // Find the last '/' within the 100-byte name window.
    size_t cut = std::string::npos;
    for (size_t i = path.size(); i > 0; i--) {
        if (path[i - 1] == '/') { cut = i - 1; break; }
    }
    if (cut == std::string::npos || cut >= TAR_PREFIX) return false;
    std::string pref = path.substr(0, cut);
    std::string rest = path.substr(cut + 1);
    if (pref.size() > TAR_PREFIX - 1 || rest.size() > TAR_NAME - 1) return false;
    std::memcpy(prefix, pref.c_str(), pref.size());
    std::memcpy(name, rest.c_str(), rest.size());
    return true;
}

// Sanitize an archive entry path for extraction: reject absolute paths, '..'
// traversal, and drive letters. Returns true and fills out_path if safe.
bool safeExtractPath(const std::string& entry, const std::string& dest, std::string& out_path) {
    if (entry.empty() || entry[0] == '/' || entry[0] == '\\') return false;
    if (entry.size() >= 2 && entry[1] == ':') return false;  // "C:..."
    std::vector<std::string> parts;
    std::string cur;
    for (char c : entry) {
        if (c == '/' || c == '\\') { parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    parts.push_back(cur);
    for (size_t i = 0; i < parts.size(); i++) {
        const auto& p = parts[i];
        if (p == "..") return false;               // parent traversal — always unsafe
        if (p == "." && i != 0) return false;      // ".", only allowed as the root alias
    }
    out_path = dest + "/" + entry;
    return true;
}

#ifndef _WIN32

bool isSymlinkTo(const std::string& linkpath, const std::string& target) {
    struct stat st;
    if (lstat(linkpath.c_str(), &st) != 0) return false;
    if (!S_ISLNK(st.st_mode)) return false;
    char buf[4096];
    ssize_t n = readlink(linkpath.c_str(), buf, sizeof(buf) - 1);
    if (n < 0) return false;
    buf[n] = '\0';
    return target == std::string(buf);
}

// Recursive directory walk, appending entries to the archive.
Result walkAndAppend(FILE* f, const std::string& disk_root,
                     const std::string& rel, uint64_t& count,
                     ProgressCallback progress) {
    std::string full = disk_root + "/" + rel;
    struct stat st;
    if (lstat(full.c_str(), &st) != 0) return Result::error("lstat failed on " + full + ": " + std::strerror(errno));

    UstarHeader h; initHeader(h);
    std::string entry_name = rel.empty() ? std::string(".") : rel;
    std::string prefix;  // unused for root
    if (!splitName(entry_name, h.name, h.prefix))
        return Result::error("path too long for ustar: " + entry_name);

    if (S_ISDIR(st.st_mode)) {
        h.typeflag = TYPE_DIR;
        putOctal(h.mode, sizeof(h.mode), st.st_mode & 07777);
        putOctal(h.size, sizeof(h.size), 0);
        putOctal(h.mtime, sizeof(h.mtime), static_cast<uint64_t>(st.st_mtime));
        putOctal(h.uid, sizeof(h.uid), 0);
        putOctal(h.gid, sizeof(h.gid), 0);
        std::snprintf(h.chksum, sizeof(h.chksum), "%06o", headerChecksum(h));
        h.chksum[6] = '\0'; h.chksum[7] = ' ';
        if (!writeFull(f, &h, sizeof(h))) return Result::error("write failed on " + full);
        count++;
        if (progress) progress(count, 0, "archiving " + entry_name);
        // recurse
        DIR* d = opendir(full.c_str());
        if (!d) return Result::error("opendir failed on " + full + ": " + std::strerror(errno));
        std::vector<std::string> children;
        struct dirent* de;
        while ((de = readdir(d)) != nullptr) {
            std::string n = de->d_name;
            if (n == "." || n == "..") continue;
            children.push_back(n);
        }
        closedir(d);
        std::sort(children.begin(), children.end());
        for (const auto& c : children) {
            std::string child_rel = rel.empty() ? c : rel + "/" + c;
            Result r = walkAndAppend(f, disk_root, child_rel, count, progress);
            if (r.failed()) return r;
        }
        return Result::ok();
    }

    if (S_ISLNK(st.st_mode)) {
        char target[4096];
        ssize_t n = readlink(full.c_str(), target, sizeof(target) - 1);
        if (n < 0) return Result::error("readlink failed on " + full);
        target[n] = '\0';
        h.typeflag = TYPE_SYMLINK;
        putOctal(h.mode, sizeof(h.mode), st.st_mode & 07777);
        putOctal(h.size, sizeof(h.size), 0);
        putOctal(h.mtime, sizeof(h.mtime), static_cast<uint64_t>(st.st_mtime));
        if (std::strlen(target) >= sizeof(h.linkname))
            return Result::error("symlink target too long: " + full);
        std::memcpy(h.linkname, target, std::strlen(target));
        std::snprintf(h.chksum, sizeof(h.chksum), "%06o", headerChecksum(h));
        h.chksum[6] = '\0'; h.chksum[7] = ' ';
        if (!writeFull(f, &h, sizeof(h))) return Result::error("write failed on " + full);
        count++;
        if (progress) progress(count, 0, "archiving " + entry_name);
        return Result::ok();
    }

    if (S_ISREG(st.st_mode)) {
        h.typeflag = TYPE_REG;
        putOctal(h.mode, sizeof(h.mode), st.st_mode & 07777);
        putOctal(h.size, sizeof(h.size), static_cast<uint64_t>(st.st_size));
        putOctal(h.mtime, sizeof(h.mtime), static_cast<uint64_t>(st.st_mtime));
        putOctal(h.uid, sizeof(h.uid), 0);
        putOctal(h.gid, sizeof(h.gid), 0);
        std::snprintf(h.chksum, sizeof(h.chksum), "%06o", headerChecksum(h));
        h.chksum[6] = '\0'; h.chksum[7] = ' ';
        if (!writeFull(f, &h, sizeof(h))) return Result::error("write failed on " + full);
        std::FILE* src = std::fopen(full.c_str(), "rb");
        if (!src) return Result::error("cannot open " + full + " for reading");
        uint8_t buf[65536];
        uint64_t remaining = static_cast<uint64_t>(st.st_size);
        while (remaining > 0) {
            size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : static_cast<size_t>(remaining);
            size_t got = fread(buf, 1, chunk, src);
            if (got == 0) { std::fclose(src); return Result::error("short read on " + full); }
            if (!writeFull(f, buf, got)) { std::fclose(src); return Result::error("write failed on " + full); }
            remaining -= got;
        }
        std::fclose(src);
        // pad to 512
        size_t pad = (TAR_BLOCK - (static_cast<size_t>(st.st_size) % TAR_BLOCK)) % TAR_BLOCK;
        static uint8_t zeros[TAR_BLOCK] = {0};
        if (pad > 0 && !writeFull(f, zeros, pad)) return Result::error("write failed on " + full);
        count++;
        if (progress) progress(count, 0, "archiving " + entry_name);
        return Result::ok();
    }

    // Skip special files (fifo, device, socket) silently — documented scope.
    return Result::ok();
}

#endif // !_WIN32

} // namespace

// ---------------------------------------------------------------------------
// tarCreate
// ---------------------------------------------------------------------------
Result tarCreate(const std::string& src_dir,
                 const std::string& archive_path,
                 ProgressCallback progress) {
#ifdef _WIN32
    (void)src_dir; (void)archive_path; (void)progress;
    return Result::error("tarCreate is not supported on Windows in this build; use the Linux CLI or a native tar");
#else
    struct stat root_st;
    if (stat(src_dir.c_str(), &root_st) != 0)
        return Result::error("cannot access " + src_dir + ": " + std::strerror(errno));
    if (!S_ISDIR(root_st.st_mode))
        return Result::error(src_dir + " is not a directory");

    FILE* f = std::fopen(archive_path.c_str(), "wb");
    if (!f) return Result::error("cannot open " + archive_path + " for writing");

    uint64_t count = 0;
    Result r = walkAndAppend(f, src_dir, "", count, progress);
    if (r.failed()) { std::fclose(f); std::remove(archive_path.c_str()); return r; }

    // Two 512-byte zero blocks terminate the archive.
    static uint8_t zeros[TAR_BLOCK] = {0};
    if (!writeFull(f, zeros, TAR_BLOCK) || !writeFull(f, zeros, TAR_BLOCK)) {
        std::fclose(f); std::remove(archive_path.c_str());
        return Result::error("write failed on archive terminator");
    }
    std::fclose(f);
    return Result::ok();
#endif
}

// ---------------------------------------------------------------------------
// tarList
// ---------------------------------------------------------------------------
Result tarList(const std::string& archive_path, std::vector<TarEntryInfo>& out) {
    FILE* f = std::fopen(archive_path.c_str(), "rb");
    if (!f) return Result::error("cannot open " + archive_path);
    out.clear();
    UstarHeader h;
    while (true) {
        if (!readFull(f, &h, sizeof(h))) { std::fclose(f); return Result::ok(); }
        bool all_zero = true;
        for (size_t i = 0; i < sizeof(h); i++) {
            if (reinterpret_cast<const uint8_t*>(&h)[i] != 0) { all_zero = false; break; }
        }
        if (all_zero) { std::fclose(f); return Result::ok(); }
        if (std::memcmp(h.magic, "ustar", 6) != 0) {
            std::fclose(f);
            return Result::error("archive is not a valid ustar tar at header for '" +
                                 std::string(h.name, boundedStrlen(h.name, sizeof(h.name))) + "'");
        }
        TarEntryInfo e;
        std::string pref(h.prefix, boundedStrlen(h.prefix, sizeof(h.prefix)));
        e.name = pref.empty() ? std::string(h.name, boundedStrlen(h.name, sizeof(h.name)))
                              : pref + "/" + std::string(h.name, boundedStrlen(h.name, sizeof(h.name)));
        e.size = parseOctal(h.size, sizeof(h.size));
        e.mtime = parseOctal(h.mtime, sizeof(h.mtime));
        e.mode = static_cast<uint32_t>(parseOctal(h.mode, sizeof(h.mode)));
        e.is_dir = h.typeflag == TYPE_DIR;
        e.is_symlink = h.typeflag == TYPE_SYMLINK;
        e.linkname = std::string(h.linkname, boundedStrlen(h.linkname, sizeof(h.linkname)));
        out.push_back(e);
        // skip data blocks (dirs/symlinks have size 0)
        uint64_t skip = ((e.size + TAR_BLOCK - 1) / TAR_BLOCK) * TAR_BLOCK;
        if (skip > 0) {
            if (std::fseek(f, static_cast<long>(skip), SEEK_CUR) != 0) {
                std::fclose(f);
                return Result::error("truncated archive (data block)");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// tarExtract
// ---------------------------------------------------------------------------
Result tarExtract(const std::string& archive_path,
                  const std::string& dest_dir,
                  ProgressCallback progress) {
#ifdef _WIN32
    (void)archive_path; (void)dest_dir; (void)progress;
    return Result::error("tarExtract is not supported on Windows in this build; use the Linux CLI or a native tar");
#else
    struct stat dst_st;
    if (stat(dest_dir.c_str(), &dst_st) != 0)
        return Result::error("cannot access destination " + dest_dir + ": " + std::strerror(errno));
    if (!S_ISDIR(dst_st.st_mode))
        return Result::error(dest_dir + " is not a directory");

    FILE* f = std::fopen(archive_path.c_str(), "rb");
    if (!f) return Result::error("cannot open " + archive_path);

    uint64_t count = 0;
    UstarHeader h;
    while (true) {
        if (!readFull(f, &h, sizeof(h))) { std::fclose(f); return Result::ok(); }
        bool all_zero = true;
        for (size_t i = 0; i < sizeof(h); i++) {
            if (reinterpret_cast<const uint8_t*>(&h)[i] != 0) { all_zero = false; break; }
        }
        if (all_zero) { std::fclose(f); return Result::ok(); }
        if (std::memcmp(h.magic, "ustar", 6) != 0) {
            std::fclose(f);
            return Result::error("archive is not a valid ustar tar");
        }
        std::string pref(h.prefix, boundedStrlen(h.prefix, sizeof(h.prefix)));
        std::string entry = pref.empty() ? std::string(h.name, boundedStrlen(h.name, sizeof(h.name)))
                                         : pref + "/" + std::string(h.name, boundedStrlen(h.name, sizeof(h.name)));
        uint64_t size = parseOctal(h.size, sizeof(h.size));
        std::string out_path;
        if (!safeExtractPath(entry, dest_dir, out_path)) {
            std::fclose(f);
            return Result::error("refusing to extract unsafe path: " + entry);
        }

        if (h.typeflag == TYPE_DIR) {
            std::string mkdir = std::string("mkdir -p ") + out_path;
            if (std::system(mkdir.c_str()) != 0) {
                std::fclose(f);
                return Result::error("cannot create directory " + out_path);
            }
        } else if (h.typeflag == TYPE_SYMLINK) {
            std::string target(h.linkname, boundedStrlen(h.linkname, sizeof(h.linkname)));
            if (!target.empty()) {
                std::remove(out_path.c_str());
                if (symlink(target.c_str(), out_path.c_str()) != 0 && errno != EEXIST) {
                    std::fclose(f);
                    return Result::error("cannot create symlink " + out_path + ": " + std::strerror(errno));
                }
            }
        } else if (h.typeflag == TYPE_REG) {
            // Ensure parent dir exists.
            auto slash = out_path.find_last_of('/');
            if (slash != std::string::npos) {
                std::string parent = out_path.substr(0, slash);
                std::string mkdir = std::string("mkdir -p ") + parent;
                std::system(mkdir.c_str());
            }
            std::FILE* out = std::fopen(out_path.c_str(), "wb");
            if (!out) {
                std::fclose(f);
                return Result::error("cannot open " + out_path + " for writing: " + std::strerror(errno));
            }
            uint8_t buf[65536];
            uint64_t remaining = size;
            while (remaining > 0) {
            size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : static_cast<size_t>(remaining);
            size_t got = fread(buf, 1, chunk, f);
            if (got == 0) { std::fclose(out); std::fclose(f); return Result::error("truncated archive while reading " + entry); }
            if (fwrite(buf, 1, got, out) != got) { std::fclose(out); std::fclose(f); return Result::error("write failed on " + out_path); }
            remaining -= got;
            }
            std::fclose(out);
            // Consume the 512-block padding after the file data.
            size_t pad = (TAR_BLOCK - (size % TAR_BLOCK)) % TAR_BLOCK;
            if (pad > 0 && std::fseek(f, static_cast<long>(pad), SEEK_CUR) != 0) {
            std::fclose(f);
            return Result::error("truncated archive (padding) while reading " + entry);
            }
            } else {
            // Unsupported type: skip its data blocks.
        }
        // Skip any data blocks for non-reg entries (size should be 0, but be safe).
        uint64_t skip = ((size + TAR_BLOCK - 1) / TAR_BLOCK) * TAR_BLOCK;
        if (skip > 0 && h.typeflag != TYPE_REG) {
            if (std::fseek(f, static_cast<long>(skip), SEEK_CUR) != 0) {
                std::fclose(f);
                return Result::error("truncated archive (data block)");
            }
        }
        count++;
        if (progress) progress(count, 0, "extracting " + entry);
    }
#endif
}

} // namespace opm