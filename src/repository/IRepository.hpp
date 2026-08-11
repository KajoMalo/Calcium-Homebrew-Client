#pragma once

#include "AppMetadata.hpp"

#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace calcium::repository {

/// Result of a repository operation.
struct RepoResult {
    bool        success = false;
    std::string error;

    static RepoResult ok()                    { return {true,  {}}; }
    static RepoResult fail(std::string msg)   { return {false, std::move(msg)}; }
};

/// Abstract repository interface.
///
/// A repository knows how to fetch its index and expose application metadata.
/// Multiple concrete implementations can coexist (JSON-over-HTTP, local file,
/// cached copy, etc.).
class IRepository {
public:
    virtual ~IRepository() = default;

    /// Unique identifier for this repository (matches RepositorySource::id).
    virtual const std::string& id()   const = 0;
    /// Human-readable name.
    virtual const std::string& name() const = 0;
    /// Base URL or file path.
    virtual const std::string& url()  const = 0;

    /// Fetch the index from the remote source and populate the app list.
    /// This is a blocking network call; call it from a background thread.
    virtual RepoResult refresh() = 0;

    /// True if refresh() has been called at least once successfully.
    virtual bool is_loaded() const = 0;

    /// All apps in this repository (empty until refresh() succeeds).
    virtual const std::vector<AppMetadata>& apps() const = 0;

    /// Find a single app by id.
    virtual std::optional<AppMetadata> find_app(const std::string& app_id) const = 0;

    /// Timestamp of the last successful refresh (empty if never refreshed).
    virtual const std::string& last_updated() const = 0;
};

} // namespace calcium::repository
