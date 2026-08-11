#include <catch2/catch_test_macros.hpp>
#include "networking/DownloadManager.hpp"
#include "networking/IHttpClient.hpp"

#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <filesystem>
#include <fstream>

using namespace calcium::networking;

// ─── MockHttpClient ───────────────────────────────────────────────────────────
// Simulates network responses without any real I/O.

class MockHttpClient final : public IHttpClient {
public:
    // Configure outcome before each test.
    bool      should_succeed      = true;
    int       fail_attempts       = 0;   // fail this many attempts then succeed
    uint64_t  file_size_bytes     = 1024;
    int       progress_chunk_count = 4;  // how many progress callbacks to emit

    HttpResponse get(const std::string& /*url*/,
                     const HttpOptions& /*opts*/) override {
        if (m_cancelled.load()) {
            HttpResponse r;
            r.error_message = "Cancelled";
            return r;
        }
        HttpResponse r;
        if (!should_succeed) {
            r.error_message = "Mock network error";
            return r;
        }
        r.status_code = 200;
        r.body = R"({"schema_version":"1.0","apps":[]})";
        return r;
    }

    bool download(const std::string& url,
                  const std::string& dest_path,
                  ProgressCallback on_progress,
                  const HttpOptions& /*opts*/) override {
        if (m_cancelled.load()) return false;

        if (fail_attempts > 0) {
            --fail_attempts;
            return false;
        }

        if (!should_succeed) return false;

        // Write a small dummy file.
        {
            std::ofstream f(dest_path, std::ios::binary);
            std::string data(static_cast<std::size_t>(file_size_bytes), 0x42);
            f.write(data.data(), static_cast<std::streamsize>(data.size()));
        }

        // Emit progress in chunks.
        if (on_progress) {
            uint64_t chunk = file_size_bytes / progress_chunk_count;
            for (int i = 1; i <= progress_chunk_count; ++i) {
                if (m_cancelled.load()) return false;
                on_progress(chunk * i, file_size_bytes);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        return true;
    }

    void cancel() override { m_cancelled.store(true); }
    bool is_cancelled() const override { return m_cancelled.load(); }
    void reset_cancel() { m_cancelled.store(false); }

private:
    std::atomic<bool> m_cancelled{false};
};

// ─── Helper to wait for a download to reach a terminal status ─────────────────

static bool wait_for_terminal(DownloadManager& dm,
                               const std::string& id,
                               int timeout_ms = 3000) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        auto item = dm.find(id);
        if (item) {
            auto s = item->status;
            if (s == DownloadStatus::Completed ||
                s == DownloadStatus::Failed    ||
                s == DownloadStatus::Cancelled) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false; // timed out
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("DownloadManager - successful download reaches Completed", "[download]") {
    auto mock = std::make_shared<MockHttpClient>();
    DownloadManager dm(mock);

    auto tmp = std::filesystem::temp_directory_path() / "calcium_dm_test.bin";

    bool completed = false;
    DownloadCallbacks cbs;
    cbs.on_completed = [&](const DownloadItem&) { completed = true; };
    dm.set_callbacks(cbs);
    dm.set_max_retries(0);

    REQUIRE(dm.enqueue("test.app", "https://example.com/test.bin", tmp.string(), "Test App"));

    REQUIRE(wait_for_terminal(dm, "test.app"));

    auto item = dm.find("test.app");
    REQUIRE(item.has_value());
    CHECK(item->status == DownloadStatus::Completed);
    CHECK(completed);
    CHECK(std::filesystem::exists(tmp));

    std::filesystem::remove(tmp);
}

TEST_CASE("DownloadManager - failed download reaches Failed after retries", "[download]") {
    auto mock = std::make_shared<MockHttpClient>();
    mock->should_succeed = false;

    DownloadManager dm(mock);
    dm.set_max_retries(2);

    bool failed = false;
    DownloadCallbacks cbs;
    cbs.on_failed = [&](const DownloadItem&) { failed = true; };
    dm.set_callbacks(cbs);

    auto tmp = std::filesystem::temp_directory_path() / "calcium_dm_fail.bin";
    REQUIRE(dm.enqueue("fail.app", "https://example.com/fail.bin", tmp.string()));

    REQUIRE(wait_for_terminal(dm, "fail.app", 5000));

    auto item = dm.find("fail.app");
    REQUIRE(item.has_value());
    CHECK(item->status == DownloadStatus::Failed);
    CHECK(failed);
}

TEST_CASE("DownloadManager - cancellation of queued item works immediately", "[download]") {
    auto mock = std::make_shared<MockHttpClient>();
    DownloadManager dm(mock);

    // Enqueue two items; cancel the second before it starts.
    auto tmp1 = std::filesystem::temp_directory_path() / "calcium_dm_q1.bin";
    auto tmp2 = std::filesystem::temp_directory_path() / "calcium_dm_q2.bin";

    dm.enqueue("app.one", "https://example.com/one.bin", tmp1.string());
    dm.enqueue("app.two", "https://example.com/two.bin", tmp2.string());

    // Cancel the queued item before it gets picked up.
    dm.cancel("app.two");

    auto item = dm.find("app.two");
    if (item) {
        CHECK(item->status == DownloadStatus::Cancelled);
    }

    // Wait for first to finish.
    wait_for_terminal(dm, "app.one");

    std::filesystem::remove(tmp1);
    std::filesystem::remove(tmp2);
}

TEST_CASE("DownloadManager - duplicate enqueue returns false", "[download]") {
    auto mock = std::make_shared<MockHttpClient>();
    DownloadManager dm(mock);

    auto tmp = std::filesystem::temp_directory_path() / "calcium_dm_dup.bin";
    REQUIRE(dm.enqueue("dup.app", "https://example.com/dup.bin", tmp.string()));
    CHECK_FALSE(dm.enqueue("dup.app", "https://example.com/dup.bin", tmp.string()));

    wait_for_terminal(dm, "dup.app");
    std::filesystem::remove(tmp);
}

TEST_CASE("DownloadManager - progress callbacks are called", "[download]") {
    auto mock = std::make_shared<MockHttpClient>();
    mock->file_size_bytes      = 4096;
    mock->progress_chunk_count = 4;

    DownloadManager dm(mock);
    dm.set_max_retries(0);

    int progress_count = 0;
    DownloadCallbacks cbs;
    cbs.on_progress = [&](const DownloadItem&) { ++progress_count; };
    dm.set_callbacks(cbs);

    auto tmp = std::filesystem::temp_directory_path() / "calcium_dm_prog.bin";
    dm.enqueue("prog.app", "https://example.com/prog.bin", tmp.string());

    wait_for_terminal(dm, "prog.app");
    CHECK(progress_count >= 1);

    std::filesystem::remove(tmp);
}

TEST_CASE("DownloadManager - all_items returns all known items", "[download]") {
    auto mock = std::make_shared<MockHttpClient>();
    DownloadManager dm(mock);
    dm.set_max_retries(0);

    auto tmp1 = std::filesystem::temp_directory_path() / "calcium_all1.bin";
    auto tmp2 = std::filesystem::temp_directory_path() / "calcium_all2.bin";
    dm.enqueue("all.app1", "https://example.com/all1.bin", tmp1.string());
    dm.enqueue("all.app2", "https://example.com/all2.bin", tmp2.string());

    wait_for_terminal(dm, "all.app1");
    wait_for_terminal(dm, "all.app2");

    auto items = dm.all_items();
    CHECK(items.size() >= 2);

    std::filesystem::remove(tmp1);
    std::filesystem::remove(tmp2);
}

TEST_CASE("DownloadItem - progress() returns correct fraction", "[download]") {
    DownloadItem item;
    item.bytes_total    = 1000;
    item.bytes_received = 250;
    CHECK(item.progress() == Catch::Approx(0.25f).epsilon(0.001f));

    item.bytes_total = 0;
    CHECK(item.progress() == 0.0f);
}

TEST_CASE("DownloadStatus - to_string covers all values", "[download]") {
    CHECK(to_string(DownloadStatus::Queued)      == "Queued");
    CHECK(to_string(DownloadStatus::Downloading) == "Downloading");
    CHECK(to_string(DownloadStatus::Paused)      == "Paused");
    CHECK(to_string(DownloadStatus::Completed)   == "Completed");
    CHECK(to_string(DownloadStatus::Failed)      == "Failed");
    CHECK(to_string(DownloadStatus::Cancelled)   == "Cancelled");
}
