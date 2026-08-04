#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"

#include <cstring>

namespace opm {

namespace {

// GPT partition type -> MBR partition type byte. PartitionType values are
// the MBR type bytes themselves, so this is mostly a direct mapping with a
// few fallbacks for types that have no MBR equivalent.
uint8_t mbrTypeByteFor(PartitionType type) {
    switch (type) {
        case PartitionType::EFI:        return 0xEF;  // EFI system
        case PartitionType::LinuxSwap:  return 0x82;
        case PartitionType::LinuxLVM:   return 0x8E;
        case PartitionType::LinuxRAID:  return 0xFD;
        case PartitionType::Linux:      return 0x83;
        case PartitionType::NTFS:       return 0x07;  // NTFS/exFAT
        case PartitionType::FAT12:      return 0x01;
        case PartitionType::FAT16:
        case PartitionType::FAT16B:     return 0x06;
        case PartitionType::FAT16BLBA:  return 0x0E;
        case PartitionType::FAT32CHS:   return 0x0B;
        case PartitionType::FAT32LBA:   return 0x0C;
        case PartitionType::Unknown:    return 0x83;
        default:                        return 0x83;
    }
}

// MBR extended container types are not converted as themselves: on the GPT
// side the container disappears and its logical partitions become standalone
// entries.
bool isExtendedContainer(PartitionType type) {
    return type == PartitionType::ExtendedCHS ||
           type == PartitionType::ExtendedLBA ||
           type == PartitionType::LinuxExtended;
}

// Human-readable MBR partition type name (for error messages)
std::string mbrTypeName(uint8_t t) {
    switch (t) {
        case 0x01: return "FAT12";
        case 0x04: return "FAT16 <32M";
        case 0x05: return "Extended CHS";
        case 0x06: return "FAT16";
        case 0x07: return "NTFS/exFAT";
        case 0x0B: return "FAT32 CHS";
        case 0x0C: return "FAT32 LBA";
        case 0x0E: return "FAT16 LBA";
        case 0x0F: return "Extended LBA";
        case 0x82: return "Linux swap";
        case 0x83: return "Linux";
        case 0x85: return "Linux extended";
        case 0x8E: return "Linux LVM";
        case 0xEF: return "EFI system";
        case 0xFD: return "Linux RAID";
        default:   return "unknown";
    }
}

Result convertToGPT(std::shared_ptr<DiskIO> disk) {
    auto source = PartitionTable::load(disk);
    if (!source) {
        return Result::error("No partition table found to convert");
    }

    auto gpt = GPTTable::createNew(disk);

    int added = 0;
    int skipped = 0;
    const auto parts = source->getPartitions();
    for (size_t i = 0; i < parts.size(); i++) {
        const auto& p = parts[i];
        // Extended containers disappear; logicals become standalone.
        if (isExtendedContainer(p.type())) {
            skipped++;
            continue;
        }
        PartitionType t = p.type();
        // MBR hidden types carry the 0x10 bit (e.g. 0x17 = hidden NTFS). GPT
        // has no hidden bit; normalize to the visible type so the GUID map
        // recognizes it.
        if ((static_cast<uint8_t>(t) & 0x10) != 0) {
            t = static_cast<PartitionType>(static_cast<uint8_t>(t) & ~0x10);
        }
        if (t == PartitionType::Unknown) {
            t = PartitionType::Linux;
        }
        Result r = gpt->createPartition(p.startSector(), p.sizeBytes(), t, p.name());
        if (r.failed()) {
            return Result::error(
                "MBR -> GPT conversion failed at partition " + std::to_string(i + 1) +
                " (start=" + std::to_string(p.startSector()) + "): " + r.message);
        }
        added++;
    }

    Result r = gpt->commit();
    if (r.failed()) {
        return Result::error("MBR -> GPT commit failed: " + r.message);
    }

    std::string msg = "Converted MBR to GPT: " + std::to_string(added) +
                      " partition(s) written";
    if (skipped > 0) {
        msg += ", " + std::to_string(skipped) + " extended container(s) merged";
    }
    return Result::ok();
}

Result convertToMBR(std::shared_ptr<DiskIO> disk) {
    auto source = PartitionTable::load(disk);
    if (!source) {
        return Result::error("No partition table found to convert");
    }

    const auto parts = source->getPartitions();

    // MBR supports at most 4 primary partitions (extended conversion is a
    // separate feature; here we refuse rather than silently drop data).
    int primary_count = 0;
    for (const auto& p : parts) {
        if (!isExtendedContainer(p.type())) {
            primary_count++;
        }
    }
    if (primary_count > 4) {
        return Result::error(
            "GPT -> MBR conversion refused: the disk has " +
            std::to_string(primary_count) +
            " partitions but MBR supports at most 4 primary partitions");
    }

    auto mbr = MBRTable::createNew(disk);

    int added = 0;
    for (size_t i = 0; i < parts.size(); i++) {
        const auto& p = parts[i];
        if (isExtendedContainer(p.type())) {
            continue;
        }
        // MBR LBA fields are 32-bit: partitions must start and end below 2 TiB.
        if (p.startSector() >= (1ULL << 32)) {
            return Result::error(
                "GPT -> MBR conversion refused: partition " + std::to_string(i + 1) +
                " starts at sector " + std::to_string(p.startSector()) +
                " (beyond the 2 TiB MBR limit)");
        }
        if (p.startSector() + p.sectorCount() > (1ULL << 32)) {
            return Result::error(
                "GPT -> MBR conversion refused: partition " + std::to_string(i + 1) +
                " extends beyond the 2 TiB MBR limit");
        }
        PartitionType t = p.type();
        if (t == PartitionType::Unknown) {
            t = PartitionType::Linux;
        }
        uint8_t type_byte = mbrTypeByteFor(t);
        PartitionType mapped = static_cast<PartitionType>(type_byte);
        Result r = mbr->createPartition(p.startSector(), p.sizeBytes(), mapped, "");
        if (r.failed()) {
            return Result::error(
                "GPT -> MBR conversion failed at partition " + std::to_string(i + 1) +
                " (start=" + std::to_string(p.startSector()) +
                ", type=" + mbrTypeName(type_byte) + "): " + r.message);
        }
        // Preserve the bootable flag across the conversion. The positional
        // number matches because partitions are added in start order.
        if (p.isBootable()) {
            mbr->setPartitionBootable(added + 1, true);
        }
        added++;
    }

    Result r = mbr->commit();
    if (r.failed()) {
        return Result::error("GPT -> MBR commit failed: " + r.message);
    }

    // Wipe the old GPT remnants (primary header at LBA 1, partition array at
    // LBA 2..33, and the backup GPT at the end of the disk) so table detection
    // prefers the freshly written MBR. Without this, loaders that probe the
    // GPT magic first would still report a GPT.
    const uint32_t ss = disk->sectorSize();
    const uint64_t total = disk->sectorCount();
    std::vector<uint8_t> zeros(static_cast<size_t>(ss) * 33u, 0);
    r = disk->writeSectors(zeros.data(), 1, 33);
    if (r.failed()) {
        return Result::error("GPT -> MBR: failed to wipe primary GPT: " + r.message);
    }
    if (total > 33) {
        r = disk->writeSectors(zeros.data(), total - 33, 33);
        if (r.failed()) {
            return Result::error("GPT -> MBR: failed to wipe backup GPT: " + r.message);
        }
    }
    r = disk->flush();
    if (r.failed()) {
        return Result::error("GPT -> MBR: flush failed: " + r.message);
    }

    return Result::ok();
}

} // anonymous namespace

Result convertPartitionTable(std::shared_ptr<DiskIO> disk, TableType target) {
    if (!disk || !disk->isOpen()) {
        return Result::error("Disk not open");
    }
    if (disk->isReadOnly()) {
        return Result::error("Disk is read-only; open it read-write to convert");
    }

    if (target != TableType::MBR && target != TableType::GPT) {
        return Result::error("Unsupported target partition table type");
    }

    auto source = PartitionTable::load(disk);
    if (!source) {
        return Result::error("No partition table found to convert");
    }
    if (source->type() == target) {
        return Result::error("Disk already uses a " + source->typeName() + " table");
    }

    // Convert between MBR and GPT only. The load already established the
    // source; dispatch on the target now.
    if (target == TableType::GPT) {
        return convertToGPT(disk);
    }
    return convertToMBR(disk);
}

} // namespace opm
