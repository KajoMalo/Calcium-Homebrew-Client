#include <catch2/catch_test_macros.hpp>
#include "packages/PackageVerifier.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace calcium::packages;

// Known-correct SHA-256 values (verified against reference implementations).

// SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
static const std::string SHA256_EMPTY =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

// SHA-256("abc") = ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469ac1b5a0de2cda7bb9  (no trailing zeros — note this is the standard NIST value)
static const std::string SHA256_ABC =
    "ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469ac1b5a0de2cda7bb9";

// SHA-256("The quick brown fox jumps over the lazy dog")
static const std::string SHA256_FOX =
    "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592";

// ─── String hash tests ────────────────────────────────────────────────────────

TEST_CASE("PackageVerifier - sha256 of empty string", "[verifier]") {
    CHECK(PackageVerifier::sha256_hex(std::string{""}) == SHA256_EMPTY);
}

TEST_CASE("PackageVerifier - sha256 of 'abc'", "[verifier]") {
    CHECK(PackageVerifier::sha256_hex(std::string{"abc"}) == SHA256_ABC);
}

TEST_CASE("PackageVerifier - sha256 of fox string", "[verifier]") {
    CHECK(PackageVerifier::sha256_hex(
        std::string{"The quick brown fox jumps over the lazy dog"}) == SHA256_FOX);
}

TEST_CASE("PackageVerifier - sha256 of byte vector", "[verifier]") {
    std::vector<uint8_t> data{'a', 'b', 'c'};
    CHECK(PackageVerifier::sha256_hex(data) == SHA256_ABC);
}

TEST_CASE("PackageVerifier - sha256 of empty byte vector", "[verifier]") {
    std::vector<uint8_t> empty;
    CHECK(PackageVerifier::sha256_hex(empty) == SHA256_EMPTY);
}

TEST_CASE("PackageVerifier - sha256 is consistent across two calls", "[verifier]") {
    std::string input = "Calcium Client test data 12345";
    auto h1 = PackageVerifier::sha256_hex(input);
    auto h2 = PackageVerifier::sha256_hex(input);
    CHECK(h1 == h2);
}

TEST_CASE("PackageVerifier - sha256 differs for different inputs", "[verifier]") {
    CHECK(PackageVerifier::sha256_hex(std::string{"hello"}) !=
          PackageVerifier::sha256_hex(std::string{"world"}));
}

// ─── File verification tests ──────────────────────────────────────────────────

static std::filesystem::path write_temp_file(const std::string& content) {
    auto path = std::filesystem::temp_directory_path() /
                ("calcium_verify_test_" + std::to_string(std::rand()) + ".bin");
    std::ofstream f(path, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return path;
}

TEST_CASE("PackageVerifier - verify file with correct hash passes", "[verifier]") {
    std::string content = "abc";
    auto path = write_temp_file(content);

    auto result = PackageVerifier::verify(path, SHA256_ABC);

    CHECK(result.passed);
    CHECK(result.error.empty());
    CHECK(result.computed_hash == SHA256_ABC);

    std::filesystem::remove(path);
}

TEST_CASE("PackageVerifier - verify file with wrong hash fails", "[verifier]") {
    auto path = write_temp_file("abc");
    auto wrong = std::string(64, '0');

    auto result = PackageVerifier::verify(path, wrong);

    CHECK_FALSE(result.passed);
    CHECK(result.computed_hash != wrong);
    CHECK(result.error.empty()); // error field is for I/O errors, not mismatches

    std::filesystem::remove(path);
}

TEST_CASE("PackageVerifier - verify with no expected hash skips comparison", "[verifier]") {
    auto path = write_temp_file("some data");

    auto result = PackageVerifier::verify(path, "");

    CHECK(result.passed);          // no comparison requested
    CHECK(result.hash_skipped());
    CHECK_FALSE(result.computed_hash.empty()); // still computed

    std::filesystem::remove(path);
}

TEST_CASE("PackageVerifier - verify nonexistent file sets error", "[verifier]") {
    auto result = PackageVerifier::verify("/nonexistent/path/file.zip", "");

    CHECK_FALSE(result.passed);
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("PackageVerifier - verify is case-insensitive for hash comparison", "[verifier]") {
    auto path = write_temp_file("abc");

    // Upper-case the expected hash.
    std::string upper = SHA256_ABC;
    for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    auto result = PackageVerifier::verify(path, upper);
    CHECK(result.passed);

    std::filesystem::remove(path);
}

TEST_CASE("PackageVerifier - verify large file produces correct hash", "[verifier]") {
    // 200 KB of 0xAB bytes.
    std::string data(200 * 1024, '\xAB');
    auto path = write_temp_file(data);

    // Compute hash independently using the buffer API.
    std::vector<uint8_t> bytes(data.begin(), data.end());
    std::string expected = PackageVerifier::sha256_hex(bytes);

    auto result = PackageVerifier::verify(path, expected);
    CHECK(result.passed);

    std::filesystem::remove(path);
}
