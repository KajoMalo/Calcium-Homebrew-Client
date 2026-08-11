#pragma once

#include <string>
#include <filesystem>
#include <cstdint>
#include <vector>

namespace calcium::packages {

/// Result of a verification attempt.
struct VerificationResult {
    bool        passed = false;
    std::string computed_hash;   ///< Actual SHA-256 of the file (lowercase hex).
    std::string expected_hash;   ///< Hash from metadata (may be empty = skipped).
    std::string error;           ///< Non-empty if the operation itself failed.

    bool hash_skipped() const { return expected_hash.empty(); }
};

/// Verifies downloaded packages via SHA-256.
///
/// The implementation is a portable software SHA-256; no OpenSSL dependency
/// is required. For production use the implementation can be swapped to a
/// platform-accelerated version without changing callers.
class PackageVerifier {
public:
    /// Compute SHA-256 of a file on disk and compare it to expected_sha256.
    /// If expected_sha256 is empty the hash is still computed but comparison
    /// is skipped and the result is marked as passed.
    static VerificationResult verify(const std::filesystem::path& path,
                                     const std::string& expected_sha256 = "");

    /// Compute SHA-256 of a raw byte buffer (useful in tests).
    static std::string sha256_hex(const std::vector<uint8_t>& data);

    /// Compute SHA-256 of a string.
    static std::string sha256_hex(const std::string& data);

private:
    // SHA-256 core — public domain implementation embedded directly so the
    // project has no external cryptographic dependency.
    struct Sha256State {
        uint32_t state[8];
        uint64_t bit_count;
        uint8_t  buffer[64];

        Sha256State();
        void update(const uint8_t* data, std::size_t len);
        void finalise(uint8_t digest[32]);
    };

    static void process_block(uint32_t state[8], const uint8_t block[64]);
    static std::string to_hex(const uint8_t digest[32]);
};

} // namespace calcium::packages
