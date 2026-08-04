#pragma once

#include "types.hpp"
#include <string>

namespace opm {

// Locate an external tool: explicit candidate paths first, then a PATH
// search. Returns true and sets `path` when found.
bool findTool(const std::string& name, std::string& path);

// --- LUKS via cryptsetup ----------------------------------------------------
// Opens a LUKS volume (prompts for the passphrase on the controlling
// terminal), closes a mapped name, or reports mapping status.
Result luksOpen(const std::string& device, const std::string& name);
Result luksClose(const std::string& name);
Result luksStatus(const std::string& name, std::string& output);

// --- BitLocker via dislocker ------------------------------------------------
// Unlocks a BitLocker volume to a mount directory. When recovery_key is
// empty, dislocker prompts for the password.
Result bitlockerUnlock(const std::string& device, const std::string& mount_dir,
                       const std::string& recovery_key = "");
Result bitlockerStatus(const std::string& device, std::string& output);

// --- UEFI boot entries via efibootmgr ---------------------------------------
Result uefiListEntries(std::string& output);
Result uefiAddEntry(const std::string& label, const std::string& device,
                    const std::string& loader);

// --- Windows password reset via chntpw --------------------------------------
// Resets the password of `username` in the SAM hive at `sam_hive_path`.
Result windowsResetPassword(const std::string& sam_hive_path,
                            const std::string& username);

// --- Linux shadow password reset (self-contained, no external tool) ---------
// Rewrites `username`'s password hash in /etc/shadow at `shadow_path` using a
// freshly generated SHA-512 ($6$) hash of `new_password`. Writes atomically.
Result resetLinuxPassword(const std::string& shadow_path,
                          const std::string& username,
                          const std::string& new_password);

} // namespace opm