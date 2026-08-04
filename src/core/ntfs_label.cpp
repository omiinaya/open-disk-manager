#include "opm/ntfs_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <vector>

namespace opm {
namespace ntfs {

namespace {

uint64_t readLE64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

uint32_t readLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readLE16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

// Resident attribute header size (type,length,nonres,namelen,nameoff,flags,id
// + value_length + value_offset + flags + reserved = 24 bytes).
constexpr size_t RES_ATTR_HEADER = 24;

} // anonymous namespace

void fixupUpdateSequence(void* record) {
    if (!record) return;
    uint8_t* data = static_cast<uint8_t*>(record);
    MFTRecordHeader* hdr = reinterpret_cast<MFTRecordHeader*>(record);

    if (hdr->mr_magic != MFT_RECORD_MAGIC) return;
    uint32_t record_size = hdr->mr_alloc_size;
    if (record_size == 0 || record_size % 512 != 0) return;
    uint16_t usn_offset = hdr->mr_usn_offset;
    uint16_t usa_count = hdr->mr_usn_size;
    if (usa_count < 2) return;

    // Pick a USN value (deterministic; the value itself is arbitrary as long
    // as it matches across the record's sector tails).
    uint16_t usn = static_cast<uint16_t>(0x1000u + (hdr->mr_record_number * 257u) % 0xEFFFu);
    hdr->mr_usn = usn;

    // The update sequence array starts at usn_offset. Word 0 holds the USN
    // value; words 1..usa_count-1 hold the original tails of each 512-byte
    // sector (starting at sector 1).
    uint8_t* usa = data + usn_offset;
    usa[0] = static_cast<uint8_t>(usn & 0xFF);
    usa[1] = static_cast<uint8_t>(usn >> 8);

    uint32_t sector_count = record_size / 512;
    uint32_t fixup_slots = usa_count - 1;
    for (uint32_t s = 1; s < sector_count && s <= fixup_slots; s++) {
        uint8_t* tail = data + s * 512 - 2;
        usa[s * 2] = tail[0];
        usa[s * 2 + 1] = tail[1];
        tail[0] = static_cast<uint8_t>(usn & 0xFF);
        tail[1] = static_cast<uint8_t>(usn >> 8);
    }
}

Result setLabel(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                const std::string& label) {
    if (!disk || !disk->isOpen()) {
        return Result::error("Disk not open");
    }
    if (disk->isReadOnly()) {
        return Result::error("Disk is read-only");
    }

    uint8_t boot[512];
    Result r = disk->readSector(boot, start_sector);
    if (r.failed()) {
        return Result::error("Failed to read NTFS boot sector: " + r.message);
    }
    if (std::memcmp(boot + 3, "NTFS    ", 8) != 0) {
        return Result::error("Not a valid NTFS boot sector");
    }

    const uint16_t bps = readLE16(boot + 11);
    const uint8_t spc = boot[13];
    const uint64_t mft_lcn = readLE64(boot + 48);
    const int8_t cpmr = static_cast<int8_t>(boot[64]);

    uint32_t mft_record_size;
    if (cpmr > 0) {
        mft_record_size = static_cast<uint32_t>(cpmr) * bps * spc;
    } else {
        mft_record_size = 1u << (-cpmr);
    }
    if (mft_record_size < 512 || mft_record_size > 65536) {
        return Result::error("Unsupported MFT record size");
    }

    const uint64_t record_byte =
        (start_sector + mft_lcn * spc) * bps + MFT_VOLUME * mft_record_size;

    std::vector<uint8_t> rec(mft_record_size, 0);
    MFTRecordHeader* hdr = reinterpret_cast<MFTRecordHeader*>(rec.data());
    initMFTRecord(*hdr, MFT_VOLUME, false);
    hdr->mr_flags |= MFT_RECORD_FLAG_SYSTEM;
    hdr->mr_next_attr_id = 4;
    hdr->mr_usn_offset = 48;
    hdr->mr_usn_size = 3;

    size_t off = hdr->mr_attr_offset;  // 56

    // --- $VOLUME_INFORMATION (0x70), resident, 12-byte value ---
    const uint32_t vi_val_len = 12;
    ResidentAttributeHeader* vi =
        reinterpret_cast<ResidentAttributeHeader*>(&rec[off]);
    vi->header.a_type = 0x70;
    vi->header.a_length = RES_ATTR_HEADER + vi_val_len;
    vi->header.a_non_resident = 0;
    vi->header.a_name_length = 0;
    vi->header.a_name_offset = 0;
    vi->header.a_flags = 0;
    vi->header.a_id = 1;
    vi->ra_value_length = vi_val_len;
    vi->ra_value_offset = RES_ATTR_HEADER;
    vi->ra_flags = 0;
    vi->ra_reserved = 0;
    uint8_t* vi_val = &rec[off + vi->ra_value_offset];
    vi_val[0] = 1;  // major version
    vi_val[1] = 1;  // minor version
    vi_val[2] = 0;  // flags (clean)
    // bytes 3..11 zero
    off += vi->header.a_length;

    // --- $VOLUME_NAME (0x60), resident, UTF-16LE ---
    size_t chars = label.size() < 32 ? label.size() : 32;
    const uint32_t name_len = static_cast<uint32_t>(chars) * 2;
    ResidentAttributeHeader* vn =
        reinterpret_cast<ResidentAttributeHeader*>(&rec[off]);
    vn->header.a_type = 0x60;
    vn->header.a_length = RES_ATTR_HEADER + name_len;
    vn->header.a_non_resident = 0;
    vn->header.a_name_length = 0;
    vn->header.a_name_offset = 0;
    vn->header.a_flags = 0;
    vn->header.a_id = 2;
    vn->ra_value_length = name_len;
    vn->ra_value_offset = RES_ATTR_HEADER;
    vn->ra_flags = 0;
    vn->ra_reserved = 0;
    uint8_t* name_val = &rec[off + vn->ra_value_offset];
    for (size_t i = 0; i < chars; i++) {
        uint16_t c = static_cast<unsigned char>(label[i]);
        name_val[i * 2] = static_cast<uint8_t>(c & 0xFF);
        name_val[i * 2 + 1] = static_cast<uint8_t>(c >> 8);
    }
    off += vn->header.a_length;

    // --- $END (0xFFFFFFFF), 8 bytes ---
    AttributeHeader* end = reinterpret_cast<AttributeHeader*>(&rec[off]);
    end->a_type = 0xFFFFFFFFu;
    end->a_length = 8;
    off += 8;

    hdr->mr_used_size = static_cast<uint32_t>(off);

    fixupUpdateSequence(rec.data());

    r = disk->write(rec.data(), record_byte, mft_record_size);
    if (r.failed()) {
        return Result::error("Failed to write $Volume record: " + r.message);
    }
    r = disk->flush();
    if (r.failed()) {
        return Result::error("Failed to flush: " + r.message);
    }
    return Result::ok();
}

} // namespace ntfs
} // namespace opm