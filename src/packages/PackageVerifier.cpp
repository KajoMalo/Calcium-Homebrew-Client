#include "PackageVerifier.hpp"
#include "../logging/Logger.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <stdexcept>

namespace calcium::packages {

static constexpr std::string_view TAG = "PackageVerifier";

// ─── SHA-256 constants ────────────────────────────────────────────────────────
// First 32 bits of the fractional parts of the cube roots of the first 64 primes.

static constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

// ─── Sha256State ──────────────────────────────────────────────────────────────

PackageVerifier::Sha256State::Sha256State() {
    // Initial hash values: first 32 bits of fractional parts of sqrt of first 8 primes.
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
    bit_count = 0;
    std::memset(buffer, 0, sizeof(buffer));
}

static inline uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32u - n));
}

void PackageVerifier::process_block(uint32_t st[8], const uint8_t block[64]) {
    uint32_t w[64];

    // Prepare message schedule.
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4    ]) << 24)
             | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
             | (static_cast<uint32_t>(block[i * 4 + 2]) <<  8)
             |  static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i- 2], 17) ^ rotr32(w[i- 2], 19) ^ (w[i- 2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
    uint32_t e = st[4], f = st[5], g = st[6], h = st[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1    = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch    = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0    = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
    st[4] += e; st[5] += f; st[6] += g; st[7] += h;
}

void PackageVerifier::Sha256State::update(const uint8_t* data, std::size_t len) {
    std::size_t buf_used = static_cast<std::size_t>((bit_count / 8) % 64);

    bit_count += static_cast<uint64_t>(len) * 8;

    // Fill partial buffer first.
    if (buf_used > 0) {
        std::size_t fill = std::min(len, 64 - buf_used);
        std::memcpy(buffer + buf_used, data, fill);
        data    += fill;
        len     -= fill;
        buf_used += fill;
        if (buf_used == 64) {
            process_block(state, buffer);
            buf_used = 0;
        }
    }

    // Process full 64-byte blocks directly from input.
    while (len >= 64) {
        process_block(state, data);
        data += 64;
        len  -= 64;
    }

    // Buffer any remaining bytes.
    if (len > 0) {
        std::memcpy(buffer, data, len);
    }
}

void PackageVerifier::Sha256State::finalise(uint8_t digest[32]) {
    std::size_t buf_used = static_cast<std::size_t>((bit_count / 8) % 64);

    // Append 0x80 padding byte.
    buffer[buf_used++] = 0x80;

    // If not enough room for the 8-byte length, pad and process this block.
    if (buf_used > 56) {
        std::memset(buffer + buf_used, 0, 64 - buf_used);
        process_block(state, buffer);
        buf_used = 0;
    }

    // Pad remaining space up to byte 56, then append big-endian bit count.
    std::memset(buffer + buf_used, 0, 56 - buf_used);
    for (int i = 7; i >= 0; --i) {
        buffer[56 + (7 - i)] = static_cast<uint8_t>((bit_count >> (i * 8)) & 0xff);
    }
    process_block(state, buffer);

    // Produce digest in big-endian order.
    for (int i = 0; i < 8; ++i) {
        digest[i * 4    ] = static_cast<uint8_t>(state[i] >> 24);
        digest[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
        digest[i * 4 + 2] = static_cast<uint8_t>(state[i] >>  8);
        digest[i * 4 + 3] = static_cast<uint8_t>(state[i]       );
    }
}

// ─── Public hex helpers ───────────────────────────────────────────────────────

std::string PackageVerifier::to_hex(const uint8_t digest[32]) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string PackageVerifier::sha256_hex(const std::vector<uint8_t>& data) {
    Sha256State s;
    s.update(data.data(), data.size());
    uint8_t digest[32];
    s.finalise(digest);
    return to_hex(digest);
}

std::string PackageVerifier::sha256_hex(const std::string& data) {
    Sha256State s;
    s.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    uint8_t digest[32];
    s.finalise(digest);
    return to_hex(digest);
}

// ─── File verification ────────────────────────────────────────────────────────

VerificationResult PackageVerifier::verify(const std::filesystem::path& path,
                                            const std::string& expected_sha256) {
    VerificationResult result;
    result.expected_hash = expected_sha256;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        result.error = "Cannot open file for verification: " + path.string();
        logging::Logger::instance().error(TAG, result.error);
        return result;
    }

    Sha256State state;
    constexpr std::size_t CHUNK = 65536;
    std::vector<uint8_t> buf(CHUNK);

    while (file) {
        file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(CHUNK));
        std::streamsize read = file.gcount();
        if (read > 0) {
            state.update(buf.data(), static_cast<std::size_t>(read));
        }
    }

    if (file.bad()) {
        result.error = "I/O error reading file: " + path.string();
        logging::Logger::instance().error(TAG, result.error);
        return result;
    }

    uint8_t digest[32];
    state.finalise(digest);
    result.computed_hash = to_hex(digest);

    if (expected_sha256.empty()) {
        // No expected hash — compute only, skip comparison.
        result.passed = true;
        logging::Logger::instance().debug(TAG,
            "Hash computed (no expected): " + result.computed_hash);
    } else {
        // Case-insensitive comparison.
        auto lower = [](std::string s) {
            for (auto& c : s) c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        result.passed = (lower(result.computed_hash) == lower(expected_sha256));
        if (result.passed) {
            logging::Logger::instance().info(TAG,
                "Hash verified OK: " + result.computed_hash);
        } else {
            logging::Logger::instance().error(TAG,
                "Hash mismatch!\n  expected: " + expected_sha256 +
                "\n  got:      " + result.computed_hash);
        }
    }

    return result;
}

} // namespace calcium::packages
