/**
 * @file ProtectedProjectDetector.hpp
 * @brief Detects project roots that should be protected from file sorting.
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief How aggressively a detected project root should be protected.
 */
enum class ProtectedProjectStrength {
    /** @brief Strong signal; recursive scans should skip this root. */
    Strong,
    /** @brief Weak signal; useful for diagnostics but not skipped automatically. */
    Weak
};

/**
 * @brief Declarative rule for recognizing a protected project root.
 */
struct ProtectedProjectRule {
    /** @brief Stable rule identifier. */
    std::string id;
    /** @brief User-facing project type name. */
    std::string name;
    /** @brief Relative marker paths that must all exist. */
    std::vector<std::filesystem::path> required_paths;
    /** @brief Relative marker path groups where at least one path per group must exist. */
    std::vector<std::vector<std::filesystem::path>> any_path_groups;
    /** @brief Root entry suffixes where at least one direct child must match. */
    std::vector<std::string> root_entry_suffixes_any;
    /** @brief Protection strength for this rule. */
    ProtectedProjectStrength strength{ProtectedProjectStrength::Strong};
    /** @brief Explanation logged when the rule matches. */
    std::string reason;
};

/**
 * @brief Result of matching a protected project rule.
 */
struct ProtectedProjectMatch {
    /** @brief Matching project root. */
    std::filesystem::path root;
    /** @brief Rule identifier. */
    std::string id;
    /** @brief Project type name. */
    std::string name;
    /** @brief Protection strength. */
    ProtectedProjectStrength strength{ProtectedProjectStrength::Strong};
    /** @brief Explanation for the match. */
    std::string reason;
};

/**
 * @brief Returns the built-in protected-project rules.
 * @return Declarative detector rules in priority order.
 */
const std::vector<ProtectedProjectRule>& built_in_protected_project_rules();

/**
 * @brief Evaluates protected-project rules against candidate directories.
 */
class ProtectedProjectDetector {
public:
    /**
     * @brief Construct a detector using the built-in registry.
     */
    ProtectedProjectDetector();

    /**
     * @brief Construct a detector with explicit rules.
     * @param rules Rule registry to evaluate.
     */
    explicit ProtectedProjectDetector(std::vector<ProtectedProjectRule> rules);

    /**
     * @brief Detect whether a directory is a protected project root.
     * @param directory Candidate directory.
     * @return Matched project metadata, or std::nullopt when no rule matches.
     */
    std::optional<ProtectedProjectMatch> detect(const std::filesystem::path& directory) const;

    /**
     * @brief Returns whether a match is strong enough to skip during scans.
     * @param match Match to inspect.
     * @return True when scan traversal should skip the project root.
     */
    static bool should_skip(const ProtectedProjectMatch& match);

private:
    std::vector<ProtectedProjectRule> owned_rules_;
    const std::vector<ProtectedProjectRule>* rules_{nullptr};
};
