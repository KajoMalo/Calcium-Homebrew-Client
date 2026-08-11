#pragma once

#include "IRepository.hpp"
#include "../networking/IHttpClient.hpp"

#include <memory>
#include <mutex>

namespace calcium::repository {

/// JSON-over-HTTP repository implementation.
///
/// Expects the remote URL to serve a JSON document conforming to the
/// Calcium repository schema (schema_version 1.0).
class Repository final : public IRepository {
public:
    Repository(std::string id,
               std::string name,
               std::string url,
               std::shared_ptr<networking::IHttpClient> http_client);

    const std::string& id()   const override { return m_id;   }
    const std::string& name() const override { return m_name; }
    const std::string& url()  const override { return m_url;  }

    RepoResult refresh() override;

    bool is_loaded() const override;

    const std::vector<AppMetadata>& apps() const override;

    std::optional<AppMetadata> find_app(const std::string& app_id) const override;

    const std::string& last_updated() const override;

    /// Load the repository index from a JSON string (used in tests / offline).
    RepoResult load_from_json(const std::string& json_text);

private:
    std::string m_id;
    std::string m_name;
    std::string m_url;

    std::shared_ptr<networking::IHttpClient> m_http;

    mutable std::mutex        m_mutex;
    std::vector<AppMetadata>  m_apps;
    std::string               m_last_updated;
    bool                      m_loaded = false;
};

} // namespace calcium::repository
