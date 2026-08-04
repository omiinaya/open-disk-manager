#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace opm {

// ============================================================================
// File / folder-level backup (minimal POSIX ustar)
//
// A small, self-contained tar writer/reader that produces archives readable by
// GNU tar / bsdtar and can extract the subset of entries it writes itself.
// Scope is deliberately honest: regular files and directories, ustar names up
// to 255 chars via the prefix field, numeric fields in octal. Symlinks are
// stored as link entries (content preserved; extraction restores the symlink
// when the target path is safe). Hard links are preserved (a file with
// st_nlink > 1 whose inode was already archived becomes a hard-link entry);
// char/block device nodes are stored with their major/minor numbers. pax and
// xattrs are NOT supported and are skipped with a note.
// ============================================================================

struct TarEntryInfo {
    std::string name;      // archive path (relative, '/'-separated)
    std::string linkname;  // for symlinks / hard links (archive target path)
    uint64_t size = 0;     // file size in bytes
    uint64_t mtime = 0;
    uint32_t mode = 0644;
    bool is_dir = false;
    bool is_symlink = false;
    bool is_hardlink = false;
    bool is_device = false;    // char or block device node
    bool is_blockdev = false;  // true=block, false=char
    uint32_t devmajor = 0;
    uint32_t devminor = 0;
};

// Write a tar archive of src_dir into archive_path. src_dir itself is the
// archive root (its immediate children become top-level entries).
Result tarCreate(const std::string& src_dir,
                 const std::string& archive_path,
                 ProgressCallback progress = nullptr);

// List archive contents (metadata only).
Result tarList(const std::string& archive_path,
               std::vector<TarEntryInfo>& out);

// Extract an archive written by tarCreate into dest_dir. Skips entries whose
// paths would escape dest_dir (path traversal guard).
Result tarExtract(const std::string& archive_path,
                  const std::string& dest_dir,
                  ProgressCallback progress = nullptr);

} // namespace opm