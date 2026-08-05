#include "opm/convert_fs.hpp"
#include "opm/disk_io.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ntfs_impl.hpp"
#include "opm/partition_table.hpp"
#include "opm/filesystem.hpp"

#include <algorithm>
#include <cstring>
#include <memory>

namespace opm {

namespace {

using namespace ntfs;  // NTFS constants (ATTR_*, MFT_*, FILE_ATTR_*, flags)

// ============================================================================
// FAT32 tree walk
// ============================================================================

struct FatEntry {
    std::u16string name;          // UTF-16 long name (LFN); empty if none
    std::string name8;            // 8.3 short name
    std::string asciiName() const {
        if (!name.empty()) {
            std::string out;
            for (char16_t c : name) {
                if (c < 128) out += static_cast<char>(c);
                else out += '?';
            }
            return out;
        }
        std::string out = name8;
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out;
    }
    uint8_t attrs = 0;
    uint32_t first_cluster = 0;
    uint32_t size = 0;
    uint16_t crt_date = 0, crt_time = 0, crt_tenth = 0;
    uint16_t acc_date = 0;
    uint16_t wrt_date = 0, wrt_time = 0;
    std::vector<uint32_t> chain;
    std::vector<size_t> children;
    size_t parent = SIZE_MAX;
    uint64_t record = 0;
    bool is_dir = false;
    bool is_volume_label = false;
};

struct FatTree {
    std::vector<FatEntry> entries;
    size_t root = 0;
    std::string label;
};

uint32_t nextCluster(const std::vector<uint32_t>& fat, uint32_t cluster) {
    if (cluster < 2 || cluster >= fat.size()) return fat32::FAT32_EOC;
    uint32_t v = fat[cluster] & fat32::FAT32_EOC_MASK;
    if (v >= fat32::FAT32_RESERVED_START) return fat32::FAT32_EOC;
    return v;
}

std::vector<uint32_t> readChain(const std::vector<uint32_t>& fat,
                                uint32_t start) {
    std::vector<uint32_t> chain;
    if (start < 2) return chain;
    uint32_t c = start;
    for (size_t guard = 0; guard < fat.size() && c >= 2 && c < fat.size(); guard++) {
        chain.push_back(c);
        c = nextCluster(fat, c);
        if (c == fat32::FAT32_EOC) break;
    }
    return chain;
}

void appendLFNChars(std::u16string& out, const fat32::FAT32LFNEntry& lfn) {
    uint16_t b1[5], b2[6], b3[2];
    std::memcpy(b1, lfn.lfn_name1, sizeof(b1));
    std::memcpy(b2, lfn.lfn_name2, sizeof(b2));
    std::memcpy(b3, lfn.lfn_name3, sizeof(b3));
    const uint16_t* parts[3] = { b1, b2, b3 };
    const int counts[3] = { 5, 6, 2 };
    for (int p = 0; p < 3; p++) {
        for (int i = 0; i < counts[p]; i++) {
            uint16_t c = parts[p][i];
            if (c == 0x0000 || c == 0xFFFF) return;
            out.push_back(static_cast<char16_t>(c));
        }
    }
}

struct DirRecord {
    fat32::FAT32DirEntry e;
    std::vector<fat32::FAT32LFNEntry> lfns;
};

Result readDirCluster(const std::shared_ptr<DiskIO>& disk,
                      const fat32::FAT32Layout& layout, uint64_t start_sector,
                      const std::vector<uint32_t>& chain,
                      std::vector<DirRecord>& out) {
    std::vector<fat32::FAT32LFNEntry> pending_lfns;
    for (uint32_t cl : chain) {
        uint64_t sector = start_sector + layout.clusterToSector(cl);
        uint64_t bytes = static_cast<uint64_t>(layout.sectors_per_cluster) *
                         layout.bytes_per_sector;
        std::vector<uint8_t> buf(bytes);
        Result r = disk->read(buf.data(), sector * layout.bytes_per_sector, bytes);
        if (r.failed()) return r;
        const size_t count = bytes / sizeof(fat32::FAT32DirEntry);
        for (size_t i = 0; i < count; i++) {
            fat32::FAT32DirEntry e;
            std::memcpy(&e, buf.data() + i * sizeof(fat32::FAT32DirEntry),
                        sizeof(fat32::FAT32DirEntry));
            if (e.isUnused()) continue;
            if (e.isLFN()) {
                fat32::FAT32LFNEntry l;
                std::memcpy(&l, &e, sizeof(fat32::FAT32LFNEntry));
                pending_lfns.push_back(l);
                continue;
            }
            // Short entry: pair with the LFN sequence accumulated before it.
            DirRecord rec;
            rec.e = e;
            rec.lfns = pending_lfns;
            pending_lfns.clear();
            out.push_back(rec);
        }
    }
    return Result::ok();
}

FatEntry decodeEntry(const fat32::FAT32DirEntry& e,
                     const std::vector<fat32::FAT32LFNEntry>& lfns,
                     size_t parent_idx) {
    FatEntry out;
    out.attrs = e.dir_attr;
    out.first_cluster = e.getCluster();
    out.size = e.dir_file_size;
    out.crt_date = e.dir_crt_date;
    out.crt_time = e.dir_crt_time;
    out.crt_tenth = e.dir_crt_time_tenth;
    out.acc_date = e.dir_lst_acc_date;
    out.wrt_date = e.dir_wrt_date;
    out.wrt_time = e.dir_wrt_time;
    out.parent = parent_idx;
    out.is_dir = (e.dir_attr & fat32::ATTR_DIRECTORY) != 0;

    char name8[12];
    std::memcpy(name8, e.dir_name, 11);
    name8[11] = 0;
    out.name8 = name8;

    // Volume labels store the raw 11-byte name (no 8.3 split, no dot).
    if ((e.dir_attr & fat32::ATTR_VOLUME_ID) &&
        !(e.dir_attr & fat32::ATTR_DIRECTORY)) {
        std::string raw = out.name8;
        while (!raw.empty() && raw.back() == ' ') raw.pop_back();
        out.name8 = raw;
        return out;
    }

    size_t sp = 0;
    while (sp < 8 && out.name8[sp] != ' ') sp++;
    std::string stem = out.name8.substr(0, sp);
    size_t ex = 8;
    while (ex < 11 && out.name8[ex] != ' ') ex++;
    std::string ext = out.name8.substr(8, ex - 8);
    if (stem.empty()) stem = "_";
    if (!ext.empty()) stem += "." + ext;
    out.name8 = stem;

    if (!lfns.empty()) {
        for (auto it = lfns.rbegin(); it != lfns.rend(); ++it) {
            appendLFNChars(out.name, *it);
        }
        while (!out.name.empty() && out.name.back() == 0xFFFF) out.name.pop_back();
    }
    return out;
}

Result walkFAT32Tree(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     const fat32::FAT32Layout& layout,
                     const std::vector<uint32_t>& fat, FatTree& tree) {
    tree.entries.clear();

    FatEntry root;
    root.is_dir = true;
    root.name8 = "/";
    root.parent = SIZE_MAX;
    root.first_cluster = layout.root_cluster;
    root.chain = readChain(fat, layout.root_cluster);
    tree.entries.push_back(root);
    tree.root = 0;

    std::vector<size_t> pending{ tree.root };
    while (!pending.empty()) {
        size_t idx = pending.back();
        pending.pop_back();
        FatEntry& dir = tree.entries[idx];

        std::vector<DirRecord> recs;
        Result r = readDirCluster(disk, layout, start_sector, dir.chain, recs);
        if (r.failed()) return r;

        for (const auto& rec : recs) {
            const auto& e = rec.e;
            const auto& lfns = rec.lfns;
            if (e.isDeleted() || e.isUnused()) continue;
            if ((e.dir_attr & fat32::ATTR_VOLUME_ID) &&
                !(e.dir_attr & fat32::ATTR_DIRECTORY)) {
                FatEntry v = decodeEntry(e, lfns, idx);
                v.is_volume_label = true;
                tree.label = v.asciiName();
                continue;
            }
            if (e.dir_name[0] == '.' && !(e.dir_attr & fat32::ATTR_LONG_NAME)) {
                if (e.dir_name[1] == ' ' || e.dir_name[1] == '.') continue;
            }
            FatEntry child = decodeEntry(e, lfns, idx);
            child.chain = readChain(fat, child.first_cluster);
            tree.entries.push_back(child);
            size_t child_idx = tree.entries.size() - 1;
            tree.entries[idx].children.push_back(child_idx);
            if (child.is_dir) pending.push_back(child_idx);
        }
    }
    return Result::ok();
}

// ============================================================================
// NTFS record/attribute building helpers
// ============================================================================

constexpr size_t REC_SIZE = 1024;
constexpr size_t INDX_SIZE = 4096;
constexpr uint32_t RES_HDR = 24;   // resident attribute header
constexpr uint32_t NR_HDR = 64;    // non-resident attribute header
constexpr size_t MFT_HDR_SIZE = 56;  // record header up to first attribute

void put16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
void put32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF;
}
void put64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (v >> (8 * i)) & 0xFF;
}
uint16_t get16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t get32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24);
}
uint64_t get64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= uint64_t(p[i]) << (8 * i);
    return v;
}

// FAT date/time -> Unix seconds (FAT local time treated as UTC; deterministic).
int64_t fatDateTimeToUnix(uint16_t date, uint16_t time, uint8_t tenths = 0) {
    int year = 1980 + ((date >> 9) & 0x7F);
    int month = (date >> 5) & 0x0F;
    int day = date & 0x1F;
    int hour = (time >> 11) & 0x1F;
    int minute = (time >> 5) & 0x3F;
    int second = (time & 0x1F) * 2;
    (void)tenths;
    if (month < 1 || month > 12 || day < 1 || day > 31) return 0;
    if (hour > 23 || minute > 59 || second > 59) return 0;

    int64_t y = year;
    int64_t m = month;
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = era * 146097 + doe - 719468;
    return days * 86400 + hour * 3600 + minute * 60 + second;
}

uint64_t unixToFileTime(int64_t unix) {
    return (static_cast<uint64_t>(unix) + 11644473600ULL) * 10000000ULL;
}

uint64_t fatTimeToFileTime(uint16_t date, uint16_t time, uint8_t tenths = 0) {
    return unixToFileTime(fatDateTimeToUnix(date, time, tenths));
}

std::vector<uint8_t> utf16le(const std::u16string& s) {
    std::vector<uint8_t> out;
    for (char16_t c : s) {
        out.push_back(c & 0xFF);
        out.push_back((c >> 8) & 0xFF);
    }
    return out;
}

struct Run {
    int64_t lcn;
    uint64_t len;
};

std::vector<uint8_t> encodeRuns(const std::vector<Run>& runs) {
    std::vector<uint8_t> out;
    int64_t prev = 0;
    for (const Run& r : runs) {
        int64_t offset = r.lcn - prev;
        int llen = 1;
        uint64_t len = r.len;
        while ((len >> (8 * llen)) != 0 && llen < 4) llen++;
        int olen = 1;
        int64_t o = offset;
        if (o < 0) {
            int64_t all = -1;
            for (int i = 1; i < 8; i++) {
                if ((o >> (8 * i)) != all) olen = i + 1;
            }
        } else {
            for (int i = 1; i < 8; i++) {
                if ((o >> (8 * i)) != 0) olen = i + 1;
            }
        }
        out.push_back(static_cast<uint8_t>((llen << 4) | olen));
        for (int i = 0; i < llen; i++) out.push_back((len >> (8 * i)) & 0xFF);
        for (int i = 0; i < olen; i++) out.push_back((o >> (8 * i)) & 0xFF);
        prev = r.lcn;
    }
    out.push_back(0x00);
    return out;
}

void addResident(std::vector<uint8_t>& rec, uint32_t type, uint16_t id,
                 const uint8_t* value, uint32_t value_len,
                 const char* name = nullptr) {
    size_t off = rec.size();
    uint8_t nl = name ? static_cast<uint8_t>(std::strlen(name)) : 0;
    uint32_t total = RES_HDR + nl + value_len;
    rec.resize(off + total, 0);
    uint8_t* h = rec.data() + off;
    put32(h + 0, type);
    put32(h + 4, total);
    h[8] = 0;                       // resident
    h[9] = nl;
    put16(h + 10, nl ? RES_HDR : 0); // name offset
    put16(h + 12, 0);                // flags
    put16(h + 14, id);
    put32(h + 16, value_len);        // value length
    put16(h + 20, static_cast<uint16_t>(RES_HDR + nl)); // value offset
    h[22] = 0;                       // ra_flags
    h[23] = 0;
    if (nl) std::memcpy(h + RES_HDR, name, nl);
    if (value_len) std::memcpy(h + RES_HDR + nl, value, value_len);
}

void addNonResident(std::vector<uint8_t>& rec, uint32_t type, uint16_t id,
                    const std::vector<Run>& runs, uint64_t alloc_size,
                    uint64_t data_size, const char* name = nullptr) {
    std::vector<uint8_t> runs_buf = encodeRuns(runs);
    size_t off = rec.size();
    uint8_t nl = name ? static_cast<uint8_t>(std::strlen(name)) : 0;
    uint32_t total = NR_HDR + nl + static_cast<uint32_t>(runs_buf.size());
    rec.resize(off + total, 0);
    uint8_t* h = rec.data() + off;
    put32(h + 0, type);
    put32(h + 4, total);
    h[8] = 1;                       // non-resident
    h[9] = nl;
    put16(h + 10, nl ? NR_HDR : 0); // name offset
    put16(h + 12, 0);
    put16(h + 14, id);
    put64(h + 16, 0);               // start VCN
    uint64_t total_clusters = 0;
    for (const Run& r : runs) total_clusters += r.len;
    put64(h + 24, total_clusters == 0 ? 0 : total_clusters - 1);  // end VCN
    put16(h + 32, static_cast<uint16_t>(NR_HDR + nl)); // run list offset
    put16(h + 34, 0);               // compression unit
    put32(h + 36, 0);
    put64(h + 40, alloc_size);
    put64(h + 48, data_size);
    put64(h + 56, data_size);       // initialized size
    if (nl) std::memcpy(h + NR_HDR, name, nl);
    std::memcpy(h + NR_HDR + nl, runs_buf.data(), runs_buf.size());
}

std::vector<Run> chainToRuns(const std::vector<uint32_t>& chain,
                             uint64_t data_start_lcn) {
    std::vector<Run> runs;
    size_t i = 0;
    while (i < chain.size()) {
        uint64_t lcn = data_start_lcn + (chain[i] - 2);
        size_t j = i + 1;
        while (j < chain.size() && chain[j] == chain[j - 1] + 1) j++;
        runs.push_back(Run{ static_cast<int64_t>(lcn), j - i });
        i = j;
    }
    return runs;
}

struct MFTRecordBuf {
    std::vector<uint8_t> bytes;
    uint64_t rec_num;
    uint16_t rec_flags;
    MFTRecordBuf(uint64_t num, uint16_t fl) : rec_num(num), rec_flags(fl) {
        // Start with just the header; attributes are appended sequentially
        // from mr_attr_offset. The record is padded to REC_SIZE in finish().
        bytes.assign(MFT_HDR_SIZE, 0);
        ntfs::MFTRecordHeader* h =
            reinterpret_cast<ntfs::MFTRecordHeader*>(bytes.data());
        ntfs::initMFTRecord(*h, num, false);
        h->mr_flags = fl;
        h->mr_usn_offset = 48;
        h->mr_usn_size = 3;
        h->mr_alloc_size = static_cast<uint32_t>(REC_SIZE);
        h->mr_used_size = h->mr_attr_offset;
    }
    void appendAttr(uint32_t type, uint16_t id, const uint8_t* v, uint32_t len,
                    const char* name = nullptr) {
        addResident(bytes, type, id, v, len, name);
    }
    void appendAttrNR(uint32_t type, uint16_t id, const std::vector<Run>& runs,
                      uint64_t alloc, uint64_t data, const char* name = nullptr) {
        addNonResident(bytes, type, id, runs, alloc, data, name);
    }
    void finish() {
        size_t off = bytes.size();
        if (off + 8 > REC_SIZE) {
            off = REC_SIZE - 8;
        }
        bytes.resize(off + 8, 0);
        put32(bytes.data() + off, 0xFFFFFFFFu);
        put32(bytes.data() + off + 4, 8);
        ntfs::MFTRecordHeader* h =
            reinterpret_cast<ntfs::MFTRecordHeader*>(bytes.data());
        h->mr_used_size = static_cast<uint32_t>(off + 8);
        h->mr_attr_offset = 56;
        // Pad to the full record size: fixupUpdateSequence walks sectors
        // according to mr_alloc_size and needs the whole record in memory.
        if (bytes.size() < REC_SIZE) bytes.resize(REC_SIZE, 0);
        ntfs::fixupUpdateSequence(bytes.data());
    }
};

std::vector<uint8_t> stdInfoValue(const FatEntry& e, uint32_t ntfs_attrs) {
    std::vector<uint8_t> v(72, 0);
    uint64_t c = fatTimeToFileTime(e.crt_date, e.crt_time, e.crt_tenth);
    uint64_t w = fatTimeToFileTime(e.wrt_date, e.wrt_time);
    uint64_t a = fatTimeToFileTime(e.acc_date, 0);
    put64(v.data() + 0, c);
    put64(v.data() + 8, w);
    put64(v.data() + 16, w);
    put64(v.data() + 24, a);
    put32(v.data() + 32, ntfs_attrs);
    return v;
}

std::vector<uint8_t> fileNameValue(const FatEntry& e, uint64_t parent_ref,
                                   uint64_t alloc_size, uint64_t data_size,
                                   uint32_t ntfs_attrs, uint8_t name_type) {
    std::u16string nm = e.name.empty()
        ? std::u16string(e.name8.begin(), e.name8.end())
        : e.name;
    if (nm.empty()) nm = u"_";
    std::vector<uint8_t> v(66 + nm.size() * 2, 0);
    put64(v.data() + 0, parent_ref);
    uint64_t c = fatTimeToFileTime(e.crt_date, e.crt_time, e.crt_tenth);
    uint64_t w = fatTimeToFileTime(e.wrt_date, e.wrt_time);
    uint64_t a = fatTimeToFileTime(e.acc_date, 0);
    put64(v.data() + 8, c);
    put64(v.data() + 16, w);
    put64(v.data() + 24, w);
    put64(v.data() + 32, a);
    put64(v.data() + 40, alloc_size);
    put64(v.data() + 48, data_size);
    put32(v.data() + 56, ntfs_attrs);
    put32(v.data() + 60, 0);
    v[64] = static_cast<uint8_t>(nm.size());
    v[65] = name_type;
    std::vector<uint8_t> u = utf16le(nm);
    std::memcpy(v.data() + 66, u.data(), u.size());
    return v;
}

struct IdxEntry {
    uint64_t mft_ref;
    std::vector<uint8_t> key;
    uint64_t child_vcn = 0;
    bool is_node = false;
    bool is_end = false;
};

std::vector<uint8_t> buildIndexValue(const std::vector<IdxEntry>& entries,
                                     uint32_t* index_len_out, bool large) {
    // INDEX_ROOT value layout:
    //   0x00  indexed attribute type (4) = $FILE_NAME
    //   0x04  collation rule (4) = 1 (filename)
    //   0x08  index block size (4)
    //   0x0C  clusters per index block (1) + padding (3)
    //   0x10  INDEX_HEADER: entries offset (4) = 0x20
    //   0x14  index length (4)
    //   0x18  allocated size (4)
    //   0x1C  flags (4): 0 = small index, 1 = large index
    //   0x20  entries...
    std::vector<uint8_t> v;
    v.resize(0x20, 0);
    put32(v.data() + 0, 0x30);        // indexed attribute = $FILE_NAME
    put32(v.data() + 4, 1);           // collation rule (filename)
    put32(v.data() + 8, INDX_SIZE);   // index block size
    v[12] = 0;                        // clusters per index block
    put32(v.data() + 16, 0x20);       // entries offset
    put32(v.data() + 20, 0x20);       // index length (updated below)
    put32(v.data() + 24, INDX_SIZE - 0x20);
    put32(v.data() + 28, large ? 1u : 0u);

    for (const IdxEntry& ie : entries) {
        size_t off = v.size();
        uint32_t key_len = static_cast<uint32_t>(ie.key.size());
        uint32_t ent_size = 16 + key_len;
        if (ie.is_node) ent_size += 8;
        ent_size = (ent_size + 7) & ~7u;
        v.resize(off + ent_size, 0);
        put64(v.data() + off, ie.mft_ref);
        put16(v.data() + off + 8, ent_size);
        put16(v.data() + off + 10, key_len);
        uint16_t fl = 0;
        if (ie.is_node) fl |= INDEX_ENTRY_NODE;
        if (ie.is_end) fl |= INDEX_ENTRY_END;
        put16(v.data() + off + 12, fl);
        put16(v.data() + off + 14, 0);
        std::memcpy(v.data() + off + 16, ie.key.data(), key_len);
        if (ie.is_node) {
            put64(v.data() + off + 16 + key_len, ie.child_vcn);
        }
    }
    // Terminator
    {
        size_t off = v.size();
        uint32_t ent_size = 16;
        v.resize(off + ent_size, 0);
        put64(v.data() + off, 0);
        put16(v.data() + off + 8, ent_size);
        put16(v.data() + off + 10, 0);
        put16(v.data() + off + 12, INDEX_ENTRY_END);
        put16(v.data() + off + 14, 0);
    }
    put32(v.data() + 20, static_cast<uint32_t>(v.size()));  // index length
    if (index_len_out) *index_len_out = static_cast<uint32_t>(v.size());
    return v;
}

// Build one INDX record buffer (4096 bytes).
std::vector<uint8_t> buildIndxRecord(const std::vector<IdxEntry>& entries,
                                     uint64_t vcn, bool is_last, uint64_t next_vcn) {
    std::vector<uint8_t> indx(INDX_SIZE, 0);
    put32(indx.data() + 0, 0x58444E49);  // "INDX"
    put16(indx.data() + 4, 40);          // USA offset
    put16(indx.data() + 6, 9);           // USA count
    put64(indx.data() + 8, 0);           // LSN
    put64(indx.data() + 16, vcn);        // VCN of this buffer

    std::vector<IdxEntry> ents = entries;
    if (!is_last) {
        ents.back().is_node = true;
        ents.back().child_vcn = next_vcn;
    }
    uint32_t ilen = 0;
    std::vector<uint8_t> iv = buildIndexValue(ents, &ilen, true);
    std::memcpy(indx.data() + 24, iv.data(), ilen);

    uint16_t usn = static_cast<uint16_t>(0x1000u + (vcn * 257u) % 0xEFFFu);
    put16(indx.data() + 40, usn);
    for (uint32_t s = 1; s < 8; s++) {
        uint8_t* tail = indx.data() + s * 512 - 2;
        put16(indx.data() + 40 + s * 2, get16(tail));
        tail[0] = usn & 0xFF;
        tail[1] = (usn >> 8) & 0xFF;
    }
    return indx;
}

// ============================================================================
// Conversion plan
// ============================================================================

struct ConvertPlan {
    uint32_t spc = 1;
    uint64_t total_sectors = 0;
    uint64_t total_clusters = 0;
    uint64_t data_start_lcn = 0;
    uint64_t mft_lcn = 0;
    uint64_t mft_mirr_lcn = 0;
    uint32_t mft_record_size = REC_SIZE;
    uint32_t index_record_size = INDX_SIZE;
    uint64_t serial = 0;
    uint64_t mft_clusters = 0;
    uint64_t bitmap_clusters = 0;
    uint64_t log_clusters = 0;
    uint64_t upcase_clusters = 0;
    uint64_t mirror_clusters = 0;
    uint64_t indx_clusters = 0;      // precomputed INDX buffer region size
    uint64_t indx_used = 0;          // running cursor while building records
    std::vector<uint8_t> bitmap;
};

// Estimate index-entry bytes for a child (FILE_NAME key + entry header).
size_t indexEntryBytes(const FatEntry& e, uint64_t bpc) {
    std::u16string nm = e.name.empty()
        ? std::u16string(e.name8.begin(), e.name8.end())
        : e.name;
    if (nm.empty()) nm = u"_";
    size_t key = 66 + nm.size() * 2;
    return ((16 + key) + 7) & ~7u;
}

} // anonymous namespace

// ============================================================================
// Public: detect filesystem
// ============================================================================

FileSystemType detectFilesystemAt(std::shared_ptr<DiskIO> disk,
                                  uint64_t start_sector) {
    if (!disk || !disk->isOpen()) return FileSystemType::Unknown;
    uint8_t boot[512];
    Result r = disk->readSector(boot, start_sector);
    if (r.failed()) return FileSystemType::Unknown;
    if (boot[510] == 0x55 && boot[511] == 0xAA) {
        if (std::memcmp(boot + 3, "NTFS    ", 8) == 0) return FileSystemType::NTFS;
        if (std::memcmp(boot + 3, "EXFAT   ", 8) == 0) return FileSystemType::exFAT;
        if (std::memcmp(boot + 82, "FAT32   ", 8) == 0) return FileSystemType::FAT32;
        if (std::memcmp(boot + 54, "FAT16   ", 8) == 0 ||
            std::memcmp(boot + 54, "FAT12   ", 8) == 0) {
            return FileSystemType::FAT16;
        }
    }
    uint8_t sb[2];
    r = disk->read(sb, (start_sector + 2) * 512 + 0x38, 2);
    if (r.success() && sb[0] == 0x53 && sb[1] == 0xEF) {
        return FileSystemType::EXT4;
    }
    return FileSystemType::Unknown;
}

// ============================================================================
// Public: FAT32 -> NTFS conversion
// ============================================================================

Result convertFAT32ToNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          uint64_t size_bytes, const std::string& label) {
    if (!disk || !disk->isOpen()) return Result::error("Disk not open");
    if (disk->isReadOnly()) return Result::error("Disk is read-only; open read-write");

    // Guard: only FAT32 volumes are convertible. Detect before parsing so a
    // foreign filesystem (ext4, NTFS, swap...) fails cleanly instead of being
    // misread as a corrupt FAT32.
    FileSystemType fs = detectFilesystemAt(disk, start_sector);
    if (fs != FileSystemType::FAT32) {
        return Result::error("region is not FAT32 (found " +
                             getFilesystemName(fs) + ")");
    }

    // --- 1. Parse FAT32 ---
    fat32::FAT32BootSector boot;
    fat32::FAT32Layout layout;
    Result r = fat32::getFAT32Info(disk, start_sector, boot, layout);
    if (r.failed()) return Result::error("Not a FAT32 volume: " + r.message);

    std::vector<uint32_t> fat;
    r = fat32::readFATTable(disk, start_sector, layout, 0, fat);
    if (r.failed()) return Result::error("Failed to read FAT table: " + r.message);

    // --- 2. Walk the tree ---
    FatTree tree;
    r = walkFAT32Tree(disk, start_sector, layout, fat, tree);
    if (r.failed()) return Result::error("Failed to read directory tree: " + r.message);

    // --- 3. Layout: NTFS cluster size = FAT32 cluster size ---
    ConvertPlan plan;
    plan.spc = layout.sectors_per_cluster;
    plan.total_sectors = size_bytes / layout.bytes_per_sector;
    plan.total_clusters = plan.total_sectors / plan.spc;
    if (plan.total_clusters < 64) {
        return Result::error("Volume too small to convert (needs >= 64 clusters)");
    }
    if (layout.data_start_sector % plan.spc != 0) {
        return Result::error("FAT32 data region is not cluster-aligned; "
                             "conversion unsupported for this layout");
    }
    plan.data_start_lcn = layout.data_start_sector / plan.spc;
    plan.serial = ntfs::generateNTFSSerial();

    const uint64_t bpc = static_cast<uint64_t>(plan.spc) * 512;

    size_t user_records = 0;
    for (const auto& e : tree.entries) {
        if (!e.is_volume_label) user_records++;
    }
    const uint64_t total_records = 16 + user_records;
    plan.mft_clusters = (total_records * plan.mft_record_size + bpc - 1) / bpc;
    if (plan.mft_clusters < 1) plan.mft_clusters = 1;

    plan.bitmap_clusters = (((plan.total_clusters + 7) / 8) + bpc - 1) / bpc;
    if (plan.bitmap_clusters < 1) plan.bitmap_clusters = 1;

    uint64_t log_bytes = 4ULL * 1024 * 1024;
    if (size_bytes < 100ULL * 1024 * 1024) log_bytes = 1ULL * 1024 * 1024;
    plan.log_clusters = log_bytes / bpc;
    if (plan.log_clusters < 1) plan.log_clusters = 1;

    plan.upcase_clusters = (65536 * 2 + bpc - 1) / bpc;
    if (plan.upcase_clusters < 1) plan.upcase_clusters = 1;

    plan.mirror_clusters = (4 * plan.mft_record_size + bpc - 1) / bpc;
    if (plan.mirror_clusters < 1) plan.mirror_clusters = 1;

    // --- INDX buffer pre-sizing (large directories) ---
    uint64_t per_buf = (INDX_SIZE - 64) / 100;  // conservative per-entry bytes
    if (per_buf < 1) per_buf = 1;
    plan.indx_clusters = 0;
    for (const auto& e : tree.entries) {
        if (e.is_volume_label || !e.is_dir) continue;
        size_t idx_val_len = 32;  // index headers
        for (size_t child : e.children) {
            if (tree.entries[child].is_volume_label) continue;
            idx_val_len += indexEntryBytes(tree.entries[child], bpc) + 8;
        }
        idx_val_len += 16;  // terminator
        if (idx_val_len > REC_SIZE - 300) {
            uint64_t nbuf = (e.children.size() + per_buf - 1) / per_buf;
            if (nbuf == 0) nbuf = 1;
            plan.indx_clusters += (nbuf * INDX_SIZE + bpc - 1) / bpc;
        }
    }

    uint64_t metadata_clusters = plan.mft_clusters + plan.bitmap_clusters +
                                 plan.log_clusters + plan.upcase_clusters +
                                 plan.mirror_clusters + plan.indx_clusters;

    // --- 4. Find free space ---
    std::vector<uint8_t> used(plan.total_clusters, 0);
    auto markUsed = [&](uint64_t c) {
        if (c < plan.total_clusters) used[c] = 1;
    };
    for (const auto& e : tree.entries) {
        if (e.is_volume_label) continue;
        for (uint32_t cl : e.chain) markUsed(plan.data_start_lcn + (cl - 2));
    }

    uint64_t mft_lcn = 0;
    // Candidate A: old reserved+FAT region [0, data_start_lcn) — always free.
    if (plan.data_start_lcn >= metadata_clusters) {
        mft_lcn = 0;
    } else {
        // Candidate B: largest contiguous free run in the data region.
        uint64_t best_start = 0, best_len = 0;
        uint64_t run_start = 0, run_len = 0;
        for (uint64_t c = 0; c < plan.total_clusters; c++) {
            if (used[c] == 0) {
                if (run_len == 0) run_start = c;
                run_len++;
            } else {
                if (run_len > best_len) { best_len = run_len; best_start = run_start; }
                run_len = 0;
            }
        }
        if (run_len > best_len) { best_len = run_len; best_start = run_start; }
        if (best_len < metadata_clusters) {
            return Result::error(
                "Not enough contiguous free space for NTFS metadata (need " +
                std::to_string(metadata_clusters) + " clusters, largest free run is " +
                std::to_string(best_len) + "). Free space or defragment first.");
        }
        mft_lcn = best_start;
    }
    plan.mft_lcn = mft_lcn;

    // --- 5. Assign MFT record numbers (parents before children) ---
    uint64_t next_record = 16;
    tree.entries[tree.root].record = ntfs::MFT_ROOT;
    std::vector<size_t> order{ tree.root };
    for (size_t i = 0; i < order.size(); i++) {
        for (size_t child : tree.entries[order[i]].children) {
            FatEntry& ce = tree.entries[child];
            if (ce.is_volume_label) continue;
            ce.record = next_record++;
            order.push_back(child);
        }
    }

    // --- 6. Bitmap: metadata + boot region + file data clusters ---
    plan.bitmap.assign((plan.total_clusters + 7) / 8, 0);
    auto bitSet = [&](uint64_t c) {
        if (c < plan.total_clusters) {
            plan.bitmap[c / 8] |= static_cast<uint8_t>(1 << (c % 8));
        }
    };
    // Boot region: first 16 sectors hold the NTFS boot sector + $Boot data.
    {
        uint64_t boot_clusters = 16 / plan.spc;
        if (boot_clusters < 1) boot_clusters = 1;
        for (uint64_t c = 0; c < boot_clusters; c++) bitSet(c);
    }
    for (uint64_t c = mft_lcn; c < mft_lcn + metadata_clusters; c++) bitSet(c);
    for (const auto& e : tree.entries) {
        if (e.is_volume_label) continue;
        for (uint32_t cl : e.chain) bitSet(plan.data_start_lcn + (cl - 2));
    }

    // --- 7. Build MFT records ---
    std::vector<std::vector<uint8_t>> records(total_records,
                                              std::vector<uint8_t>(REC_SIZE, 0));

    auto sysTimestamp = []() {
        FatEntry p;
        p.crt_date = p.wrt_date = p.acc_date = 0x4A21;  // 2020-01-01
        p.crt_time = p.wrt_time = 0;
        return p;
    };

    // $MFT (0)
    {
        MFTRecordBuf rec(ntfs::MFT_MFT, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        std::u16string nm = u"$MFT";
        FatEntry pn = sysTimestamp();
        pn.name = nm;
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        std::vector<Run> runs{ Run{ static_cast<int64_t>(mft_lcn), plan.mft_clusters } };
        rec.appendAttrNR(ATTR_DATA, 0x80, runs,
                         plan.mft_clusters * bpc, plan.mft_clusters * bpc);
        rec.finish();
        records[0] = rec.bytes;
    }

    // $MFTMirr (1)
    uint64_t mirr_lcn = mft_lcn + plan.mft_clusters + plan.bitmap_clusters +
                        plan.log_clusters + plan.upcase_clusters + plan.indx_clusters;
    plan.mft_mirr_lcn = mirr_lcn;
    {
        MFTRecordBuf rec(ntfs::MFT_MIRR, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$MFTMirr";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        std::vector<Run> runs{ Run{ static_cast<int64_t>(mirr_lcn), plan.mirror_clusters } };
        rec.appendAttrNR(ATTR_DATA, 0x80, runs,
                         plan.mirror_clusters * bpc, plan.mirror_clusters * bpc);
        rec.finish();
        records[1] = rec.bytes;
    }

    // $LogFile (2)
    {
        MFTRecordBuf rec(ntfs::MFT_LOGFILE, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$LogFile";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        std::vector<Run> runs{ Run{ static_cast<int64_t>(mft_lcn + plan.mft_clusters +
                                                         plan.bitmap_clusters),
                                    plan.log_clusters } };
        rec.appendAttrNR(ATTR_DATA, 0x80, runs,
                         plan.log_clusters * bpc, plan.log_clusters * bpc);
        rec.finish();
        records[2] = rec.bytes;
    }

    // $Volume (3)
    {
        MFTRecordBuf rec(ntfs::MFT_VOLUME, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$Volume";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        uint8_t vi[12] = { 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        rec.appendAttr(ATTR_VOLUME_INFO, 0x70, vi, 12);
        std::string lbl = label.empty() ? tree.label : label;
        if (lbl.size() > 32) lbl = lbl.substr(0, 32);
        std::u16string vl(lbl.begin(), lbl.end());
        std::vector<uint8_t> vn = utf16le(vl);
        rec.appendAttr(ATTR_VOLUME_NAME, 0x60, vn.data(), static_cast<uint32_t>(vn.size()));
        rec.finish();
        records[3] = rec.bytes;
    }

    // $AttrDef (4)
    {
        MFTRecordBuf rec(ntfs::MFT_ATTRDEF, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$AttrDef";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        rec.appendAttr(ATTR_DATA, 0x80, nullptr, 0);
        rec.finish();
        records[4] = rec.bytes;
    }

    // $Root (5)
    {
        MFTRecordBuf rec(ntfs::MFT_ROOT, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_DIR |
                                           MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u".";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_ROOT | (1ULL << 48), 0, 0,
                                                FILE_ATTR_DIRECTORY, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        std::vector<IdxEntry> idx;
        for (size_t child : tree.entries[tree.root].children) {
            const FatEntry& ce = tree.entries[child];
            if (ce.is_volume_label) continue;
            IdxEntry ie;
            ie.mft_ref = ce.record | (1ULL << 48);
            uint64_t alloc = ce.is_dir ? 0 : ((static_cast<uint64_t>(ce.size) + bpc - 1) / bpc) * bpc;
            FatEntry ce2 = ce;
            ie.key = fileNameValue(ce2, tree.entries[ce.parent].record | (1ULL << 48),
                                   alloc, ce.size,
                                   ce.is_dir ? FILE_ATTR_DIRECTORY : 0, 3);
            idx.push_back(ie);
        }
        uint32_t ilen = 0;
        std::vector<uint8_t> iv = buildIndexValue(idx, &ilen, false);
        rec.appendAttr(ATTR_INDEX_ROOT, 0x90, iv.data(),
                       static_cast<uint32_t>(iv.size()), "$I30");
        rec.finish();
        records[5] = rec.bytes;
    }

    // $Bitmap (6)
    {
        MFTRecordBuf rec(ntfs::MFT_BITMAP, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$Bitmap";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        std::vector<Run> runs{ Run{ static_cast<int64_t>(mft_lcn + plan.mft_clusters),
                                    plan.bitmap_clusters } };
        rec.appendAttrNR(ATTR_DATA, 0x80, runs,
                         plan.bitmap_clusters * bpc, plan.bitmap_clusters * bpc);
        rec.finish();
        records[6] = rec.bytes;
    }

    // $Boot (7)
    {
        MFTRecordBuf rec(ntfs::MFT_BOOT, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$Boot";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        uint64_t boot_clusters = 16 / plan.spc;
        if (boot_clusters < 1) boot_clusters = 1;
        std::vector<Run> runs{ Run{ 0, boot_clusters } };
        rec.appendAttrNR(ATTR_DATA, 0x80, runs, boot_clusters * bpc, 8192);
        rec.finish();
        records[7] = rec.bytes;
    }

    // $BadClus (8)
    {
        MFTRecordBuf rec(ntfs::MFT_BADCLUS, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$BadClus";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        rec.appendAttr(ATTR_DATA, 0x80, nullptr, 0);
        rec.finish();
        records[8] = rec.bytes;
    }

    // $Secure (9)
    {
        MFTRecordBuf rec(ntfs::MFT_SECURE, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$Secure";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        rec.appendAttr(ATTR_DATA, 0x80, nullptr, 0);
        rec.finish();
        records[9] = rec.bytes;
    }

    // $UpCase (10)
    {
        MFTRecordBuf rec(ntfs::MFT_UPCASE, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$UpCase";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_MFT, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        std::vector<Run> runs{ Run{ static_cast<int64_t>(mft_lcn + plan.mft_clusters +
                                                         plan.bitmap_clusters +
                                                         plan.log_clusters),
                                    plan.upcase_clusters } };
        rec.appendAttrNR(ATTR_DATA, 0x80, runs,
                         plan.upcase_clusters * bpc, plan.upcase_clusters * bpc);
        rec.finish();
        records[10] = rec.bytes;
    }

    // $Extend (11)
    {
        MFTRecordBuf rec(ntfs::MFT_EXTEND, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_DIR |
                                            MFT_RECORD_FLAG_SYSTEM);
        FatEntry p = sysTimestamp();
        std::vector<uint8_t> si = stdInfoValue(p, FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));
        FatEntry pn = sysTimestamp();
        pn.name = u"$Extend";
        std::vector<uint8_t> fn = fileNameValue(pn, ntfs::MFT_EXTEND, 0, 0,
                                                FILE_ATTR_SYSTEM | FILE_ATTR_HIDDEN, 1);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));
        std::vector<IdxEntry> idx;
        uint32_t ilen = 0;
        std::vector<uint8_t> iv = buildIndexValue(idx, &ilen, false);
        rec.appendAttr(ATTR_INDEX_ROOT, 0x90, iv.data(),
                       static_cast<uint32_t>(iv.size()), "$I30");
        rec.finish();
        records[11] = rec.bytes;
    }

    // Records 12..15: in use, minimal.
    for (uint64_t i = 12; i < 16; i++) {
        MFTRecordBuf rec(i, MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_SYSTEM);
        rec.finish();
        records[i] = rec.bytes;
    }

    // --- INDX buffers for large dirs (precompute content, write after MFT) ---
    struct IndxWrite {
        uint64_t start_lcn;
        std::vector<uint8_t> content;  // contiguous cluster region
    };
    std::vector<IndxWrite> indx_writes;
    plan.indx_used = 0;

    // --- User records ---
    for (const auto& e : tree.entries) {
        if (e.is_volume_label) continue;
        if (e.record < 16) continue;  // root handled above
        MFTRecordBuf rec(e.record,
                         e.is_dir ? (MFT_RECORD_FLAG_IN_USE | MFT_RECORD_FLAG_DIR)
                                  : MFT_RECORD_FLAG_IN_USE);
        uint32_t ntfs_attrs = 0;
        if (e.attrs & fat32::ATTR_READ_ONLY) ntfs_attrs |= FILE_ATTR_READONLY;
        if (e.attrs & fat32::ATTR_HIDDEN) ntfs_attrs |= FILE_ATTR_HIDDEN;
        if (e.attrs & fat32::ATTR_SYSTEM) ntfs_attrs |= FILE_ATTR_SYSTEM;
        if (e.attrs & fat32::ATTR_ARCHIVE) ntfs_attrs |= FILE_ATTR_ARCHIVE;
        if (e.is_dir) ntfs_attrs |= FILE_ATTR_DIRECTORY;

        std::vector<uint8_t> si = stdInfoValue(e, ntfs_attrs);
        rec.appendAttr(ATTR_STANDARD_INFO, 0x10, si.data(), static_cast<uint32_t>(si.size()));

        uint64_t parent_ref = (e.parent == SIZE_MAX)
            ? (ntfs::MFT_ROOT | (1ULL << 48))
            : (tree.entries[e.parent].record | (1ULL << 48));
        uint64_t alloc_size = e.is_dir ? 0 : ((static_cast<uint64_t>(e.size) + bpc - 1) / bpc) * bpc;
        std::vector<uint8_t> fn = fileNameValue(e, parent_ref, alloc_size, e.size,
                                                ntfs_attrs, e.name.empty() ? 1 : 3);
        rec.appendAttr(ATTR_FILE_NAME, 0x30, fn.data(), static_cast<uint32_t>(fn.size()));

        if (e.is_dir) {
            std::vector<IdxEntry> idx;
            for (size_t child : e.children) {
                const FatEntry& ce = tree.entries[child];
                if (ce.is_volume_label) continue;
                IdxEntry ie;
                ie.mft_ref = ce.record | (1ULL << 48);
                uint64_t c_alloc = ce.is_dir ? 0
                    : ((static_cast<uint64_t>(ce.size) + bpc - 1) / bpc) * bpc;
                FatEntry ce2 = ce;
                ie.key = fileNameValue(ce2, ce.record | (1ULL << 48), c_alloc, ce.size,
                                       ce.is_dir ? FILE_ATTR_DIRECTORY : 0, 3);
                idx.push_back(ie);
            }
            size_t idx_val_len = 32;
            for (const auto& ie : idx) {
                idx_val_len += 16 + ie.key.size() + 8;
                idx_val_len = (idx_val_len + 7) & ~7u;
            }
            idx_val_len += 16;

            if (idx_val_len <= REC_SIZE - 300) {
                uint32_t ilen = 0;
                std::vector<uint8_t> iv = buildIndexValue(idx, &ilen, false);
                rec.appendAttr(ATTR_INDEX_ROOT, 0x90, iv.data(),
                               static_cast<uint32_t>(iv.size()), "$I30");
            } else {
                // Non-resident $INDEX_ALLOCATION with INDX buffers.
                uint64_t nbuf = (idx.size() + per_buf - 1) / per_buf;
                if (nbuf == 0) nbuf = 1;
                uint64_t start = mft_lcn + plan.mft_clusters + plan.bitmap_clusters +
                                 plan.log_clusters + plan.upcase_clusters + plan.indx_used;
                uint64_t ncl = (nbuf * INDX_SIZE + bpc - 1) / bpc;
                plan.indx_used += ncl;

                std::vector<Run> runs{ Run{ static_cast<int64_t>(start), ncl } };
                rec.appendAttrNR(ATTR_INDEX_ALLOCATION, 0xA0, runs,
                                 nbuf * INDX_SIZE, nbuf * INDX_SIZE, "$I30");
                rec.appendAttrNR(ATTR_BITMAP, 0xB0, runs,
                                 (nbuf + 7) / 8, (nbuf + 7) / 8, "$I30");
                std::vector<IdxEntry> empty;
                uint32_t ilen2 = 0;
                std::vector<uint8_t> iv2 = buildIndexValue(empty, &ilen2, true);
                rec.appendAttr(ATTR_INDEX_ROOT, 0x90, iv2.data(),
                               static_cast<uint32_t>(iv2.size()), "$I30");

                // Build the buffer content now.
                IndxWrite w;
                w.start_lcn = start;
                w.content.assign(ncl * bpc, 0);
                for (uint64_t b = 0; b < nbuf; b++) {
                    std::vector<IdxEntry> sub;
                    for (size_t i = b * per_buf;
                         i < idx.size() && i < (b + 1) * per_buf; i++) {
                        sub.push_back(idx[i]);
                    }
                    bool is_last = (b + 1 == nbuf);
                    uint64_t next_vcn = is_last ? 0 : start + b + 1;
                    std::vector<uint8_t> indx =
                        buildIndxRecord(sub, start + b, is_last, next_vcn);
                    std::memcpy(w.content.data() + static_cast<size_t>(b) * INDX_SIZE,
                                indx.data(), INDX_SIZE);
                }
                indx_writes.push_back(std::move(w));
            }
        } else {
            if (e.chain.empty() || e.size == 0) {
                rec.appendAttr(ATTR_DATA, 0x80, nullptr, 0);
            } else {
                std::vector<Run> runs = chainToRuns(e.chain, plan.data_start_lcn);
                rec.appendAttrNR(ATTR_DATA, 0x80, runs, alloc_size, e.size);
            }
        }
        rec.finish();
        records[e.record] = rec.bytes;
    }

    // --- 8. Write boot sector ---
    ntfs::NTFSLayout nl;
    nl.total_size = size_bytes;
    nl.total_sectors = plan.total_sectors;
    nl.sectors_per_cluster = plan.spc;
    nl.bytes_per_sector = 512;
    nl.bytes_per_cluster = bpc;
    nl.total_clusters = plan.total_clusters;
    nl.mft_lcn = plan.mft_lcn;
    nl.mft_mirr_lcn = plan.mft_mirr_lcn;
    nl.mft_record_size = REC_SIZE;
    nl.index_record_size = INDX_SIZE;
    nl.clusters_per_mft_record = -10;
    nl.clusters_per_index_record = -12;
    nl.serial_number = plan.serial;

    ntfs::NTFSBootSector bs;
    initBootSector(bs, nl);
    r = disk->write(&bs, start_sector * 512, sizeof(ntfs::NTFSBootSector));
    if (r.failed()) return Result::error("Failed to write NTFS boot sector: " + r.message);

    // --- 9. Write metadata regions ---
    {
        uint64_t mft_off = (start_sector + plan.mft_lcn * plan.spc) * 512;
        uint64_t total_mft_bytes = total_records * REC_SIZE;
        std::vector<uint8_t> mft(total_mft_bytes, 0);
        for (size_t i = 0; i < records.size(); i++) {
            std::memcpy(mft.data() + i * REC_SIZE, records[i].data(), REC_SIZE);
        }
        r = disk->write(mft.data(), mft_off, mft.size());
        if (r.failed()) return Result::error("Failed to write MFT: " + r.message);
    }
    {
        uint64_t mirr_off = (start_sector + plan.mft_mirr_lcn * plan.spc) * 512;
        std::vector<uint8_t> mirr(plan.mirror_clusters * bpc, 0);
        for (uint64_t i = 0; i < 4 && i < records.size(); i++) {
            std::memcpy(mirr.data() + i * REC_SIZE, records[i].data(), REC_SIZE);
        }
        r = disk->write(mirr.data(), mirr_off, mirr.size());
        if (r.failed()) return Result::error("Failed to write MFT mirror: " + r.message);
    }
    {
        uint64_t bmp_off = (start_sector + (plan.mft_lcn + plan.mft_clusters) * plan.spc) * 512;
        std::vector<uint8_t> bmp_region(plan.bitmap_clusters * bpc, 0);
        std::memcpy(bmp_region.data(), plan.bitmap.data(), plan.bitmap.size());
        r = disk->write(bmp_region.data(), bmp_off, bmp_region.size());
        if (r.failed()) return Result::error("Failed to write $Bitmap: " + r.message);
    }
    {
        uint64_t log_off = (start_sector + (plan.mft_lcn + plan.mft_clusters +
                                            plan.bitmap_clusters) * plan.spc) * 512;
        std::vector<uint8_t> logdata(plan.log_clusters * bpc, 0);
        logdata[0] = 'R'; logdata[1] = 'S'; logdata[2] = 'T'; logdata[3] = 'R';
        r = disk->write(logdata.data(), log_off, logdata.size());
        if (r.failed()) return Result::error("Failed to write $LogFile: " + r.message);
    }
    {
        uint64_t up_off = (start_sector + (plan.mft_lcn + plan.mft_clusters +
                                           plan.bitmap_clusters + plan.log_clusters) * plan.spc) * 512;
        std::vector<uint16_t> upcase(65536);
        for (uint32_t i = 0; i < 65536; i++) {
            upcase[i] = (i >= 'a' && i <= 'z') ? i - 'a' + 'A' : i;
        }
        r = disk->write(upcase.data(), up_off, upcase.size() * sizeof(uint16_t));
        if (r.failed()) return Result::error("Failed to write $UpCase: " + r.message);
    }
    // INDX buffers
    for (const auto& w : indx_writes) {
        uint64_t off = (start_sector + w.start_lcn * plan.spc) * 512;
        r = disk->write(w.content.data(), off, w.content.size());
        if (r.failed()) return Result::error("Failed to write index buffers: " + r.message);
    }

    r = disk->flush();
    if (r.failed()) return Result::error("Failed to flush: " + r.message);

    return Result::ok();
}

Result convertPartitionToNTFS(std::shared_ptr<DiskIO> disk, int partition_number,
                              std::string* out_label) {
    if (!disk || !disk->isOpen()) return Result::error("Disk not open");
    auto table = PartitionTable::load(disk);
    if (!table) return Result::error("No partition table found");
    auto p = table->getPartition(partition_number);
    if (!p) return Result::error("Partition " + std::to_string(partition_number) +
                                 " does not exist");
    FileSystemType fs = detectFilesystemAt(disk, p->startSector());
    if (fs != FileSystemType::FAT32) {
        return Result::error("Partition " + std::to_string(partition_number) +
                             " is not FAT32 (found " + getFilesystemName(fs) + ")");
    }
    std::string label;
    Result r = convertFAT32ToNTFS(disk, p->startSector(), p->sizeBytes(), "");
    if (r.failed()) return r;

    auto t2 = PartitionTable::load(disk);
    if (t2) {
        auto p2 = t2->getPartition(partition_number);
        if (p2) {
            uint8_t boot[512];
            if (disk->readSector(boot, p2->startSector()).success()) {
                uint64_t mft_lcn = get64(boot + 48);
                uint8_t spc = boot[13];
                uint64_t rec_off = (p2->startSector() + mft_lcn * spc) * 512 +
                                   ntfs::MFT_VOLUME * REC_SIZE;
                std::vector<uint8_t> rec(REC_SIZE, 0);
                if (disk->read(rec.data(), rec_off, REC_SIZE).success()) {
                    size_t off = get16(rec.data() + 20);  // attr offset
                    while (off + 8 <= REC_SIZE) {
                        uint32_t type = get32(rec.data() + off);
                        if (type == 0xFFFFFFFFu) break;
                        uint32_t len = get32(rec.data() + off + 4);
                        if (len == 0 || off + len > REC_SIZE) break;
                        if (type == ntfs::ATTR_VOLUME_NAME) {
                            uint32_t vlen = get32(rec.data() + off + 16);
                            uint32_t voff = get32(rec.data() + off + 20);
                            if (voff + vlen <= REC_SIZE) {
                                std::vector<uint8_t> raw(
                                    rec.begin() + off + voff,
                                    rec.begin() + off + voff + vlen);
                                std::u16string u16;
                                for (size_t i = 0; i + 1 < raw.size(); i += 2) {
                                    u16.push_back(static_cast<char16_t>(
                                        raw[i] | (raw[i + 1] << 8)));
                                }
                                label.assign(u16.begin(), u16.end());
                            }
                        }
                        off += len;
                    }
                }
            }
        }
    }
    if (out_label) *out_label = label;
    return Result::ok();
}

} // namespace opm
