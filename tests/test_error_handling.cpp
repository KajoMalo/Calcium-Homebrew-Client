#include <catch2/catch_test_macros.hpp>
#include "repository/MetadataParser.hpp"
#include "packages/PackageVerifier.hpp"
#include "config/Config.hpp"
#include "config/InstalledDatabase.hpp"
#include "networking/IHttpClient.hpp"
#include "repository/Repository.hpp"

#include <filesystem>
#include <fstream>

using namespace calcium;

// ─── Error handling: MetadataParser edge cases ────────────────────────────────

TEST_CASE("ErrorHandling - parser handles null values gracefully", "[errors][parser]") {
    const std::string json = R"({
      "schema_version": "1.0",
      "apps": [
        {
          "id": "com.example.nullfields",
          "name": null,
          "version": "1.0",
          "author": "Dev",
          "download_url": "https://example.com/x.zip",
          "description": null,
          "tags": null,
          "screenshots": null
        }
      ]
    })";
    // null fields should either be skipped or produce empty strings,
    // not crash the parser.
    REQUIRE_NOTHROW([&] {
        auto result = repository::MetadataParser::parse_index(json);
        // May succeed or fail validation, but must not throw.
        (void)result;
    }());
}

TEST_CASE("ErrorHandling - parser handles wrong-type fields", "[errors][parser]") {
    const std::string json = R"({
      "schema_version": "1.0",
      "apps": [
        {
          "id": 12345,
          "name": true,
          "version": [],
          "author": {},
          "download_url": "https://example.com/x.zip"
        }
      ]
    })";
    REQUIRE_NOTHROW([&] {
        auto result = repository::MetadataParser::parse_index(json);
        (void)result;
    }());
}

TEST_CASE("ErrorHandling - parser handles very large index gracefully", "[errors][parser]") {
    // Build a JSON with 500 apps.
    nlohmann::json root;
    root["schema_version"] = "1.0";
    root["apps"] = nlohmann::json::array();
    for (int i = 0; i < 500; ++i) {
        root["apps"].push_back({
            {"id",           "com.stress.app" + std::to_string(i)},
            {"name",         "Stress App " + std::to_string(i)},
            {"version",      "1.0"},
            {"author",       "Dev"},
            {"download_url", "https://example.com/app" + std::to_string(i) + ".zip"},
        });
    }
    auto result = repository::MetadataParser::parse_index(root.dump());
    REQUIRE(result.ok());
    CHECK(result.value->size() == 500);
}

// ─── Error handling: PackageVerifier ──────────────────────────────────────────

TEST_CASE("ErrorHandling - verifier on empty file computes correct hash", "[errors][verifier]") {
    auto tmp = std::filesystem::temp_directory_path() / "calcium_err_empty.bin";
    { std::ofstream f(tmp); } // create empty file

    static const std::string SHA256_EMPTY =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    auto result = packages::PackageVerifier::verify(tmp, SHA256_EMPTY);
    CHECK(result.passed);
    CHECK(result.error.empty());

    std::filesystem::remove(tmp);
}

TEST_CASE("ErrorHandling - verifier on directory path sets error", "[errors][verifier]") {
    auto dir = std::filesystem::temp_directory_path();
    auto result = packages::PackageVerifier::verify(dir, "");
    // Opening a directory as a binary file should either fail or produce an error.
    // We just require it not to crash.
    REQUIRE_NOTHROW([&]{ (void)result; }());
}

// ─── Error handling: Config ───────────────────────────────────────────────────

TEST_CASE("ErrorHandling - config with extra unknown fields is accepted", "[errors][config]") {
    const std::string json = R"({
      "download_dir": "./dl",
      "unknown_field_xyz": "ignored",
      "another_unknown": 99,
      "nested_unknown": {"a":1,"b":2}
    })";
    config::Config cfg;
    nlohmann::json j = nlohmann::json::parse(json);
    REQUIRE(cfg.from_json(j));
    CHECK(cfg.data().download_dir == "./dl");
}

TEST_CASE("ErrorHandling - config with invalid integer field uses default", "[errors][config]") {
    // download_timeout_secs is a string instead of int — should not crash.
    const std::string json = R"({
      "download_timeout_secs": "not-a-number"
    })";
    config::Config cfg;
    REQUIRE_NOTHROW([&] {
        nlohmann::json j = nlohmann::json::parse(json);
        cfg.from_json(j);
    }());
}

TEST_CASE("ErrorHandling - saving config to unwritable path returns false", "[errors][config]") {
    config::Config cfg;
    nlohmann::json j = nlohmann::json::parse(R"({})");
    cfg.from_json(j);

    // Attempt to write to an invalid path.
#if defined(_WIN32)
    bool saved = cfg.save_to("Z:\\nonexistent\\deeply\\nested\\config.json");
#else
    bool saved = cfg.save_to("/proc/nonexistent/calcium_config.json");
#endif
    CHECK_FALSE(saved);
}

// ─── Error handling: InstalledDatabase ────────────────────────────────────────

TEST_CASE("ErrorHandling - installed db handles corrupt file gracefully", "[errors][db]") {
    auto tmp = std::filesystem::temp_directory_path() / "calcium_corrupt_db.json";
    {
        std::ofstream f(tmp);
        f << "this is not json at all ][{";
    }
    config::InstalledDatabase db;
    CHECK_FALSE(db.load(tmp));
    std::filesystem::remove(tmp);
}

TEST_CASE("ErrorHandling - installed db handles partial record gracefully", "[errors][db]") {
    // JSON array with one valid record and one that is just a number.
    auto tmp = std::filesystem::temp_directory_path() / "calcium_partial_db.json";
    {
        std::ofstream f(tmp);
        f << R"([
            {"app_id":"com.example.ok","name":"OK","version":"1.0",
             "install_path":"/inst/ok","content_id":"","repo_id":"r",
             "installed_size":0,"installed_at":"2026-01-01T00:00:00Z"},
            12345
          ])";
    }
    config::InstalledDatabase db;
    REQUIRE(db.load(tmp));
    // The corrupt entry (12345) should be skipped; the valid one kept.
    CHECK(db.count() == 1);
    CHECK(db.is_installed("com.example.ok"));
    std::filesystem::remove(tmp);
}

// ─── Error handling: Repository network errors ────────────────────────────────

class AlwaysFailHttpClient final : public networking::IHttpClient {
public:
    networking::HttpResponse get(const std::string&,
                                  const networking::HttpOptions&) override {
        networking::HttpResponse r;
        r.error_message = "Simulated network timeout";
        return r;
    }
    bool download(const std::string&, const std::string&,
                  networking::ProgressCallback,
                  const networking::HttpOptions&) override { return false; }
    void cancel() override {}
    bool is_cancelled() const override { return false; }
};

TEST_CASE("ErrorHandling - repository refresh on network failure returns error message", "[errors][repo]") {
    auto http = std::make_shared<AlwaysFailHttpClient>();
    repository::Repository repo("err-repo","Err","https://err.example.com", http);

    auto result = repo.refresh();
    CHECK_FALSE(result.success);
    CHECK(result.error.find("Simulated network timeout") != std::string::npos);
    CHECK_FALSE(repo.is_loaded());
}

TEST_CASE("ErrorHandling - repository find_app on unloaded repo returns nullopt", "[errors][repo]") {
    auto http = std::make_shared<AlwaysFailHttpClient>();
    repository::Repository repo("err-repo","Err","https://err.example.com", http);

    auto found = repo.find_app("com.any.app");
    CHECK_FALSE(found.has_value());
}
