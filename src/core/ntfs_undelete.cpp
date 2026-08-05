#include "opm/ntfs_undelete.hpp"
#include "opm/disk_io.hpp"
#include "opm/ntfs_impl.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <set>
#include <sys/stat.h>
#include <sys/types.h>

namespace opm {
namespace ntfs {

namespace {

constexpr size_t REC_SIZE = 1024;
constexpr uint32_t NR_HDR = 64;
constexpr uint32_t RES_HDR = 24;

void putU16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
void putU32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF;
}
void putU64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (v >> (8 * i)) & 0xFF;
}

uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t rd32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24);
}
uint64_t rd64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= uint64_t(p[i]) << (8 * i);
    return v;
}

// Parse $FILE_NAME from a record. Returns false when absent/corrupt.
bool parseFileName(const std::vector<uint8_t>& rec, uint64_t& parent_ref,
                   uint64_t& data_size, uint32_t& attrs, std::string& name) {
    if (rec.size() < 56) return false;
    uint16_t attr_off = rd16(rec.data() + 20);
    if (attr_off == 0 || attr_off >= rec.size()) return false;
    size_t off = attr_off;
    while (off + 8 <= rec.size()) {
        uint32_t t = rd32(rec.data() + off);
        if (t == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + off + 4);
        if (len == 0 || off + len > rec.size()) break;
        const uint8_t* h = rec.data() + off;
        if (t == ATTR_FILE_NAME && (h[8] & 1) == 0) {
            uint32_t vlen = rd32(h + 16);
            uint32_t voff = rd32(h + 20);
            if (voff + vlen > len) break;
            const uint8_t* v = h + voff;
            parent_ref = rd64(v);
            data_size = rd64(v + 48);
            attrs = rd32(v + 56);
            uint8_t nlen = v[64];
            name.clear();
            for (uint8_t i = 0; i < nlen; i++) {
                uint16_t c = rd16(v + 66 + i * 2);
                if (c < 128) name += static_cast<char>(c);
                else name += '?';
            }
            return true;
        }
        off += len;
    }
    return false;
}

// Parse the first non-resident $DATA run list; fills runs.
bool parseDataRuns(const std::vector<uint8_t>& rec,
                   std::vector<std::pair<uint64_t, uint64_t>>& runs) {
    runs.clear();
    if (rec.size() < 56) return false;
    uint16_t attr_off = rd16(rec.data() + 20);
    if (attr_off == 0 || attr_off >= rec.size()) return false;
    size_t off = attr_off;
    while (off + 8 <= rec.size()) {
        uint32_t t = rd32(rec.data() + off);
        if (t == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + off + 4);
        if (len == 0 || off + len > rec.size()) break;
        const uint8_t* h = rec.data() + off;
        if (t == ATTR_DATA && (h[8] & 1) && len > NR_HDR) {
            // Prefer the unnamed $DATA stream (name length 0).
            uint8_t nl = h[9];
            if (nl != 0) { off += len; continue; }
            uint16_t runoff = rd16(h + 32);
            if (runoff == 0 || runoff >= len) { off += len; continue; }
            size_t p = off + runoff;
            int64_t prev = 0;
            while (p < rec.size()) {
                uint8_t hdr = rec[p++];
                if (hdr == 0) break;
                int llen = hdr >> 4;
                int olen = hdr & 0x0F;
                if (llen == 0 || p + llen + olen > rec.size()) break;
                uint64_t run_len = 0;
                for (int i = 0; i < llen; i++)
                    run_len |= uint64_t(rec[p + i]) << (8 * i);
                int64_t offs = 0;
                for (int i = 0; i < olen; i++)
                    offs |= int64_t(rec[p + llen + i]) << (8 * i);
                if (olen && (rec[p + llen + olen - 1] & 0x80))
                    for (int i = olen; i < 8; i++)
                        offs |= int64_t(-1) << (8 * i);
                int64_t lcn = prev + offs;
                runs.push_back({ uint64_t(lcn), run_len });
                prev = lcn;
                p += llen + olen;
            }
            return !runs.empty();
        }
        off += len;
    }
    return false;
}

struct NtfsLayoutInfo {
    uint8_t spc = 1;
    uint64_t mft_lcn = 0;
    uint64_t mft_mirr_lcn = 0;
    uint32_t mft_record_size = REC_SIZE;
    uint64_t total_sectors = 0;
    uint64_t bpc = 512;
};

bool readLayout(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                NtfsLayoutInfo& out) {
    uint8_t boot[512];
    if (disk->readSector(boot, start_sector).failed()) return false;
    if (std::memcmp(boot + 3, "NTFS    ", 8) != 0) return false;
    out.spc = boot[13];
    out.total_sectors = rd64(boot + 40);
    out.mft_lcn = rd64(boot + 48);
    out.mft_mirr_lcn = rd64(boot + 56);
    int8_t cpmr = static_cast<int8_t>(boot[64]);
    if (cpmr > 0) out.mft_record_size = uint32_t(cpmr) * 512 * out.spc;
    else out.mft_record_size = 1u << (-cpmr);
    if (out.mft_record_size < 256 || out.mft_record_size > 65536)
        out.mft_record_size = REC_SIZE;
    out.bpc = uint64_t(out.spc) * 512;
    return out.mft_lcn != 0;
}

// Read one MFT record.
std::vector<uint8_t> readRecord(std::shared_ptr<DiskIO> disk, uint64_t start,
                                const NtfsLayoutInfo& layout, uint64_t num) {
    uint64_t off = (start + layout.mft_lcn * layout.spc) * 512 +
                   num * layout.mft_record_size;
    std::vector<uint8_t> rec(layout.mft_record_size, 0);
    if (disk->read(rec.data(), off, rec.size()).failed()) rec.clear();
    return rec;
}

// $Bitmap location: resolved from record 6's run list when present, else the
// fixed position after the 16-record MFT (format path).
uint64_t bitmapLcn(std::shared_ptr<DiskIO> disk, uint64_t start,
                   const NtfsLayoutInfo& layout) {
    auto rec = readRecord(disk, start, layout, MFT_BITMAP);
    std::vector<std::pair<uint64_t, uint64_t>> runs;
    if (parseDataRuns(rec, runs) && !runs.empty()) return runs[0].first;
    // fixed geometry fallback
    uint64_t mft_clusters = (16ULL * layout.mft_record_size + layout.bpc - 1) /
                            layout.bpc;
    return layout.mft_lcn + mft_clusters;
}

// Read the whole $Bitmap into memory.
std::vector<uint8_t> readBitmap(std::shared_ptr<DiskIO> disk, uint64_t start,
                                const NtfsLayoutInfo& layout) {
    uint64_t clusters = layout.total_sectors / layout.spc;
    uint64_t bmp_bytes = (clusters + 7) / 8;
    uint64_t lcn = bitmapLcn(disk, start, layout);
    std::vector<uint8_t> bmp(bmp_bytes, 0);
    if (disk->read(bmp.data(), (start + lcn * layout.spc) * 512, bmp_bytes).failed()) {
        bmp.clear();
    }
    return bmp;
}

bool isClusterFree(const std::vector<uint8_t>& bmp, uint64_t c) {
    uint64_t byte_idx = c / 8;
    uint64_t bit_idx = c % 8;
    if (byte_idx >= bmp.size()) return false;
    return (bmp[byte_idx] & (1 << bit_idx)) == 0;
}

void setClusterUsed(std::vector<uint8_t>& bmp, uint64_t c) {
    uint64_t byte_idx = c / 8;
    uint64_t bit_idx = c % 8;
    if (byte_idx < bmp.size()) bmp[byte_idx] |= static_cast<uint8_t>(1 << bit_idx);
}

// Build a $FILE_NAME key for an index entry from a deleted record's own
// $FILE_NAME attribute value (copied verbatim from the record).
bool copyFileNameKey(const std::vector<uint8_t>& rec, std::vector<uint8_t>& key) {
    if (rec.size() < 56) return false;
    uint16_t attr_off = rd16(rec.data() + 20);
    size_t off = attr_off;
    while (off + 8 <= rec.size()) {
        uint32_t t = rd32(rec.data() + off);
        if (t == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + off + 4);
        if (len == 0 || off + len > rec.size()) break;
        const uint8_t* h = rec.data() + off;
        if (t == ATTR_FILE_NAME && (h[8] & 1) == 0) {
            uint32_t vlen = rd32(h + 16);
            uint32_t voff = rd32(h + 20);
            if (voff + vlen > len) return false;
            key.assign(h + voff, h + voff + vlen);
            return true;
        }
        off += len;
    }
    return false;
}

// Insert an index entry into a directory record's resident $INDEX_ROOT $I30.
// Returns false when the index is non-resident/large or won't fit (caller
// should fall back to export).
bool insertResidentIndexEntry(std::vector<uint8_t>& rec, uint64_t mft_ref,
                              const std::vector<uint8_t>& fn_key) {
    if (rec.size() < 56) return false;
    uint16_t attr_off = rd16(rec.data() + 20);
    size_t off = attr_off;
    while (off + 8 <= rec.size()) {
        uint32_t t = rd32(rec.data() + off);
        if (t == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + off + 4);
        if (len == 0 || off + len > rec.size()) break;
        const uint8_t* h = rec.data() + off;
        if (t == ATTR_INDEX_ROOT && (h[8] & 1) == 0 && h[9] == 4 &&
            std::memcmp(h + RES_HDR, "$I30", 4) == 0) {
            // Value: INDEX_ROOT (0x10) + INDEX_HEADER (0x10) + entries
            uint32_t vlen = rd32(h + 16);
            uint32_t voff = rd32(h + 20);
            if (voff + vlen > len) return false;
            uint8_t* val = const_cast<uint8_t*>(h) + voff;
            // Large index (flags & 1) uses $INDEX_ALLOCATION — not resident.
            uint32_t flags = rd32(val + 28);
            if (flags & 1) return false;
            uint32_t entries_off = rd32(val + 16);
            uint32_t idx_len = rd32(val + 20);
            if (entries_off == 0 || entries_off > vlen || idx_len > vlen)
                return false;
            // Find the END entry (flags & 2).
            size_t p = entries_off;
            size_t end_pos = p;
            while (p + 8 <= idx_len) {
                uint16_t esize = rd16(val + p + 8);
                uint16_t eflags = rd16(val + p + 12);
                if (esize == 0 || p + esize > idx_len) return false;
                if (eflags & 2) { end_pos = p; break; }
                p += esize;
            }
            // Build the new entry: 16-byte header + key, aligned to 8.
            uint32_t key_len = static_cast<uint32_t>(fn_key.size());
            uint32_t ent_size = (16 + key_len + 7) & ~7u;
            // Shift the END entry right by ent_size.
            size_t tail = idx_len - end_pos;
            if (end_pos + ent_size + tail + RES_HDR > rec.size()) return false;
            std::vector<uint8_t> tail_bytes(val + end_pos, val + idx_len);
            std::memmove(val + end_pos + ent_size, tail_bytes.data(), tail);
            uint8_t* e = val + end_pos;
            putU64(e + 0, mft_ref);
            putU16(e + 8, ent_size);
            putU16(e + 10, key_len);
            putU16(e + 12, 0);  // no node/end flag
            putU16(e + 14, 0);
            std::memcpy(e + 16, fn_key.data(), key_len);
            // Update index length; entries offset unchanged.
            uint32_t new_len = idx_len + ent_size;
            putU32(val + 20, new_len);
            // Update attribute value length + record used size.
            uint8_t* hdr_mut = const_cast<uint8_t*>(h);
            putU32(hdr_mut + 16, vlen + ent_size);
            putU32(hdr_mut + 4, len + ent_size);
            MFTRecordHeader* rh = reinterpret_cast<MFTRecordHeader*>(rec.data());
            rh->mr_used_size = static_cast<uint32_t>(rec.size() + ent_size);
            if (rh->mr_used_size > rec.size()) rh->mr_used_size = static_cast<uint32_t>(rec.size());
            return true;
        }
        off += len;
    }
    return false;
}

} // anonymous namespace

// ============================================================================
// Scan
// ============================================================================

std::vector<NtfsDeletedFile> scanDeletedNTFS(std::shared_ptr<DiskIO> disk,
                                             uint64_t start_sector) {
    std::vector<NtfsDeletedFile> out;
    if (!disk || !disk->isOpen()) return out;
    NtfsLayoutInfo layout;
    if (!readLayout(disk, start_sector, layout)) return out;

    // Bound the scan by the MFT's own size from record 0's $DATA.
    uint64_t total_records = 0;
    {
        auto rec0 = readRecord(disk, start_sector, layout, MFT_MFT);
        std::vector<std::pair<uint64_t, uint64_t>> runs;
        if (parseDataRuns(rec0, runs)) {
            uint64_t total_clusters = 0;
            for (auto& r : runs) total_clusters += r.second;
            total_records = total_clusters * layout.bpc / layout.mft_record_size;
        }
        if (total_records == 0) total_records = 64;  // conservative default
        if (total_records > 4 * 1024 * 1024 / layout.mft_record_size)
            total_records = 4 * 1024 * 1024 / layout.mft_record_size;
    }

    for (uint64_t num = 0; num < total_records; num++) {
        auto rec = readRecord(disk, start_sector, layout, num);
        if (rec.size() < 56) continue;
        uint32_t magic = rd32(rec.data());
        if (magic != MFT_RECORD_MAGIC) continue;
        uint16_t flags = rd16(rec.data() + 22);
        if (flags & MFT_RECORD_FLAG_IN_USE) continue;  // live record
        if (num < 16) continue;  // system records
        if (rd16(rec.data() + 6) < 2) continue;  // bad USA

        NtfsDeletedFile f;
        f.mft_record = num;
        f.sequence = rd16(rec.data() + 16);
        f.attrs = 0;
        std::string nm;
        uint64_t parent_ref = 0, dsize = 0;
        if (!parseFileName(rec, parent_ref, dsize, f.attrs, nm)) continue;
        f.name = nm;
        f.parent_ref = parent_ref;
        f.parent_record = parent_ref & 0xFFFFFFFFFFFFULL;
        f.data_size = dsize;
        f.is_dir = (f.attrs & FILE_ATTR_DIRECTORY) != 0;
        // Skip $-prefixed system names (e.g. leftover system records).
        if (!f.name.empty() && f.name[0] == '$') continue;
        // Skip names that are clearly not user data (dotted parents).
        if (f.name == "." || f.name == "..") continue;
        parseDataRuns(rec, f.runs);
        uint64_t alloc = 0;
        for (auto& r : f.runs) alloc += r.second * layout.bpc;
        f.alloc_size = alloc;
        out.push_back(std::move(f));
    }

    std::sort(out.begin(), out.end(), [](const NtfsDeletedFile& a,
                                         const NtfsDeletedFile& b) {
        if (a.parent_record != b.parent_record) return a.parent_record < b.parent_record;
        return a.name < b.name;
    });
    return out;
}

// ============================================================================
// Restore (in-place)
// ============================================================================

Result restoreDeletedNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const NtfsDeletedFile& file) {
    if (!disk || !disk->isOpen()) return Result::error("Disk not open");
    if (disk->isReadOnly()) return Result::error("Disk is read-only");
    NtfsLayoutInfo layout;
    if (!readLayout(disk, start_sector, layout))
        return Result::error("Not an NTFS volume");

    // 1. Verify the record is still deleted.
    auto rec = readRecord(disk, start_sector, layout, file.mft_record);
    if (rec.size() < 56) return Result::error("Cannot read MFT record");
    if (rd32(rec.data()) != MFT_RECORD_MAGIC)
        return Result::error("Record is no longer a valid FILE record");
    if (rd16(rec.data() + 22) & MFT_RECORD_FLAG_IN_USE)
        return Result::error("Record is no longer marked deleted");

    // 2. Verify all clusters are still free in $Bitmap.
    auto bmp = readBitmap(disk, start_sector, layout);
    if (bmp.empty()) return Result::error("Cannot read $Bitmap");
    for (auto& r : file.runs) {
        for (uint64_t c = r.first; c < r.first + r.second; c++) {
            if (!isClusterFree(bmp, c)) {
                return Result::error(
                    "Cluster " + std::to_string(c) +
                    " has been reallocated; file data was overwritten");
            }
        }
    }

    // 3. Rebuild the parent index entry (resident $INDEX_ROOT only).
    auto parent = readRecord(disk, start_sector, layout, file.parent_record);
    if (parent.size() < 56) return Result::error("Cannot read parent directory");
    std::vector<uint8_t> fn_key;
    if (!copyFileNameKey(rec, fn_key))
        return Result::error("Cannot extract $FILE_NAME key from record");
    uint64_t mft_ref = file.mft_record | (uint64_t(file.sequence) << 48);
    if (!insertResidentIndexEntry(parent, mft_ref, fn_key)) {
        return Result::error(
            "Parent directory uses a non-resident/large index; in-place "
            "restore unsupported here — use 'opm undelete --export <dir>' "
            "to recover the file data");
    }

    // 4. Mark clusters used in $Bitmap.
    for (auto& r : file.runs) {
        for (uint64_t c = r.first; c < r.first + r.second; c++) setClusterUsed(bmp, c);
    }
    uint64_t bmp_lcn = bitmapLcn(disk, start_sector, layout);
    Result r = disk->write(bmp.data(),
                           (start_sector + bmp_lcn * layout.spc) * 512, bmp.size());
    if (r.failed()) return Result::error("Failed to write $Bitmap: " + r.message);

    // 5. Write the parent directory record back (with the new index entry).
    uint64_t parent_off = (start_sector + layout.mft_lcn * layout.spc) * 512 +
                          file.parent_record * layout.mft_record_size;
    r = disk->write(parent.data(), parent_off, parent.size());
    if (r.failed()) return Result::error("Failed to write parent record: " + r.message);

    // 6. Set IN_USE on the deleted record (also bump sequence).
    uint16_t flags = rd16(rec.data() + 22);
    flags |= MFT_RECORD_FLAG_IN_USE;
    putU16(rec.data() + 22, flags);
    putU16(rec.data() + 16, file.sequence + 1);
    ntfs::fixupUpdateSequence(rec.data());
    uint64_t rec_off = (start_sector + layout.mft_lcn * layout.spc) * 512 +
                       file.mft_record * layout.mft_record_size;
    r = disk->write(rec.data(), rec_off, rec.size());
    if (r.failed()) return Result::error("Failed to write restored record: " + r.message);

    r = disk->flush();
    if (r.failed()) return Result::error("Failed to flush: " + r.message);
    return Result::ok();
}

// ============================================================================
// Export
// ============================================================================

Result exportDeletedNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                         const NtfsDeletedFile& file,
                         const std::string& out_dir) {
    if (!disk || !disk->isOpen()) return Result::error("Disk not open");
    NtfsLayoutInfo layout;
    if (!readLayout(disk, start_sector, layout))
        return Result::error("Not an NTFS volume");

    // Sanitize the output name.
    std::string safe;
    for (char c : file.name) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
            c == '_') safe += c;
        else safe += '_';
    }
    if (safe.empty()) safe = "file";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)file.mft_record);
    std::string path = out_dir + "/" + buf + "_" + safe;

    // Create the output directory if needed.
    std::string mkdir_cmd = "mkdir -p " + out_dir;  // via shell in CLI; here direct:
    (void)mkdir_cmd;
    struct stat st;
    if (stat(out_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        if (mkdir(out_dir.c_str(), 0755) != 0 && errno != EEXIST) {
            return Result::error("Cannot create output directory " + out_dir);
        }
    }

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return Result::error("Cannot open " + path + " for writing");

    // Write each run's clusters.
    uint64_t written = 0;
    for (auto& r : file.runs) {
        uint64_t bytes = r.second * layout.bpc;
        std::vector<uint8_t> buf2(bytes);
        if (disk->read(buf2.data(), (start_sector + r.first * layout.spc) * 512,
                       bytes).failed()) {
            std::fclose(f);
            return Result::error("Failed to read cluster run for export");
        }
        size_t take = file.data_size > written ? file.data_size - written : 0;
        if (take > buf2.size()) take = buf2.size();
        if (fwrite(buf2.data(), 1, take, f) != take) {
            std::fclose(f);
            return Result::error("Failed to write export file");
        }
        written += take;
        if (written >= file.data_size) break;
    }
    std::fclose(f);
    return Result::ok();
}

} // namespace ntfs
} // namespace opm
