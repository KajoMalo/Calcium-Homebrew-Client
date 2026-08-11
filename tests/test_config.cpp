#include <catch2/catch_test_macros.hpp>
#include "config/Config.hpp"

#include <filesystem>
#include <fstream>

using namespace calcium::config;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static const std::string VALID_CONFIG_JSON = R"({
  "download_dir": "/tmp/calcium/downloads",
  "install_dir":  "/tmp/calcium/installed",
  "log_file":     "/tmp/calcium/calcium.log",
  "log_level":    "debug",
  "download_timeout_secs": 45,
  "download_max_retries":  5,
  "verify_hashes": false,
  "ui": {
    "theme":      "dark",
    "font_size":  18,
    "show_nsfw":  false,
    "reduced_motion": true
  },
  "repositories": [
    {
      "id":      "repo-alpha",
      "name":    "Alpha Repo",
      "url":     "https://alpha.example.com/index.json",
      "enabled": true
    },
    {
      "id":      "repo-beta",
      "name":    "Beta Repo",
      "url":     "https://beta.example.com/index.json",
      "enabled": false
    }
  ]
})";

TEST_CASE("Config - parse valid JSON", "[config]") {
    Config cfg;
    nlohmann::json j = nlohmann::json::parse(VALID_CONFIG_JSON);
    REQUIRE(cfg.from_json(j));

    CHECK(cfg.data().download_dir          == "/tmp/calcium/downloads");
    CHECK(cfg.data().install_dir           == "/tmp/calcium/installed");
    CHECK(cfg.data().log_file              == "/tmp/calcium/calcium.log");
    CHECK(cfg.data().log_level             == "debug");
    CHECK(cfg.data().download_timeout_secs == 45);
    CHECK(cfg.data().download_max_retries  == 5);
    CHECK(cfg.data().verify_hashes         == false);

    CHECK(cfg.data().ui.theme          == "dark");
    CHECK(cfg.data().ui.font_size      == 18);
    CHECK(cfg.data().ui.reduced_motion == true);

    REQUIRE(cfg.data().repositories.size() == 2);
    CHECK(cfg.data().repositories[0].id      == "repo-alpha");
    CHECK(cfg.data().repositories[0].enabled == true);
    CHECK(cfg.data().repositories[1].id      == "repo-beta");
    CHECK(cfg.data().repositories[1].enabled == false);

    CHECK(cfg.is_loaded());
}

TEST_CASE("Config - defaults are applied when field is absent", "[config]") {
    Config cfg;
    nlohmann::json j = nlohmann::json::parse(R"({})");
    REQUIRE(cfg.from_json(j));

    // Default values from the struct.
    CHECK(cfg.data().download_dir          == "./downloads");
    CHECK(cfg.data().log_level             == "info");
    CHECK(cfg.data().download_timeout_secs == 30);
    CHECK(cfg.data().verify_hashes         == true);
    CHECK(cfg.data().repositories.empty());
}

TEST_CASE("Config - load from file (existing)", "[config]") {
    // Write a temp file.
    auto tmp = std::filesystem::temp_directory_path() / "calcium_test_config.json";
    {
        std::ofstream f(tmp);
        f << VALID_CONFIG_JSON;
    }

    Config cfg;
    REQUIRE(cfg.load(tmp));
    CHECK(cfg.data().log_level == "debug");
    CHECK(cfg.data().repositories.size() == 2);

    std::filesystem::remove(tmp);
}

TEST_CASE("Config - load from nonexistent file uses defaults", "[config]") {
    Config cfg;
    REQUIRE(cfg.load("/nonexistent/path/config.json"));
    // Should succeed (uses defaults) rather than returning false.
    CHECK(cfg.is_loaded());
    CHECK(cfg.data().log_level == "info");
}

TEST_CASE("Config - round-trip: to_json then from_json preserves values", "[config]") {
    Config original;
    nlohmann::json j = nlohmann::json::parse(VALID_CONFIG_JSON);
    REQUIRE(original.from_json(j));

    nlohmann::json serialised = original.to_json();

    Config restored;
    REQUIRE(restored.from_json(serialised));

    CHECK(restored.data().download_dir          == original.data().download_dir);
    CHECK(restored.data().install_dir           == original.data().install_dir);
    CHECK(restored.data().log_level             == original.data().log_level);
    CHECK(restored.data().download_timeout_secs == original.data().download_timeout_secs);
    CHECK(restored.data().verify_hashes         == original.data().verify_hashes);
    CHECK(restored.data().repositories.size()   == original.data().repositories.size());
    CHECK(restored.data().repositories[0].id    == original.data().repositories[0].id);
    CHECK(restored.data().repositories[1].url   == original.data().repositories[1].url);
    CHECK(restored.data().ui.theme              == original.data().ui.theme);
}

TEST_CASE("Config - save and reload from disk", "[config]") {
    auto tmp = std::filesystem::temp_directory_path() / "calcium_save_test.json";

    Config cfg;
    nlohmann::json j = nlohmann::json::parse(VALID_CONFIG_JSON);
    REQUIRE(cfg.from_json(j));
    REQUIRE(cfg.save_to(tmp));

    Config reloaded;
    REQUIRE(reloaded.load(tmp));
    CHECK(reloaded.data().log_level          == "debug");
    CHECK(reloaded.data().repositories.size() == 2);
    CHECK(reloaded.data().repositories[0].name == "Alpha Repo");

    std::filesystem::remove(tmp);
}

TEST_CASE("Config - add and remove repository", "[config]") {
    Config cfg;
    nlohmann::json j = nlohmann::json::parse(R"({})");
    REQUIRE(cfg.from_json(j));

    RepositorySource src;
    src.id      = "new-repo";
    src.name    = "New Repo";
    src.url     = "https://new.example.com/index.json";
    src.enabled = true;
    cfg.add_repository(src);

    REQUIRE(cfg.data().repositories.size() == 1);
    CHECK(cfg.data().repositories[0].id == "new-repo");

    cfg.remove_repository("new-repo");
    CHECK(cfg.data().repositories.empty());
}

TEST_CASE("Config - remove nonexistent repository is a no-op", "[config]") {
    Config cfg;
    nlohmann::json j = nlohmann::json::parse(R"({})");
    REQUIRE(cfg.from_json(j));

    // Should not throw or crash.
    cfg.remove_repository("does-not-exist");
    CHECK(cfg.data().repositories.empty());
}

TEST_CASE("Config - malformed JSON returns false", "[config]") {
    Config cfg;
    // Write a temp file with bad JSON.
    auto tmp = std::filesystem::temp_directory_path() / "calcium_bad_config.json";
    {
        std::ofstream f(tmp);
        f << "{this is : not : json}";
    }
    bool loaded = cfg.load(tmp);
    CHECK_FALSE(loaded);
    std::filesystem::remove(tmp);
}
