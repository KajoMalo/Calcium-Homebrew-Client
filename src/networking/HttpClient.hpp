#pragma once

#include "IHttpClient.hpp"
#include <atomic>

namespace calcium::networking {

/// libcurl-backed HTTP client.
///
/// When CALCIUM_USE_CURL is not defined (e.g. PS4 or test builds without curl)
/// this class falls back to a stub that returns an error on every call.
/// Platform-specific HTTP stacks should provide their own IHttpClient
/// implementation behind the same interface.
class HttpClient final : public IHttpClient {
public:
    HttpClient();
    ~HttpClient() override;

    HttpResponse get(const std::string& url,
                     const HttpOptions& options = {}) override;

    bool download(const std::string& url,
                  const std::string& dest_path,
                  ProgressCallback   on_progress = nullptr,
                  const HttpOptions& options     = {}) override;

    void cancel() override;
    bool is_cancelled() const override;

private:
    std::atomic<bool> m_cancelled{false};

#ifdef CALCIUM_USE_CURL
    // libcurl global init is done once per process in the constructor of the
    // first HttpClient instance, tracked by a reference count.
    static void global_init();
    static void global_cleanup();
#endif
};

} // namespace calcium::networking
