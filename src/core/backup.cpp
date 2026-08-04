#include "opm/backup.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <ctime>

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
// OPMIMG format layout
// ============================================================================
constexpr char BACKUP_MAGIC[8] = {'O','P','M','I','M','G','0','1'};
constexpr uint32_t BACKUP_VERSION = 1;
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
    std::strncpy(h.source_name, source->devicePath().c_str(), sizeof(h.source_name) - 1);

    std::vector<uint8_t> bitmap((nb + 7) / 8, 0xFF);
    std::vector<uint8_t> table(nb * SHA256_LEN, 0);
    std::vector<uint8_t> payload;
    payload.reserve(nb * bs);
    std::vector<uint8_t> buf(bs);

    for (uint64_t i = 0; i < nb; i++) {
        uint64_t read_bytes = bs;
        if ((i + 1) * bs > src_size) read_bytes = src_size - i * bs;
        Result r = source->read(buf.data(), i * bs, bs);  // read full block size window; tail beyond size reads up to file's actual content
        if (r.failed()) return Result::error("read failed at block " + std::to_string(i) + ": " + r.message);
        sha256_of(buf.data(), read_bytes, &table[i * SHA256_LEN]);
        payload.insert(payload.end(), buf.data(), buf.data() + read_bytes);
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
    std::strncpy(h.source_name, source->devicePath().c_str(), sizeof(h.source_name) - 1);

    std::vector<uint8_t> bitmap((nb + 7) / 8, 0);
    std::vector<uint8_t> table = base_table; // start from base state; only changed digests updated
    std::vector<uint8_t> payload;
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
            payload.insert(payload.end(), buf.data(), buf.data() + read_bytes);
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
    std::vector<uint8_t> buf(bs);
    uint64_t written = 0;
    for (uint64_t i = 0; i < h.num_blocks; i++) {
        bool present = (bitmap[i / 8] >> (i % 8)) & 1;
        if (!present) continue; // inherited from base — leave target as-is
        uint64_t read_bytes = bs;
        if ((i + 1) * bs > h.source_size) read_bytes = h.source_size - i * bs;
        r = readAll(f, buf.data(), read_bytes, "block data");
        if (r.failed()) { std::fclose(f); return r; }
        r = target->write(buf.data(), i * bs, read_bytes);
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
    std::vector<uint8_t> buf(bs);
    uint64_t checked = 0;
    for (uint64_t i = 0; i < h.num_blocks; i++) {
        bool present = (bitmap[i / 8] >> (i % 8)) & 1;
        if (!present) continue;
        uint64_t read_bytes = bs;
        if ((i + 1) * bs > h.source_size) read_bytes = h.source_size - i * bs;
        r = readAll(f, buf.data(), read_bytes, "block data");
        if (r.failed()) { std::fclose(f); return r; }
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

} // namespace opm