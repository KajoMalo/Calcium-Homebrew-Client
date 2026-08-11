// PS4HttpClient.inl
// libSceHttp-backed HTTP client for PS4.
// Included directly by PS4Platform.cpp — not compiled independently.

#pragma once

#include "../../networking/IHttpClient.hpp"
#include "../../logging/Logger.hpp"

#include <orbis/Http.h>
#include <orbis/Ssl.h>

#include <fstream>
#include <vector>
#include <atomic>
#include <cstring>
#include <filesystem>

namespace calcium::networking {

static constexpr std::string_view PS4_HTTP_TAG = "PS4HttpClient";

/// libSceHttp-backed HTTP client.
class PS4HttpClient final : public IHttpClient {
public:
    PS4HttpClient() {
        // Create a persistent libSceHttp context (shared across all requests).
        m_ctx = sceHttpCreateTemplate("CalciumClient/1.0",
                                       ORBIS_HTTP_VERSION_1_1, 1);
        if (m_ctx < 0) {
            logging::Logger::instance().error(PS4_HTTP_TAG,
                "sceHttpCreateTemplate failed: " + std::to_string(m_ctx));
        }
        // Enable SSL peer verification.
        sceHttpsSetSslCallback(m_ctx, nullptr, nullptr);
    }

    ~PS4HttpClient() override {
        if (m_ctx >= 0) sceHttpDeleteTemplate(m_ctx);
    }

    void cancel() override { m_cancelled.store(true); }
    bool is_cancelled() const override { return m_cancelled.load(); }

    // ── GET ───────────────────────────────────────────────────────────────

    HttpResponse get(const std::string& url,
                     const HttpOptions& options) override {
        m_cancelled.store(false);
        HttpResponse resp;

        if (m_ctx < 0) {
            resp.error_message = "No HTTP context";
            return resp;
        }

        for (int attempt = 0; attempt <= options.max_retries; ++attempt) {
            if (m_cancelled.load()) {
                resp.error_message = "Cancelled";
                return resp;
            }

            int conn = sceHttpCreateConnectionWithURL(m_ctx, url.c_str(), 1);
            if (conn < 0) {
                resp.error_message = "sceHttpCreateConnectionWithURL failed";
                continue;
            }

            int req = sceHttpCreateRequestWithURL(conn, ORBIS_HTTP_METHOD_GET,
                                                  url.c_str(), 0);
            if (req < 0) {
                sceHttpDeleteConnection(conn);
                resp.error_message = "sceHttpCreateRequestWithURL failed";
                continue;
            }

            sceHttpSetConnectTimeOut(req, options.timeout_secs * 1000u);
            sceHttpSetRecvTimeOut(req, options.timeout_secs * 1000u);

            int ret = sceHttpSendRequest(req, nullptr, 0);
            if (ret < 0) {
                sceHttpDeleteRequest(req);
                sceHttpDeleteConnection(conn);
                resp.error_message = "sceHttpSendRequest failed: " + std::to_string(ret);
                continue;
            }

            // Read status code.
            int status = 0;
            sceHttpGetStatusCode(req, &status);
            resp.status_code = status;

            // Read body in chunks.
            resp.body.clear();
            uint8_t chunk[8192];
            int bytes_read;
            while ((bytes_read = sceHttpReadData(req, chunk, sizeof(chunk))) > 0) {
                if (m_cancelled.load()) break;
                resp.body.append(reinterpret_cast<char*>(chunk),
                                 static_cast<std::size_t>(bytes_read));
            }

            sceHttpDeleteRequest(req);
            sceHttpDeleteConnection(conn);

            if (bytes_read < 0) {
                resp.error_message = "sceHttpReadData failed: " + std::to_string(bytes_read);
                if (attempt < options.max_retries) continue;
            }
            break; // success or non-retryable error
        }

        return resp;
    }

    // ── Download ──────────────────────────────────────────────────────────

    bool download(const std::string& url,
                  const std::string& dest_path,
                  ProgressCallback   on_progress,
                  const HttpOptions& options) override {
        m_cancelled.store(false);

        if (m_ctx < 0) return false;

        // Ensure parent directory exists.
        {
            std::filesystem::path p{dest_path};
            if (p.has_parent_path()) {
                std::error_code ec;
                std::filesystem::create_directories(p.parent_path(), ec);
            }
        }

        for (int attempt = 0; attempt <= options.max_retries; ++attempt) {
            if (m_cancelled.load()) return false;

            std::ofstream file(dest_path, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                logging::Logger::instance().error(PS4_HTTP_TAG,
                    "Cannot open dest file: " + dest_path);
                return false;
            }

            int conn = sceHttpCreateConnectionWithURL(m_ctx, url.c_str(), 1);
            if (conn < 0) continue;

            int req = sceHttpCreateRequestWithURL(conn, ORBIS_HTTP_METHOD_GET,
                                                  url.c_str(), 0);
            if (req < 0) { sceHttpDeleteConnection(conn); continue; }

            sceHttpSetConnectTimeOut(req, options.timeout_secs * 1000u);
            sceHttpSetRecvTimeOut(req, options.timeout_secs * 1000u);

            if (sceHttpSendRequest(req, nullptr, 0) < 0) {
                sceHttpDeleteRequest(req);
                sceHttpDeleteConnection(conn);
                continue;
            }

            int status = 0;
            sceHttpGetStatusCode(req, &status);

            // Get Content-Length for progress.
            uint64_t content_length = 0;
            {
                char* cl_val = nullptr;
                if (sceHttpGetResponseContentLength(req, nullptr, &content_length) < 0) {
                    content_length = 0;
                }
            }

            uint64_t total_received = 0;
            uint8_t  chunk[65536];
            int      bytes_read;
            bool     read_error = false;

            while ((bytes_read = sceHttpReadData(req, chunk, sizeof(chunk))) > 0) {
                if (m_cancelled.load()) {
                    sceHttpDeleteRequest(req);
                    sceHttpDeleteConnection(conn);
                    file.close();
                    std::filesystem::remove(dest_path);
                    return false;
                }
                file.write(reinterpret_cast<const char*>(chunk),
                           static_cast<std::streamsize>(bytes_read));
                total_received += static_cast<uint64_t>(bytes_read);
                if (on_progress) on_progress(total_received, content_length);
            }

            if (bytes_read < 0) {
                read_error = true;
                logging::Logger::instance().warning(PS4_HTTP_TAG,
                    "Read error on attempt " + std::to_string(attempt));
            }

            sceHttpDeleteRequest(req);
            sceHttpDeleteConnection(conn);
            file.close();

            if (!read_error && status >= 200 && status < 300) {
                return true;
            }
            std::filesystem::remove(dest_path);
        }

        return false;
    }

private:
    int               m_ctx = -1;
    std::atomic<bool> m_cancelled{false};
};

} // namespace calcium::networking
