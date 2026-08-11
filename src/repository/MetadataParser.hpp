#pragma once

#include "AppMetadata.hpp"

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace calcium::repository {

/// Result of a parse attempt. Carries either a value or a human-readable error.
template<typename T>
struct ParseResult {
    std::optional<T> value;
    std::string      error;

    bool ok() const { return value.has_value(); }

    static ParseResult success(T v)           { return {std::move(v), {}}; }
    static ParseResult failure(std::string e) { return {std::nullopt, std::move(e)}; }
};

/// Parses JSON into strongly-typed metadata structures.
///
/// All parsing is non-throwing; errors are returned as ParseResult::failure.
class MetadataParser {
public:
    /// Parse a full repository index JSON document.
    /// Returns a list of AppMetadata records on success.
    static ParseResult<std::vector<AppMetadata>>
        parse_index(const std::string& json_text);

    /// Parse a single app metadata object from a JSON value.
    static ParseResult<AppMetadata>
        parse_app(const nlohmann::json& j);

    /// Parse a CompatInfo block.
    static CompatInfo parse_compat(const nlohmann::json& j);

    /// Validate that a parsed AppMetadata has all required fields.
    /// Returns an empty string on success, or a description of the first problem.
    static std::string validate(const AppMetadata& meta);
};

} // namespace calcium::repository
