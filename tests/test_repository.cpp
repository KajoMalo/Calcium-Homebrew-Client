#include <catch2/catch_test_macros.hpp>
#include "repository/Repository.hpp"
#include "repository/RepositoryManager.hpp"
#include "repository/MetadataParser.hpp"
#include "config/Config.hpp"
#include "networking/IHttpClient.hpp"

#include <filesystem>
#include <fstream>

using namespace calcium::repository;
using namespace calcium::networking;

// ─── MockHttpClient for repository tests ─────────────────────────────────────

class RepoMockHttpClient final : public IHttpClient {
public:
    std::string response_body;
    int         status_code    = 200;
    bool        should_fail    = false;
    std::string error_message;

    HttpResponse get(const std::string& /*url*/,
                     const HttpOptions& /*opts*/) override {
        HttpResponse r;
        if (should_fail) {
            r.error_message = error_message.empty() ? "Mock failure" : error_message;
            return r;
        }
        r.status_code = status_code;
        r.body        = response_body;
        return r;
    }

    bool download(const std::string&, const std::string&,
                  ProgressCallback, const HttpOptions&) override { return false; }
    void cancel() override {}
    bool is_cancelled() const override { return false; }
};

// ─── Fixture JSON ─────────────────────────────────────────────────────────────

static const std::string REPO_JSON = R"({
  "schema_version": "1.0",
  "repository": {"name":"Test Repo","url":"https://test.example.com"},
  "apps": [
    {
      "id": "com.test.alpha",
      "name": "Alpha",
      "version": "1.0",
      "author": "Dev",
      "category": "emulator",
      "description": "Alpha app.",
      "download_url": "https://test.example.com/alpha.zip",
      "tags": ["emulator","retro"]
    },
    {
      "id": "com.test.beta",
      "name": "Beta",
      "version": "2.0",
      "author": "Dev",
      "category": "utility",
      "description": "Beta utility.",
      "download_url": "https://test.example.com/beta.zip",
      "tags": ["utility","network"]
    },
    {
      "id": "com.test.gamma",
      "name": "Gamma Player",
      "version": "3.1",
      "author": "MediaTeam",
      "category": "media",
      "description": "Play media files.",
      "download_url": "https://test.example.com/gamma.zip",
      "tags": ["media","video"]
    }
  ]
})";

// ─── Repository tests ─────────────────────────────────────────────────────────

TEST_CASE("Repository - load_from_json populates app list", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);

    auto result = repo.load_from_json(REPO_JSON);
    REQUIRE(result.success);
    REQUIRE(repo.is_loaded());
    REQUIRE(repo.apps().size() == 3);
}

TEST_CASE("Repository - app fields are parsed correctly", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);
    repo.load_from_json(REPO_JSON);

    const auto& apps = repo.apps();
    CHECK(apps[0].id       == "com.test.alpha");
    CHECK(apps[0].name     == "Alpha");
    CHECK(apps[0].category == "emulator");
    CHECK(apps[0].repo_id  == "test-repo"); // stamped by Repository
    CHECK(apps[1].id       == "com.test.beta");
    CHECK(apps[1].version  == "2.0");
}

TEST_CASE("Repository - find_app locates by id", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);
    repo.load_from_json(REPO_JSON);

    auto found = repo.find_app("com.test.gamma");
    REQUIRE(found.has_value());
    CHECK(found->name == "Gamma Player");

    auto missing = repo.find_app("com.example.nope");
    CHECK_FALSE(missing.has_value());
}

TEST_CASE("Repository - refresh succeeds with valid HTTP response", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    http->response_body = REPO_JSON;
    http->status_code   = 200;

    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);
    auto result = repo.refresh();

    REQUIRE(result.success);
    CHECK(repo.apps().size() == 3);
}

TEST_CASE("Repository - refresh fails on network error", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    http->should_fail = true;

    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);
    auto result = repo.refresh();

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error.empty());
    CHECK_FALSE(repo.is_loaded());
}

TEST_CASE("Repository - refresh fails on HTTP 404", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    http->status_code   = 404;
    http->response_body = "Not Found";

    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);
    auto result = repo.refresh();

    CHECK_FALSE(result.success);
    CHECK(result.error.find("404") != std::string::npos);
}

TEST_CASE("Repository - refresh fails on malformed JSON", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    http->response_body = "{not: valid: json}}}";

    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);
    auto result = repo.refresh();

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("Repository - last_updated is set after successful refresh", "[repo]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    http->response_body = REPO_JSON;

    Repository repo("test-repo", "Test Repo", "https://test.example.com", http);
    CHECK(repo.last_updated().empty());

    repo.refresh();
    CHECK_FALSE(repo.last_updated().empty());
}

TEST_CASE("Repository - load via file:// scheme", "[repo]") {
    // Write the fixture to a temp file.
    auto tmp = std::filesystem::temp_directory_path() / "calcium_repo_test.json";
    {
        std::ofstream f(tmp);
        f << REPO_JSON;
    }

    auto http = std::make_shared<RepoMockHttpClient>();
    Repository repo("file-repo", "File Repo", "file://" + tmp.string(), http);

    auto result = repo.refresh();
    REQUIRE(result.success);
    CHECK(repo.apps().size() == 3);

    std::filesystem::remove(tmp);
}

// ─── RepositoryManager tests ──────────────────────────────────────────────────

TEST_CASE("RepositoryManager - configure builds repos from config sources", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);

    std::vector<calcium::config::RepositorySource> sources = {
        {"repo-1", "Repo One",  "https://one.example.com",  true},
        {"repo-2", "Repo Two",  "https://two.example.com",  true},
        {"repo-3", "Repo Three","https://three.example.com", false}, // disabled
    };
    mgr.configure(sources);

    // Only two enabled repos should be registered.
    CHECK(mgr.repository_count() == 2);
}

TEST_CASE("RepositoryManager - all_apps aggregates across repos", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    http->response_body = REPO_JSON;

    RepositoryManager mgr(http);

    auto repo1 = std::make_shared<Repository>("r1","Repo1","https://r1.example.com", http);
    auto repo2 = std::make_shared<Repository>("r2","Repo2","https://r2.example.com", http);
    repo1->load_from_json(REPO_JSON);
    repo2->load_from_json(REPO_JSON);

    mgr.add_repository(repo1);
    mgr.add_repository(repo2);

    auto all = mgr.all_apps();
    // 3 apps from each repo = 6 total.
    CHECK(all.size() == 6);
}

TEST_CASE("RepositoryManager - all_apps are sorted by name", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);

    auto repo = std::make_shared<Repository>("r1","Repo1","https://r1.example.com", http);
    repo->load_from_json(REPO_JSON);
    mgr.add_repository(repo);

    auto all = mgr.all_apps();
    for (std::size_t i = 1; i < all.size(); ++i) {
        CHECK(all[i - 1].name <= all[i].name);
    }
}

TEST_CASE("RepositoryManager - apps_by_category filters correctly", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    repo->load_from_json(REPO_JSON);
    mgr.add_repository(repo);

    auto emulators = mgr.apps_by_category("emulator");
    REQUIRE(emulators.size() == 1);
    CHECK(emulators[0].id == "com.test.alpha");

    auto utilities = mgr.apps_by_category("utility");
    REQUIRE(utilities.size() == 1);
    CHECK(utilities[0].id == "com.test.beta");

    auto none = mgr.apps_by_category("nonexistent");
    CHECK(none.empty());
}

TEST_CASE("RepositoryManager - search finds by name", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    repo->load_from_json(REPO_JSON);
    mgr.add_repository(repo);

    auto results = mgr.search("gamma");
    REQUIRE(results.size() == 1);
    CHECK(results[0].id == "com.test.gamma");
}

TEST_CASE("RepositoryManager - search is case-insensitive", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    repo->load_from_json(REPO_JSON);
    mgr.add_repository(repo);

    auto r1 = mgr.search("ALPHA");
    auto r2 = mgr.search("alpha");
    auto r3 = mgr.search("Alpha");
    CHECK(r1.size() == r2.size());
    CHECK(r2.size() == r3.size());
    CHECK(r1.size() == 1);
}

TEST_CASE("RepositoryManager - search by tag finds results", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    repo->load_from_json(REPO_JSON);
    mgr.add_repository(repo);

    auto results = mgr.search("network");
    REQUIRE(results.size() == 1);
    CHECK(results[0].id == "com.test.beta");
}

TEST_CASE("RepositoryManager - empty search returns all apps", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    repo->load_from_json(REPO_JSON);
    mgr.add_repository(repo);

    auto all   = mgr.all_apps();
    auto empty = mgr.search("");
    CHECK(all.size() == empty.size());
}

TEST_CASE("RepositoryManager - find_app locates across repos", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    repo->load_from_json(REPO_JSON);
    mgr.add_repository(repo);

    auto found = mgr.find_app("com.test.beta");
    REQUIRE(found.has_value());
    CHECK(found->name == "Beta");

    auto missing = mgr.find_app("com.example.nope");
    CHECK_FALSE(missing.has_value());
}

TEST_CASE("RepositoryManager - remove_repository shrinks count", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    mgr.add_repository(repo);
    CHECK(mgr.repository_count() == 1);

    mgr.remove_repository("r1");
    CHECK(mgr.repository_count() == 0);
}

TEST_CASE("RepositoryManager - on_apps_changed callback fires after refresh_all", "[repomgr]") {
    auto http = std::make_shared<RepoMockHttpClient>();
    http->response_body = REPO_JSON;

    RepositoryManager mgr(http);
    auto repo = std::make_shared<Repository>("r1","R1","https://r1.example.com", http);
    mgr.add_repository(repo);

    bool fired = false;
    mgr.set_on_apps_changed([&] { fired = true; });
    mgr.refresh_all();

    CHECK(fired);
}
