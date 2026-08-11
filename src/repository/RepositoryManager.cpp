#include "RepositoryManager.hpp"
#include "Repository.hpp"
#include "../logging/Logger.hpp"

#include <algorithm>
#include <cctype>

namespace calcium::repository {

static constexpr std::string_view TAG = "RepoManager";

// ─── Construction ─────────────────────────────────────────────────────────────

RepositoryManager::RepositoryManager(std::shared_ptr<networking::IHttpClient> http_client)
    : m_http(std::move(http_client))
{}

// ─── Configuration ────────────────────────────────────────────────────────────

void RepositoryManager::configure(const std::vector<config::RepositorySource>& sources) {
    std::lock_guard lock{m_mutex};
    m_repos.clear();

    for (const auto& src : sources) {
        if (!src.enabled) {
            logging::Logger::instance().info(TAG,
                "Skipping disabled repository: " + src.id);
            continue;
        }
        auto repo = std::make_shared<Repository>(src.id, src.name, src.url, m_http);
        m_repos.push_back(std::move(repo));
        logging::Logger::instance().info(TAG,
            "Registered repository: " + src.name + " (" + src.url + ")");
    }
}

void RepositoryManager::add_repository(std::shared_ptr<IRepository> repo) {
    if (!repo) return;
    std::lock_guard lock{m_mutex};
    m_repos.push_back(std::move(repo));
}

void RepositoryManager::remove_repository(const std::string& id) {
    std::lock_guard lock{m_mutex};
    m_repos.erase(
        std::remove_if(m_repos.begin(), m_repos.end(),
            [&](const std::shared_ptr<IRepository>& r) { return r->id() == id; }),
        m_repos.end()
    );
}

// ─── Refresh ──────────────────────────────────────────────────────────────────

void RepositoryManager::refresh_all(RefreshCallback on_refresh) {
    // Take a snapshot of repo pointers to avoid holding the lock during I/O.
    std::vector<std::shared_ptr<IRepository>> snapshot;
    {
        std::lock_guard lock{m_mutex};
        snapshot = m_repos;
    }

    for (const auto& repo : snapshot) {
        auto result = repo->refresh();
        logging::Logger::instance().log(
            result.success ? logging::LogLevel::Info : logging::LogLevel::Error,
            TAG,
            result.success
                ? "Refreshed: " + repo->name()
                : "Failed to refresh '" + repo->name() + "': " + result.error
        );
        if (on_refresh) on_refresh(repo->id(), result.success);
    }

    if (m_on_apps_changed) m_on_apps_changed();
}

bool RepositoryManager::refresh(const std::string& repo_id, RefreshCallback on_refresh) {
    std::shared_ptr<IRepository> target;
    {
        std::lock_guard lock{m_mutex};
        for (const auto& r : m_repos) {
            if (r->id() == repo_id) { target = r; break; }
        }
    }

    if (!target) {
        logging::Logger::instance().warning(TAG, "Refresh: unknown repo id: " + repo_id);
        return false;
    }

    auto result = target->refresh();
    if (on_refresh) on_refresh(repo_id, result.success);
    if (m_on_apps_changed) m_on_apps_changed();
    return result.success;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

std::vector<AppMetadata> RepositoryManager::all_apps() const {
    std::lock_guard lock{m_mutex};
    std::vector<AppMetadata> result;
    for (const auto& repo : m_repos) {
        if (!repo->is_loaded()) continue;
        const auto& apps = repo->apps();
        result.insert(result.end(), apps.begin(), apps.end());
    }
    // Stable sort alphabetically by name.
    std::stable_sort(result.begin(), result.end(),
        [](const AppMetadata& a, const AppMetadata& b) {
            return a.name < b.name;
        });
    return result;
}

std::vector<AppMetadata> RepositoryManager::apps_by_category(const std::string& category) const {
    auto all = all_apps();
    all.erase(
        std::remove_if(all.begin(), all.end(),
            [&](const AppMetadata& m) { return m.category != category; }),
        all.end()
    );
    return all;
}

std::vector<AppMetadata> RepositoryManager::search(const std::string& query) const {
    if (query.empty()) return all_apps();

    // Case-insensitive substring match on name, description, author, tags.
    auto lower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string q = lower(query);

    auto all = all_apps();
    all.erase(
        std::remove_if(all.begin(), all.end(),
            [&](const AppMetadata& m) {
                if (lower(m.name).find(q)        != std::string::npos) return false;
                if (lower(m.description).find(q) != std::string::npos) return false;
                if (lower(m.author).find(q)      != std::string::npos) return false;
                for (const auto& tag : m.tags) {
                    if (lower(tag).find(q) != std::string::npos) return false;
                }
                return true; // doesn't match
            }),
        all.end()
    );
    return all;
}

std::optional<AppMetadata> RepositoryManager::find_app(const std::string& app_id) const {
    std::lock_guard lock{m_mutex};
    for (const auto& repo : m_repos) {
        auto found = repo->find_app(app_id);
        if (found) return found;
    }
    return std::nullopt;
}

std::size_t RepositoryManager::repository_count() const {
    std::lock_guard lock{m_mutex};
    return m_repos.size();
}

const std::vector<std::shared_ptr<IRepository>>& RepositoryManager::repositories() const {
    return m_repos;
}

} // namespace calcium::repository
