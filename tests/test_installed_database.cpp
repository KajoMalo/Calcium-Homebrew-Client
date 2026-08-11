#include <catch2/catch_test_macros.hpp>
#include "config/InstalledDatabase.hpp"

#include <filesystem>

using namespace calcium::config;

static InstalledRecord make_record(const std::string& id,
                                   const std::string& version = "1.0.0") {
    InstalledRecord r;
    r.app_id         = id;
    r.name           = "App " + id;
    r.version        = version;
    r.install_path   = "/installed/" + id;
    r.content_id     = "";
    r.repo_id        = "test-repo";
    r.installed_size = 1048576;
    r.installed_at   = "2026-01-01T00:00:00Z";
    return r;
}

TEST_CASE("InstalledDatabase - empty on construction", "[db]") {
    InstalledDatabase db;
    CHECK(db.count() == 0);
    CHECK(db.all().empty());
}

TEST_CASE("InstalledDatabase - upsert and is_installed", "[db]") {
    InstalledDatabase db;
    // No path set — save will warn but not crash; upsert still updates in memory.
    db.upsert(make_record("com.example.app1"));

    REQUIRE(db.count() == 1);
    CHECK(db.is_installed("com.example.app1"));
    CHECK_FALSE(db.is_installed("com.example.other"));
}

TEST_CASE("InstalledDatabase - find returns correct record", "[db]") {
    InstalledDatabase db;
    db.upsert(make_record("com.example.app1", "2.0.0"));

    auto rec = db.find("com.example.app1");
    REQUIRE(rec.has_value());
    CHECK(rec->version     == "2.0.0");
    CHECK(rec->install_path == "/installed/com.example.app1");

    auto missing = db.find("com.example.nope");
    CHECK_FALSE(missing.has_value());
}

TEST_CASE("InstalledDatabase - upsert replaces existing record", "[db]") {
    InstalledDatabase db;
    db.upsert(make_record("com.example.app1", "1.0.0"));
    db.upsert(make_record("com.example.app1", "2.0.0")); // upgrade

    REQUIRE(db.count() == 1); // still only one record
    auto rec = db.find("com.example.app1");
    REQUIRE(rec.has_value());
    CHECK(rec->version == "2.0.0");
}

TEST_CASE("InstalledDatabase - remove deletes record", "[db]") {
    InstalledDatabase db;
    db.upsert(make_record("com.example.app1"));
    db.upsert(make_record("com.example.app2"));
    REQUIRE(db.count() == 2);

    db.remove("com.example.app1");
    CHECK(db.count() == 1);
    CHECK_FALSE(db.is_installed("com.example.app1"));
    CHECK(db.is_installed("com.example.app2"));
}

TEST_CASE("InstalledDatabase - remove nonexistent returns false", "[db]") {
    InstalledDatabase db;
    db.upsert(make_record("com.example.app1"));
    CHECK_FALSE(db.remove("com.example.nope"));
    CHECK(db.count() == 1); // unchanged
}

TEST_CASE("InstalledDatabase - clear removes all records", "[db]") {
    InstalledDatabase db;
    db.upsert(make_record("com.example.a"));
    db.upsert(make_record("com.example.b"));
    db.upsert(make_record("com.example.c"));
    REQUIRE(db.count() == 3);

    db.clear();
    CHECK(db.count() == 0);
    CHECK(db.all().empty());
}

TEST_CASE("InstalledDatabase - round-trip JSON serialisation", "[db]") {
    InstalledDatabase db;
    db.upsert(make_record("com.example.app1", "1.2.3"));
    db.upsert(make_record("com.example.app2", "4.5.6"));

    auto j = db.to_json();
    REQUIRE(j.is_array());
    REQUIRE(j.size() == 2);

    InstalledDatabase db2;
    REQUIRE(db2.from_json(j));
    REQUIRE(db2.count() == 2);
    CHECK(db2.is_installed("com.example.app1"));
    CHECK(db2.is_installed("com.example.app2"));

    auto rec = db2.find("com.example.app1");
    REQUIRE(rec.has_value());
    CHECK(rec->version == "1.2.3");
}

TEST_CASE("InstalledDatabase - load and save to disk", "[db]") {
    auto tmp = std::filesystem::temp_directory_path() / "calcium_test_installed.json";

    InstalledDatabase db;
    REQUIRE(db.load(tmp)); // nonexistent — starts empty
    db.upsert(make_record("com.example.savedapp", "3.0.0"));
    REQUIRE(db.save());

    InstalledDatabase db2;
    REQUIRE(db2.load(tmp));
    REQUIRE(db2.count() == 1);
    CHECK(db2.is_installed("com.example.savedapp"));
    auto rec = db2.find("com.example.savedapp");
    REQUIRE(rec.has_value());
    CHECK(rec->version == "3.0.0");

    std::filesystem::remove(tmp);
}

TEST_CASE("InstalledDatabase - from_json rejects non-array", "[db]") {
    InstalledDatabase db;
    nlohmann::json j = nlohmann::json::object();
    CHECK_FALSE(db.from_json(j));
}

TEST_CASE("InstalledDatabase - from_json skips malformed records", "[db]") {
    InstalledDatabase db;
    // One valid, one missing app_id.
    nlohmann::json j = nlohmann::json::array({
        {{"app_id","com.example.good"},{"name","Good"},{"version","1.0"},
         {"install_path","/inst/good"},{"content_id",""},{"repo_id","r"},
         {"installed_size",0},{"installed_at","2026-01-01T00:00:00Z"}},
        // Intentionally corrupt to trigger the skip path.
        42
    });
    // Should not throw; valid records are kept.
    REQUIRE(db.from_json(j));
    CHECK(db.count() == 1);
    CHECK(db.is_installed("com.example.good"));
}
