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
    bool compress = false;         // per-block sparse+RLE compression (FLAG_COMPRESSED)
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
    bool compressed = false;
    std::string source_name;
    std::string mode_string() const;
};

// One image entry discovered by backupListDir.
struct BackupEntry {
    std::string path;              // absolute path to the image file
    std::string name;              // basename
    BackupInfo info;
    uint64_t file_size = 0;
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

// Retention policy for a backup-set directory.
struct PruneOptions {
    uint64_t keep_full = 0;      // keep the N most recent FULL backups (and any
                                 // incremental/differential newer than the oldest
                                 // retained full); 0 = no count-based pruning
    uint64_t older_than_days = 0;// delete any image older than this many days; 0=off
};

// Grandfather-Father-Son retention: keeps the newest FULL backup of each of
// the last N days (son), the last N ISO weeks (father), and the last N
// calendar months (grandfather). Incremental/differential images are kept
// only while their base full backup survives (chain-safe).
struct GfsOptions {
    uint64_t daily = 7;     // son: newest full of each of the last N days
    uint64_t weekly = 4;    // father: newest full of each of the last N weeks
    uint64_t monthly = 12;  // grandfather: newest full of each of the last N months
};

// Scan a directory for OPMIMG images, sorted by created_at (newest first).
// Non-image files are ignored.
Result backupListDir(const std::string& dir, std::vector<BackupEntry>& entries);

// Apply a retention policy: delete images per PruneOptions. Filled 'removed'
// receives the absolute paths that were deleted. Returns the count of removed
// images (may be 0 with success when everything is retained).
Result backupPrune(const std::string& dir,
                   const PruneOptions& options,
                   std::vector<std::string>& removed);

// Apply a Grandfather-Father-Son retention policy. Filled 'removed' receives
// the absolute paths that were deleted.
Result backupPruneGFS(const std::string& dir,
                      const GfsOptions& options,
                      std::vector<std::string>& removed);

} // namespace opm