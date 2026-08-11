#include <catch2/catch_test_macros.hpp>
#include "repository/MetadataParser.hpp"
#include "repository/AppMetadata.hpp"

using namespace calcium::repository;

// ─── Minimal valid index ──────────────────────────────────────────────────────

static const std::string VALID_INDEX = R"({
  "schema_version": "1.0",
  "repository": { "name": "Test Repo", "url": "https://example.com" },
  "apps": [
    {
      "id": "com.example.testapp",
      "name": "Test App",
      "version": "1.0.0",
      "author": "Test Author",
      "category": "utility",
      "description": "A test application.",
      "long_description": "Extended description.",
      "download_url": "https://example.com/testapp-1.0.0.zip",
      "download_size": 1048576,
      "installed_size": 2097152,
      "sha256": "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899",
      "changelog": "Initial release.",
      "min_firmware": "9.00",
      "tags": ["test", "utility"],
      "screenshots": ["https://example.com/ss1.png"],
      "compatibility": {
        "status": "verified",
        "tested_firmware": ["9.00"],
        "notes": "Works great."
      },
      "updated_at": "2026-01-01T00:00:00Z"
    }
  ]
})";

TEST_CASE("MetadataParser - valid index parses successfully", "[parser]") {
    auto result = MetadataParser::parse_index(VALID_INDEX);

    REQUIRE(result.ok());
    REQUIRE(result.value->size() == 1);

    const auto& app = result.value->at(0);
    CHECK(app.id          == "com.example.testapp");
    CHECK(app.name        == "Test App");
    CHECK(app.version     == "1.0.0");
    CHECK(app.author      == "Test Author");
    CHECK(app.category    == "utility");
    CHECK(app.description == "A test application.");
    CHECK(app.download_url == "https://example.com/testapp-1.0.0.zip");
    CHECK(app.download_size  == 1048576);
    CHECK(app.installed_size == 2097152);
    CHECK(app.sha256      == "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899");
    CHECK(app.min_firmware == "9.00");
    CHECK(app.tags.size() == 2);
    CHECK(app.screenshot_urls.size() == 1);
    CHECK(app.compatibility.status == CompatStatus::Verified);
    CHECK(app.compatibility.tested_firmware.size() == 1);
    CHECK(app.updated_at == "2026-01-01T00:00:00Z");
}

TEST_CASE("MetadataParser - empty input returns failure", "[parser]") {
    auto result = MetadataParser::parse_index("");
    REQUIRE_FALSE(result.ok());
    REQUIRE_FALSE(result.error.empty());
}

TEST_CASE("MetadataParser - malformed JSON returns failure", "[parser]") {
    auto result = MetadataParser::parse_index("{not valid json}}}");
    REQUIRE_FALSE(result.ok());
}

TEST_CASE("MetadataParser - missing apps array returns failure", "[parser]") {
    auto result = MetadataParser::parse_index(R"({"schema_version":"1.0"})");
    REQUIRE_FALSE(result.ok());
    CHECK(result.error.find("apps") != std::string::npos);
}

TEST_CASE("MetadataParser - missing required field skips entry", "[parser]") {
    // 'download_url' is missing — entry should be skipped, not crash.
    const std::string json = R"({
      "schema_version": "1.0",
      "apps": [
        {
          "id": "com.example.noop",
          "name": "No URL App",
          "version": "1.0",
          "author": "Someone",
          "download_url": ""
        }
      ]
    })";
    auto result = MetadataParser::parse_index(json);
    // Parse succeeds but yields 0 valid apps (download_url empty fails validation).
    REQUIRE(result.ok());
    CHECK(result.value->empty());
}

TEST_CASE("MetadataParser - invalid sha256 skips entry", "[parser]") {
    const std::string json = R"({
      "schema_version": "1.0",
      "apps": [
        {
          "id": "com.example.badsha",
          "name": "Bad SHA App",
          "version": "1.0",
          "author": "Someone",
          "download_url": "https://example.com/pkg.zip",
          "sha256": "NOTAHASHVALUE"
        }
      ]
    })";
    auto result = MetadataParser::parse_index(json);
    REQUIRE(result.ok());
    CHECK(result.value->empty());
}

TEST_CASE("MetadataParser - multiple apps parsed correctly", "[parser]") {
    const std::string json = R"({
      "schema_version": "1.0",
      "apps": [
        {
          "id": "com.example.app1",
          "name": "App One",
          "version": "1.0",
          "author": "Alice",
          "download_url": "https://example.com/app1.zip"
        },
        {
          "id": "com.example.app2",
          "name": "App Two",
          "version": "2.0",
          "author": "Bob",
          "download_url": "https://example.com/app2.zip"
        }
      ]
    })";
    auto result = MetadataParser::parse_index(json);
    REQUIRE(result.ok());
    REQUIRE(result.value->size() == 2);
    CHECK(result.value->at(0).id == "com.example.app1");
    CHECK(result.value->at(1).id == "com.example.app2");
}

TEST_CASE("MetadataParser - compat status parsing", "[parser]") {
    CHECK(parse_compat_status("verified") == CompatStatus::Verified);
    CHECK(parse_compat_status("partial")  == CompatStatus::Partial);
    CHECK(parse_compat_status("broken")   == CompatStatus::Broken);
    CHECK(parse_compat_status("unknown")  == CompatStatus::Unknown);
    CHECK(parse_compat_status("garbage")  == CompatStatus::Unknown);
}

TEST_CASE("MetadataParser - unknown schema_version still parses", "[parser]") {
    const std::string json = R"({
      "schema_version": "99.0",
      "apps": [
        {
          "id": "com.example.futureapp",
          "name": "Future App",
          "version": "1.0",
          "author": "Future Dev",
          "download_url": "https://example.com/future.zip"
        }
      ]
    })";
    auto result = MetadataParser::parse_index(json);
    // Should log a warning but still succeed.
    REQUIRE(result.ok());
    CHECK(result.value->size() == 1);
}

TEST_CASE("MetadataParser - validate rejects empty id", "[parser]") {
    AppMetadata m;
    m.id = "";
    m.name = "App";
    m.version = "1.0";
    m.author  = "Dev";
    m.download_url = "https://example.com/x.zip";
    auto err = MetadataParser::validate(m);
    REQUIRE_FALSE(err.empty());
    CHECK(err.find("id") != std::string::npos);
}

TEST_CASE("MetadataParser - validate accepts valid metadata", "[parser]") {
    AppMetadata m;
    m.id           = "com.example.good";
    m.name         = "Good App";
    m.version      = "1.0.0";
    m.author       = "Dev";
    m.download_url = "https://example.com/good.zip";
    auto err = MetadataParser::validate(m);
    CHECK(err.empty());
}

TEST_CASE("MetadataParser - has_update returns correct result", "[metadata]") {
    AppMetadata m;
    m.id = "com.test.app";
    m.version = "2.0.0";

    m.is_installed = false;
    m.installed_version = "1.0.0";
    CHECK_FALSE(m.has_update()); // not installed

    m.is_installed = true;
    m.installed_version = "";
    CHECK_FALSE(m.has_update()); // no installed version known

    m.is_installed = true;
    m.installed_version = "1.0.0";
    CHECK(m.has_update()); // newer version available

    m.is_installed = true;
    m.installed_version = "2.0.0";
    CHECK_FALSE(m.has_update()); // same version
}
