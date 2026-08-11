#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <map>
#include <optional>

namespace calcium::networking {

/// Progress callback: (bytes_received, total_bytes). total_bytes may be 0 if unknown.
using ProgressCallback = std::function<void(uint64_t bytes_received, uint64_t total_bytes)>;

/// HTTP response from a synchronous request.
struct HttpResponse {
    int                              status_code = 0;
    std::string                      body;
    std::map<std::string, std::string> headers;
    std::string                      error_message; ///< Non-empty on transport failure.

    bool ok()    const { return status_code >= 200 && status_code < 300; }
    bool failed() const { return !error_message.empty() || status_code == 0; }
};

/// Options for a single HTTP request.
struct HttpOptions {
    int  timeout_secs    = 30;
    int  max_retries     = 3;
    bool follow_redirect = true;
    std::map<std::string, std::string> headers;
};

/// Abstract HTTP client interface.
///
/// Callers never depend on libcurl directly; this interface is the only
/// boundary. A mock implementation is used in tests and on PS4 until a
/// native HTTP stack is wired in.
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /// Perform a synchronous HTTP GET and return the full response body.
    virtual HttpResponse get(const std::string& url,
                             const HttpOptions& options = {}) = 0;

    /// Download a URL directly to a file path, reporting progress.
    /// Returns true on success (HTTP 2xx + file written).
    virtual bool download(const std::string& url,
                          const std::string& dest_path,
                          ProgressCallback   on_progress = nullptr,
                          const HttpOptions& options     = {}) = 0;

    /// Cancel any in-progress operation on this client instance.
    virtual void cancel() = 0;

    /// Returns true if a cancellation has been requested.
    virtual bool is_cancelled() const = 0;
};

} // namespace calcium::networking
