#pragma once

#include "types.hpp"
#include "disk_io.hpp"
#include <string>
#include <memory>

namespace opm {

// ============================================================================
// Image Backup Engine (OPMIMG format)
//
// A single self-contained image binary:
//   [BackupHeader][block bitmap][SHA-256 checksum table][data blocks]
//
// - FULL:      all blocks present + stored.
// - INCREMENTAL / DIFFERENTIAL: only blocks that differ from a base image are
//   physically stored (bit=1 in the bitmap); unchanged blocks are inherited
//   from the base. The image's checksum table always describes the full
//   logical state AFTER this backup, so restore of an incremental applies just
//   the changed blocks on top of a base already on the target.
//
// - differential compares against the FULL base; incremental compares against
//   whatever base image is supplied (differences since the last backup).
// ============================================================================

enum class BackupMode : uint32_t {
    Full = 0,
    Incremental = 1,
    Differential = 2,
};

struct BackupOptions {
    uint32_t block_size = 1048576; // 1 MiB
    ProgressCallback progress_callback;
};

struct BackupInfo {
    BackupMode mode = BackupMode::Full;
    uint64_t source_size = 0;
    uint32_t sector_size = 512;
    uint32_t block_size = 0;
    uint64_t num_blocks = 0;
    uint64_t present_blocks = 0;   // physically stored blocks
    uint64_t created_at = 0;
    std::string source_name;
    std::string mode_string() const;
};

// Create a FULL backup of a device/partition into image_path.
Result backupCreateFull(std::shared_ptr<DiskIO> source,
                        const std::string& image_path,
                        const BackupOptions& options = BackupOptions{});

// Create an INCREMENTAL (differential=false) or DIFFERENTIAL (differential=true)
// backup: stores only blocks whose SHA-256 differs from base_image's state.
Result backupCreateIncremental(std::shared_ptr<DiskIO> source,
                               const std::string& base_image,
                               const std::string& image_path,
                               bool differential,
                               const BackupOptions& options = BackupOptions{});

// Restore an image. For FULL, writes every block. For INCREMENTAL/DIFFERENTIAL,
// writes only the physically-stored (changed) blocks on top of an existing base
// on the target.
Result backupRestore(const std::string& image_path,
                     std::shared_ptr<DiskIO> target,
                     const BackupOptions& options = BackupOptions{});

// Read image metadata (header) without touching the target.
Result backupInfo(const std::string& image_path, BackupInfo& info);

// Re-hash every physically-stored data block and compare against the image's
// checksum table. Returns success only if all stored blocks verify.
Result backupVerify(const std::string& image_path);

} // namespace opm