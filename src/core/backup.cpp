#include "opm/backup.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <set>
#include <system_error>

namespace opm {

std::string BackupInfo::mode_string() const {
    switch (mode) {
        case BackupMode::Full: return "full";
        case BackupMode::Incremental: return "incremental";
        case BackupMode::Differential: return "differential";
    }
    return "unknown";
}

namespace {

// ============================================================================
// Self-contained SHA-256 (FIPS 180-4). Kept internal so the backup engine is
// cross-platform and does not depend on OpenSSL (not available under Wine in
// the Windows CI path).
// ============================================================================
struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buffer[64];
    size_t buflen;
};

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t sha_rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_transform(Sha256Ctx* ctx, const uint8_t* chunk) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)chunk[i*4]<<24) | ((uint32_t)chunk[i*4+1]<<16) |
               ((uint32_t)chunk[i*4+2]<<8) | (uint32_t)chunk[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = sha_rotr(w[i-15],7) ^ sha_rotr(w[i-15],18) ^ (w[i-15]>>3);
        uint32_t s1 = sha_rotr(w[i-2],17) ^ sha_rotr(w[i-2],19) ^ (w[i-2]>>10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=ctx->state[0],b=ctx->state[1],c=ctx->state[2],d=ctx->state[3];
    uint32_t e=ctx->state[4],f=ctx->state[5],g=ctx->state[6],h=ctx->state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = sha_rotr(e,6) ^ sha_rotr(e,11) ^ sha_rotr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + SHA256_K[i] + w[i];
        uint32_t S0 = sha_rotr(a,2) ^ sha_rotr(a,13) ^ sha_rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha256_init(Sha256Ctx* ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->bitlen=0; ctx->buflen=0;
}

static void sha256_update(Sha256Ctx* ctx, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bitlen += 512;
            ctx->buflen = 0;
        }
    }
}

static void sha256_final(Sha256Ctx* ctx, uint8_t out[32]) {
    // Pad: 0x80, zeros, then 64-bit bit length (big-endian)
    uint64_t bitlen = ctx->bitlen + ctx->buflen * 8;
    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);
    while (ctx->buflen != 56) {
        uint8_t z = 0;
        sha256_update(ctx, &z, 1);
    }
    uint8_t lenbuf[8];
    for (int i = 0; i < 8; i++) lenbuf[i] = (uint8_t)(bitlen >> (56 - i*8));
    sha256_update(ctx, lenbuf, 8);
    for (int i = 0; i < 8; i++) {
        uint32_t s = ctx->state[i];
        for (int j = 0; j < 4; j++) out[i*4+j] = (uint8_t)(s >> (24 - j*8));
    }
}

static void sha256_of(const uint8_t* data, size_t len, uint8_t out[32]) {
    Sha256Ctx ctx; sha256_init(&ctx); sha256_update(&ctx, data, len); sha256_final(&ctx, out);
}

// ============================================================================
// Per-block compression (self-contained, cross-platform — no zlib on the
// MinGW sysroot). Each stored block uses one of:
//   RAW  (0): block stored verbatim (incompressible data)
//   ZERO (1): block is all-zero (free space) — no payload bytes at all
//   RLE  (2): run-length encoded stream
// Encoding: literal byte b != 0 -> b; literal 0 -> 0x00 0x00; run of c copies
// of value v -> 0x00 c v (c >= 1; c==0 never emitted). Decoder: byte 0x00,
// next byte c: c==0 -> single zero, else read v -> emit c copies of v.
// ============================================================================
constexpr uint8_t BLK_RAW = 0;
constexpr uint8_t BLK_ZERO = 1;
constexpr uint8_t BLK_RLE = 2;

static bool allZero(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (data[i] != 0) return false;
    return true;
}

// Append the RLE-encoded form of src[0..len) to out.
static void rleEncode(const uint8_t* src, size_t len, std::vector<uint8_t>& out) {
    size_t i = 0;
    while (i < len) {
        size_t j = i + 1;
        while (j < len && src[j] == src[i] && (j - i) < 255) j++;
        size_t run = j - i;
        uint8_t v = src[i];
        // Use a run token when it saves bytes: zero runs >= 2, others >= 4.
        bool use_run = (v == 0 && run >= 2) || (v != 0 && run >= 4);
        if (use_run) {
            out.push_back(0x00);
            out.push_back(static_cast<uint8_t>(run));
            out.push_back(v);
        } else {
            for (size_t k = i; k < j; k++) {
                if (src[k] == 0) { out.push_back(0x00); out.push_back(0x00); }
                else out.push_back(src[k]);
            }
        }
        i = j;
    }
}

// Decode an RLE stream of src[0..len) into out (which must be pre-sized to the
// expected decoded length). Returns false on malformed input / overflow.
static bool rleDecode(const uint8_t* src, size_t len, std::vector<uint8_t>& out, size_t expect) {
    size_t i = 0, o = 0;
    while (i < len && o < expect) {
        if (src[i] != 0) { out[o++] = src[i++]; continue; }
        if (i + 1 >= len) return false;
        uint8_t c = src[i + 1];
        if (c == 0) { out[o++] = 0; i += 2; continue; }
        if (i + 2 >= len) return false;
        uint8_t v = src[i + 2];
        if (o + c > expect) return false;
        std::memset(&out[o], v, c);
        o += c;
        i += 3;
    }
    return i == len && o == expect;
}

// Compress one block into out: appends [method byte][payload] and returns
// the method used. Decoded size is always the caller's known block length.
static uint8_t compressBlock(const uint8_t* src, size_t len, std::vector<uint8_t>& out) {
    if (allZero(src, len)) {
        out.push_back(BLK_ZERO);
        return BLK_ZERO;
    }
    // Try RLE; if the result is not smaller than raw, store raw.
    std::vector<uint8_t> enc;
    enc.reserve(len / 2 + 16);
    rleEncode(src, len, enc);
    if (enc.size() < len) {
        out.push_back(BLK_RLE);
        out.insert(out.end(), enc.begin(), enc.end());
        return BLK_RLE;
    }
    out.push_back(BLK_RAW);
    out.insert(out.end(), src, src + len);
    return BLK_RAW;
}

// Encode one block into an owned byte vector: [method byte][payload].
static std::vector<uint8_t> encodeBlock(const std::vector<uint8_t>& src, size_t len) {
    std::vector<uint8_t> enc;
    compressBlock(src.data(), len, enc);
    return enc;
}

// Append one stored block to a data section.
//   uncompressed layout: [payload bytes] (block's raw bytes)
//   compressed layout:   [uint32 len][method byte][payload]
// Returns the method used (for progress/reporting; ZERO blocks emit only the
// method byte, RAW/RLE emit method + the block's encoded length).
static uint8_t appendBlock(const std::vector<uint8_t>& src, size_t len,
                           std::vector<uint8_t>& section, bool compress) {
    if (!compress) {
        section.insert(section.end(), src.begin(), src.begin() + static_cast<std::ptrdiff_t>(len));
        return BLK_RAW;
    }
    // Build the encoded block, then prefix with its total length.
    std::vector<uint8_t> enc = encodeBlock(src, len);
    uint8_t method = enc.empty() ? BLK_RAW : enc[0];
    uint32_t payload_len = static_cast<uint32_t>(enc.size());
    uint32_t stored_len = payload_len; // bytes that follow the 4-byte prefix
    uint8_t prefix[4] = {
        static_cast<uint8_t>(stored_len & 0xFF),
        static_cast<uint8_t>((stored_len >> 8) & 0xFF),
        static_cast<uint8_t>((stored_len >> 16) & 0xFF),
        static_cast<uint8_t>((stored_len >> 24) & 0xFF)
    };
    section.insert(section.end(), prefix, prefix + 4);
    section.insert(section.end(), enc.begin(), enc.end());
    return method;
}

// Decode one stored block: section = [method byte][payload] (the 4-byte length
// prefix is consumed by the caller before calling this). Output is the decoded
// block bytes (exactly 'expected_len' on success).
static bool readStoredBlock(const uint8_t* section, size_t section_len,
                            size_t expected_len, std::vector<uint8_t>& out) {
    if (section_len < 1) return false;
    uint8_t method = section[0];
    const uint8_t* payload = section + 1;
    uint32_t payload_len = static_cast<uint32_t>(section_len - 1);
    out.assign(expected_len, 0);
    switch (method) {
        case BLK_RAW:
            if (payload_len != expected_len) return false;
            std::memcpy(out.data(), payload, expected_len);
            return true;
        case BLK_ZERO:
            // all-zero block; out is already zeroed. Any payload ignored.
            return true;
        case BLK_RLE:
            return rleDecode(payload, payload_len, out, expected_len);
        default:
            return false;
    }
}

// ============================================================================
// OPMIMG format layout
// ============================================================================
constexpr char BACKUP_MAGIC[8] = {'O','P','M','I','M','G','0','1'};
constexpr uint32_t BACKUP_VERSION = 1;
constexpr uint32_t FLAG_COMPRESSED = 0x01; // data section is [u32 len][method][payload] per block
constexpr size_t BACKUP_HEADER_SIZE = 512;
constexpr size_t SHA256_LEN = 32;

struct __attribute__((packed)) BackupHeader {
    char magic[8];
    uint32_t version;
    uint32_t mode;          // 0 full, 1 incremental, 2 differential
    uint64_t source_size;
    uint32_t sector_size;
    uint32_t block_size;
    uint64_t num_blocks;
    uint64_t present_blocks;
    uint64_t created_at;
    char source_name[256];
    uint32_t flags;
    uint8_t reserved[196];  // pad to 512
};
static_assert(sizeof(BackupHeader) == BACKUP_HEADER_SIZE, "BackupHeader must be 512 bytes");

static uint64_t numBlocks(uint64_t bytes, uint32_t block_size) {
    return (bytes + block_size - 1) / block_size;
}

static Result writeAll(FILE* f, const void* data, size_t size, const char* what) {
    if (fwrite(data, 1, size, f) != size)
        return Result::error(std::string("failed to write ") + what);
    return Result::ok();
}

static Result readAll(FILE* f, void* data, size_t size, const char* what) {
    if (fread(data, 1, size, f) != size)
        return Result::error(std::string("truncated reading ") + what);
    return Result::ok();
}

static void fillHeader(BackupHeader& h) {
    std::memset(&h, 0, sizeof(h));
    std::memcpy(h.magic, BACKUP_MAGIC, 8);
    h.version = BACKUP_VERSION;
    h.created_at = static_cast<uint64_t>(std::time(nullptr));
}

static bool looksValid(const BackupHeader& h) {
    return std::memcmp(h.magic, BACKUP_MAGIC, 8) == 0 &&
           h.version == BACKUP_VERSION &&
           h.block_size > 0 &&
           h.num_blocks > 0 &&
           (h.mode == 0 || h.mode == 1 || h.mode == 2);
}

namespace {

// Replacement for strnlen (not available on MSVC).
static size_t boundedStrlen(const char* s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0') n++;
    return n;
}

// Rename over a partial file so a failed backup never leaves a valid-looking image.
static Result commitFile(const std::string& tmp, const std::string& final) {
    if (std::rename(tmp.c_str(), final.c_str()) != 0)
        return Result::error("failed to finalize image at " + final);
    return Result::ok();
}

} // namespace

} // namespace

// ---------------------------------------------------------------------------
// Creation (shared full/incremental writer)
// ---------------------------------------------------------------------------
static Result writeImage(const std::string& out_path,
                         const BackupHeader& header,
                         const std::vector<uint8_t>& inherited_flags, // 1=present(store), 0=inherit
                         const std::vector<uint8_t>& checksum_table,  // num_blocks*32
                         const std::vector<uint8_t>& data_blocks,     // present_blocks*block_size
                         const BackupOptions& options) {
    std::string tmp = out_path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return Result::error("cannot open " + tmp + " for writing");
    Result r = writeAll(f, &header, sizeof(header), "header");
    if (r.success()) r = writeAll(f, inherited_flags.data(), inherited_flags.size(), "bitmap");
    if (r.success()) r = writeAll(f, checksum_table.data(), checksum_table.size(), "checksum table");
    if (r.success()) r = writeAll(f, data_blocks.data(), data_blocks.size(), "data blocks");
    std::fclose(f);
    if (r.failed()) { std::remove(tmp.c_str()); return r; }
    return commitFile(tmp, out_path);
}

// ---------------------------------------------------------------------------
// FULL backup
// ---------------------------------------------------------------------------
Result backupCreateFull(std::shared_ptr<DiskIO> source,
                        const std::string& image_path,
                        const BackupOptions& options) {
    if (!source || !source->isOpen())
        return Result::error("source device is not open");
    uint64_t src_size = source->size();
    uint32_t sector = source->sectorSize();
    if (src_size == 0) return Result::error("source device has zero size");

    uint32_t bs = options.block_size > 0 ? options.block_size : 1048576;
    uint64_t nb = numBlocks(src_size, bs);

    BackupHeader h; fillHeader(h);
    h.mode = 0; h.source_size = src_size; h.sector_size = sector; h.block_size = bs;
    h.num_blocks = nb; h.present_blocks = nb;
    if (options.compress) h.flags |= FLAG_COMPRESSED;
    std::strncpy(h.source_name, source->devicePath().c_str(), sizeof(h.source_name) - 1);

    std::vector<uint8_t> bitmap((nb + 7) / 8, 0xFF);
    std::vector<uint8_t> table(nb * SHA256_LEN, 0);
    std::vector<uint8_t> payload;
    payload.reserve(options.compress ? nb * 64 : nb * bs);
    std::vector<uint8_t> buf(bs);

    for (uint64_t i = 0; i < nb; i++) {
        uint64_t read_bytes = bs;
        if ((i + 1) * bs > src_size) read_bytes = src_size - i * bs;
        Result r = source->read(buf.data(), i * bs, bs);  // read full block size window; tail beyond size reads up to file's actual content
        if (r.failed()) return Result::error("read failed at block " + std::to_string(i) + ": " + r.message);
        sha256_of(buf.data(), read_bytes, &table[i * SHA256_LEN]);
        appendBlock(buf, read_bytes, payload, options.compress);
        if (options.progress_callback) {
            options.progress_callback(i + 1, nb, "backing up full");
        }
    }
    return writeImage(image_path, h, bitmap, table, payload, options);
}

// ---------------------------------------------------------------------------
// INCREMENTAL / DIFFERENTIAL backup
// ---------------------------------------------------------------------------
Result backupCreateIncremental(std::shared_ptr<DiskIO> source,
                               const std::string& base_image,
                               const std::string& image_path,
                               bool differential,
                               const BackupOptions& options) {
    if (!source || !source->isOpen())
        return Result::error("source device is not open");
    uint64_t src_size = source->size();
    uint32_t sector = source->sectorSize();
    if (src_size == 0) return Result::error("source device has zero size");

    // Load base image header + checksum table
    FILE* f = std::fopen(base_image.c_str(), "rb");
    if (!f) return Result::error("cannot open base image " + base_image);
    BackupHeader bh;
    Result r = readAll(f, &bh, sizeof(bh), "base header");
    if (r.success() && !looksValid(bh)) r = Result::error("base image is not a valid OPMIMG");
    uint32_t base_bs = 0; uint64_t base_nb = 0;
    if (r.success()) { base_bs = bh.block_size; base_nb = bh.num_blocks; }
    std::vector<uint8_t> base_bitmap;
    if (r.success()) { base_bitmap.resize((base_nb + 7) / 8); r = readAll(f, base_bitmap.data(), base_bitmap.size(), "base bitmap"); }
    std::vector<uint8_t> base_table;
    if (r.success()) { base_table.resize(base_nb * SHA256_LEN); r = readAll(f, base_table.data(), base_table.size(), "base checksum table"); }
    std::fclose(f);
    if (r.failed()) return r;

    if (base_bs != (options.block_size > 0 ? options.block_size : 1048576) || base_nb != numBlocks(src_size, (options.block_size > 0 ? options.block_size : 1048576)))
        return Result::error("base image block geometry does not match source");

    uint32_t bs = base_bs;
    uint64_t nb = base_nb;
    if (differential && bh.mode != 0)
        return Result::error("differential base must be a full backup");

    BackupHeader h; fillHeader(h);
    h.mode = differential ? 2 : 1;
    h.source_size = src_size; h.sector_size = sector; h.block_size = bs;
    h.num_blocks = nb;
    if (options.compress) h.flags |= FLAG_COMPRESSED;
    std::strncpy(h.source_name, source->devicePath().c_str(), sizeof(h.source_name) - 1);

    std::vector<uint8_t> bitmap((nb + 7) / 8, 0);
    std::vector<uint8_t> table = base_table; // start from base state; only changed digests updated
    std::vector<uint8_t> payload;
    payload.reserve(options.compress ? 64 : bs);
    std::vector<uint8_t> buf(bs);
    uint64_t present = 0;

    for (uint64_t i = 0; i < nb; i++) {
        uint64_t read_bytes = bs;
        if ((i + 1) * bs > src_size) read_bytes = src_size - i * bs;
        Result rr = source->read(buf.data(), i * bs, bs);
        if (rr.failed()) return Result::error("read failed at block " + std::to_string(i) + ": " + rr.message);
        uint8_t digest[SHA256_LEN];
        sha256_of(buf.data(), read_bytes, digest);
        if (std::memcmp(digest, &base_table[i * SHA256_LEN], SHA256_LEN) != 0) {
            bitmap[i / 8] |= (uint8_t)(1 << (i % 8));
            std::memcpy(&table[i * SHA256_LEN], digest, SHA256_LEN);
            appendBlock(buf, read_bytes, payload, options.compress);
            present++;
        }
        if (options.progress_callback) options.progress_callback(i + 1, nb, "diffing blocks");
    }
    h.present_blocks = present;
    return writeImage(image_path, h, bitmap, table, payload, options);
}

// ---------------------------------------------------------------------------
// Restore
// ---------------------------------------------------------------------------
Result backupRestore(const std::string& image_path,
                     std::shared_ptr<DiskIO> target,
                     const BackupOptions& options) {
    if (!target || !target->isOpen())
        return Result::error("target device is not open");
    if (target->isReadOnly()) return Result::error("target is read-only; cannot restore");

    FILE* f = std::fopen(image_path.c_str(), "rb");
    if (!f) return Result::error("cannot open image " + image_path);
    BackupHeader h;
    Result r = readAll(f, &h, sizeof(h), "header");
    if (r.success() && !looksValid(h)) r = Result::error("image is not a valid OPMIMG");
    // read bitmap
    std::vector<uint8_t> bitmap;
    if (r.success()) { bitmap.resize((h.num_blocks + 7) / 8); r = readAll(f, bitmap.data(), bitmap.size(), "bitmap"); }
    // read checksum table
    std::vector<uint8_t> table;
    if (r.success()) { table.resize(h.num_blocks * SHA256_LEN); r = readAll(f, table.data(), table.size(), "checksum table"); }
    if (r.failed()) { std::fclose(f); return r; }

    uint64_t target_size = target->size();
    if (target_size < h.source_size)
        return Result::error("target is smaller than the image source (" +
                             std::to_string(target_size) + " < " + std::to_string(h.source_size) + ")");
    uint32_t bs = h.block_size;
    bool compressed = (h.flags & FLAG_COMPRESSED) != 0;
    std::vector<uint8_t> buf(bs);
    // For compressed blocks we decode into this scratch buffer (block size, not bs,
    // since a compressed block's payload is at most the decoded length).
    std::vector<uint8_t> dec;
    uint64_t written = 0;
    for (uint64_t i = 0; i < h.num_blocks; i++) {
        bool present = (bitmap[i / 8] >> (i % 8)) & 1;
        if (!present) continue; // inherited from base — leave target as-is
        uint64_t read_bytes = bs;
        if ((i + 1) * bs > h.source_size) read_bytes = h.source_size - i * bs;
        if (!compressed) {
            r = readAll(f, buf.data(), read_bytes, "block data");
            if (r.failed()) { std::fclose(f); return r; }
            r = target->write(buf.data(), i * bs, read_bytes);
        } else {
            // length-prefixed compressed block
            uint8_t lenb[4];
            r = readAll(f, lenb, 4, "block length");
            if (r.failed()) { std::fclose(f); return r; }
            uint32_t stored_len = (uint32_t)lenb[0] | ((uint32_t)lenb[1] << 8) |
                                  ((uint32_t)lenb[2] << 16) | ((uint32_t)lenb[3] << 24);
            if (stored_len == 0) { std::fclose(f); return Result::error("corrupt block length"); }
            std::vector<uint8_t> stored(stored_len);
            r = readAll(f, stored.data(), stored_len, "compressed block");
            if (r.failed()) { std::fclose(f); return r; }
            if (!readStoredBlock(stored.data(), stored_len, (size_t)read_bytes, dec)) {
                std::fclose(f);
                return Result::error("failed to decompress block " + std::to_string(i));
            }
            r = target->write(dec.data(), i * bs, read_bytes);
        }
        if (r.failed()) { std::fclose(f); return r; }
        written++;
        if (options.progress_callback) options.progress_callback(written, h.present_blocks, "restoring");
    }
    std::fclose(f);
    return target->flush();
}

// ---------------------------------------------------------------------------
// Info
// ---------------------------------------------------------------------------
Result backupInfo(const std::string& image_path, BackupInfo& info) {
    FILE* f = std::fopen(image_path.c_str(), "rb");
    if (!f) return Result::error("cannot open image " + image_path);
    BackupHeader h;
    Result r = readAll(f, &h, sizeof(h), "header");
    std::fclose(f);
    if (r.failed()) return r;
    if (!looksValid(h)) return Result::error("image is not a valid OPMIMG");
    info.mode = static_cast<BackupMode>(h.mode);
    info.source_size = h.source_size;
    info.sector_size = h.sector_size;
    info.block_size = h.block_size;
    info.num_blocks = h.num_blocks;
    info.present_blocks = h.present_blocks;
    info.created_at = h.created_at;
    info.compressed = (h.flags & FLAG_COMPRESSED) != 0;
    info.source_name = std::string(h.source_name, boundedStrlen(h.source_name, 256));
    return Result::ok();
}

// ---------------------------------------------------------------------------
// Verify: re-hash physically stored blocks against the checksum table.
// ---------------------------------------------------------------------------
Result backupVerify(const std::string& image_path) {
    FILE* f = std::fopen(image_path.c_str(), "rb");
    if (!f) return Result::error("cannot open image " + image_path);
    BackupHeader h;
    Result r = readAll(f, &h, sizeof(h), "header");
    if (r.success() && !looksValid(h)) r = Result::error("image is not a valid OPMIMG");
    std::vector<uint8_t> bitmap;
    if (r.success()) { bitmap.resize((h.num_blocks + 7) / 8); r = readAll(f, bitmap.data(), bitmap.size(), "bitmap"); }
    std::vector<uint8_t> table;
    if (r.success()) { table.resize(h.num_blocks * SHA256_LEN); r = readAll(f, table.data(), table.size(), "checksum table"); }
    if (r.failed()) { std::fclose(f); return r; }
    uint32_t bs = h.block_size;
    bool compressed = (h.flags & FLAG_COMPRESSED) != 0;
    std::vector<uint8_t> buf(bs);
    std::vector<uint8_t> dec;
    uint64_t checked = 0;
    for (uint64_t i = 0; i < h.num_blocks; i++) {
        bool present = (bitmap[i / 8] >> (i % 8)) & 1;
        if (!present) continue;
        uint64_t read_bytes = bs;
        if ((i + 1) * bs > h.source_size) read_bytes = h.source_size - i * bs;
        if (!compressed) {
            r = readAll(f, buf.data(), read_bytes, "block data");
            if (r.failed()) { std::fclose(f); return r; }
        } else {
            uint8_t lenb[4];
            r = readAll(f, lenb, 4, "block length");
            if (r.failed()) { std::fclose(f); return r; }
            uint32_t stored_len = (uint32_t)lenb[0] | ((uint32_t)lenb[1] << 8) |
                                  ((uint32_t)lenb[2] << 16) | ((uint32_t)lenb[3] << 24);
            if (stored_len == 0) { std::fclose(f); return Result::error("corrupt block length"); }
            std::vector<uint8_t> stored(stored_len);
            r = readAll(f, stored.data(), stored_len, "compressed block");
            if (r.failed()) { std::fclose(f); return r; }
            if (!readStoredBlock(stored.data(), stored_len, (size_t)read_bytes, dec)) {
                std::fclose(f);
                return Result::error("corrupt compressed block at index " + std::to_string(i));
            }
            std::memcpy(buf.data(), dec.data(), read_bytes);
        }
        uint8_t digest[SHA256_LEN];
        sha256_of(buf.data(), read_bytes, digest);
        if (std::memcmp(digest, &table[i * SHA256_LEN], SHA256_LEN) != 0) {
            std::fclose(f);
            return Result::error("checksum mismatch at block " + std::to_string(i));
        }
        checked++;
    }
    std::fclose(f);
    return Result::ok();
}

// ---------------------------------------------------------------------------
// Retention: list images in a backup-set directory + prune per policy
// ---------------------------------------------------------------------------
Result backupListDir(const std::string& dir, std::vector<BackupEntry>& entries) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec))
        return Result::error("not a directory: " + dir);
    for (const auto& de : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!de.is_regular_file(ec)) continue;
        std::string p = de.path().string();
        BackupInfo info;
        if (backupInfo(p, info).failed()) continue; // not an OPMIMG image — skip
        BackupEntry e;
        e.path = p;
        e.name = de.path().filename().string();
        e.info = info;
        e.file_size = static_cast<uint64_t>(de.file_size(ec));
        entries.push_back(std::move(e));
    }
    // Newest first
    std::sort(entries.begin(), entries.end(),
              [](const BackupEntry& a, const BackupEntry& b) {
                  return a.info.created_at > b.info.created_at;
              });
    return Result::ok();
}

Result backupPrune(const std::string& dir,
                   const PruneOptions& options,
                   std::vector<std::string>& removed) {
    std::vector<BackupEntry> entries;
    Result r = backupListDir(dir, entries);
    if (r.failed()) return r;

    // --older-than: delete images older than N days (by created_at)
    uint64_t cutoff = 0;
    if (options.older_than_days > 0) {
        uint64_t now = static_cast<uint64_t>(std::time(nullptr));
        cutoff = now - options.older_than_days * 86400ULL;
        for (const auto& e : entries) {
            if (e.info.created_at < cutoff)
                removed.push_back(e.path);
        }
    }

    // --keep-full: keep the N most recent FULL backups; any incremental /
    // differential older than the oldest retained full loses its base and is
    // deleted with it. Newer non-full images are always kept (they still have
    // a base on disk).
    if (options.keep_full > 0) {
        uint64_t kept = 0;
        uint64_t oldest_kept_full_ts = 0;
        for (const auto& e : entries) {
            if (e.info.mode != BackupMode::Full) continue;
            if (kept < options.keep_full) {
                kept++;
                oldest_kept_full_ts = e.info.created_at;
            }
        }
        if (kept == options.keep_full && oldest_kept_full_ts > 0) {
            for (const auto& e : entries) {
                if (e.info.created_at < oldest_kept_full_ts)
                    removed.push_back(e.path);
            }
        }
    }

// Deduplicate (an image can match both policies) and delete
    std::sort(removed.begin(), removed.end());
    removed.erase(std::unique(removed.begin(), removed.end()), removed.end());
    size_t failed = 0;
    for (const auto& p : removed) {
        if (std::remove(p.c_str()) != 0) failed++;
    }
    if (failed > 0)
        return Result::error(std::to_string(failed) + " of " +
                             std::to_string(removed.size()) + " images could not be removed");
    return Result::ok();
}

// ---------------------------------------------------------------------------
// Grandfather-Father-Son (GFS) retention
// ---------------------------------------------------------------------------

namespace {

// Civil calendar helpers (days-from-civil / civil-from-days, Hinnant).
int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void civilFromDays(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t yy = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yy + (m <= 2));
}

// Day-of-week: Monday=0 .. Sunday=6 (ISO).
int isoWeekday(int64_t days_since_epoch) {
    // 1970-01-01 was a Thursday. days_from_civil(1970,1,1) = 0.
    int wd = static_cast<int>((days_since_epoch + 3) % 7);
    if (wd < 0) wd += 7;
    return wd;  // 0=Mon ... 6=Sun
}

// ISO week number (1..53) + ISO year.
void isoWeek(int64_t days_since_epoch, int& year, int& week) {
    int y, m, d;
    civilFromDays(days_since_epoch, y, m, d);
    int wd = isoWeekday(days_since_epoch);
    // Thursday of the current ISO week
    int64_t thursday = days_since_epoch - wd + 3;
    civilFromDays(thursday, y, m, d);
    year = y;
    int64_t jan1 = daysFromCivil(year, 1, 1);
    week = static_cast<int>((thursday - jan1) / 7 + 1);
}

int64_t monthKey(int64_t days_since_epoch) {
    int y, m, d;
    civilFromDays(days_since_epoch, y, m, d);
    return int64_t(y) * 12 + (m - 1);
}

} // anonymous namespace

Result backupPruneGFS(const std::string& dir,
                      const GfsOptions& options,
                      std::vector<std::string>& removed) {
    std::vector<BackupEntry> entries;
    Result r = backupListDir(dir, entries);
    if (r.failed()) return r;
    removed.clear();
    if (entries.empty()) return Result::ok();

    // Entries are newest-first. Walk them once to collect the newest FULL
    // backup of each day / week / month bucket within the retention window.
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    const int64_t today = now / 86400;

    std::vector<bool> keep(entries.size(), false);
    std::set<int64_t> kept_days, kept_weeks, kept_months;

    // Pass 1 (newest first): pick anchors — newest full in each bucket.
    for (size_t i = 0; i < entries.size(); i++) {
        const BackupEntry& e = entries[i];
        if (e.info.mode != BackupMode::Full) continue;
        int64_t day = static_cast<int64_t>(e.info.created_at) / 86400;
        int w_year = 0, w_num = 0;
        isoWeek(day, w_year, w_num);
        int64_t week = static_cast<int64_t>(w_year) * 100 + w_num;
        int64_t month = monthKey(day);

        bool want = false;
        if (options.daily > 0 && today - day < static_cast<int64_t>(options.daily) &&
            !kept_days.count(day)) {
            kept_days.insert(day);
            want = true;
        }
        if (options.weekly > 0) {
            // weeks are counted from the current week backwards
            int cur_year = 0, cur_num = 0;
            isoWeek(today, cur_year, cur_num);
            int64_t cur_week = static_cast<int64_t>(cur_year) * 100 + cur_num;
            if (cur_week - week < static_cast<int64_t>(options.weekly) &&
                cur_week >= week && !kept_weeks.count(week)) {
                kept_weeks.insert(week);
                want = true;
            }
        }
        if (options.monthly > 0 && monthKey(today) - month <
            static_cast<int64_t>(options.monthly) && month <= monthKey(today) &&
            !kept_months.count(month)) {
            kept_months.insert(month);
            want = true;
        }
        if (want) keep[i] = true;
    }

    // Pass 2: chain safety. Retain a non-full image only when the most recent
    // FULL backup older than it is itself retained (restore needs that exact
    // base image on the target). A younger retained full does NOT serve as a
    // base for an older incremental, so we must find the immediately-preceding
    // full in the whole timeline and require it be kept.
    std::set<std::string> kept_paths;
    for (size_t i = 0; i < entries.size(); i++) {
        if (keep[i]) kept_paths.insert(entries[i].path);
    }

    // Build, per full, whether it is kept (by its created_at, newest first).
    std::vector<int64_t> full_ts_desc;      // all full timestamps, descending
    std::set<int64_t> kept_full_ts;         // retained full timestamps
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].info.mode == BackupMode::Full) {
            full_ts_desc.push_back(entries[i].info.created_at);
            if (keep[i]) kept_full_ts.insert(entries[i].info.created_at);
        }
    }
    // If no full anchors were selected but fulls exist, keep the newest full
    // as an implicit anchor so its incrementals survive (matches keep-full=0
    // semantics: never delete the newest full).
    if (kept_full_ts.empty() && !full_ts_desc.empty()) {
        kept_full_ts.insert(full_ts_desc.front());
        for (size_t i = 0; i < entries.size(); i++) {
            if (entries[i].info.mode == BackupMode::Full &&
                entries[i].info.created_at == full_ts_desc.front())
                keep[i] = true;
        }
    }

    for (size_t i = 0; i < entries.size(); i++) {
        if (keep[i]) continue;
        const BackupEntry& e = entries[i];
        if (e.info.mode == BackupMode::Full) continue;  // non-anchor full -> prune
        // Find the most recent full older than this image.
        int64_t base_ts = 0;
        for (int64_t ts : full_ts_desc) {
            if (ts <= e.info.created_at) { base_ts = ts; break; }
        }
        if (base_ts != 0 && kept_full_ts.count(base_ts)) keep[i] = true;
    }

    // Collect removals.
    for (size_t i = 0; i < entries.size(); i++) {
        if (!keep[i]) removed.push_back(entries[i].path);
    }
    std::sort(removed.begin(), removed.end());
    removed.erase(std::unique(removed.begin(), removed.end()), removed.end());
    size_t failed = 0;
    for (const auto& p : removed) {
        if (std::remove(p.c_str()) != 0) failed++;
    }
    if (failed > 0)
        return Result::error(std::to_string(failed) + " of " +
                             std::to_string(removed.size()) + " images could not be removed");
    return Result::ok();
}

} // namespace opm