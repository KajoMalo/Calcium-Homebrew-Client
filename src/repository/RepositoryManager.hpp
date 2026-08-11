#pragma once

#include "IRepository.hpp"
#include "../config/Config.hpp"
#include "../networking/IHttpClient.hpp"

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

namespace calcium::repository {

/// Manages the lifecycle of all configured repositories.
///
/// Responsibilities:
///   - Build Repository objects from RepositorySource config entries.
///   - Refresh all or individual repositories (blocking, meant for bg threads).
///   - Aggregate apps from all loaded repositories into one flat list.
///   - Notify callers when the app list changes.
class RepositoryManager {
public:
    using RefreshCallback = std::function<void(const std::string& repo_id, bool success)>;

    explicit RepositoryManager(std::shared_ptr<networking::IHttpClient> http_client);

    // ── Configuration ─────────────────────────────────────────────────────

    /// Build repositories from the config. Replaces any existing repositories.
    void configure(const std::vector<config::RepositorySource>& sources);

    /// Add a single repository at runtime.
    void add_repository(std::shared_ptr<IRepository> repo);

    /// Remove a repository by id.
    void remove_repository(const std::string& id);

    // ── Refresh ───────────────────────────────────────────────────────────

    /// Refresh every enabled repository. Calls on_refresh for each one.
    void refresh_all(RefreshCallback on_refresh = nullptr);

    /// Refresh a single repository by id.
    bool refresh(const std::string& repo_id,
                 RefreshCallback on_refresh = nullptr);

    // ── Queries ───────────────────────────────────────────────────────────

    /// Flat list of all apps from all loaded repositories.
    /// Stable sort: alphabetical by name.
    std::vector<AppMetadata> all_apps() const;

    /// Apps filtered by category.
    std::vector<AppMetadata> apps_by_category(const std::string& category) const;

    /// Apps whose name or description contains the search term (case-insensitive).
    std::vector<AppMetadata> search(const std::string& query) const;

    /// Find a specific app by id, searching all repositories.
    std::optional<AppMetadata> find_app(const std::string& app_id) const;

    std::size_t repository_count() const;

    const std::vector<std::shared_ptr<IRepository>>& repositories() const;

    // ── Callbacks ─────────────────────────────────────────────────────────
    void set_on_apps_changed(std::function<void()> cb) {
        m_on_apps_changed = std::move(cb);
    }

private:
    std::shared_ptr<networking::IHttpClient>        m_http;
    mutable std::mutex                              m_mutex;
    std::vector<std::shared_ptr<IRepository>>       m_repos;
    std::function<void()>                           m_on_apps_changed;
};

} // namespace calcium::repository
