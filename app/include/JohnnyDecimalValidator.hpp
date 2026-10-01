#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace JohnnyDecimalValidator {

/**
 * @brief Severity for a Johnny.Decimal validation finding.
 */
enum class Severity { Info, Warning, Error };

/**
 * @brief One read-only Johnny.Decimal archive validation finding.
 */
struct Issue {
    /** @brief Finding severity. */
    Severity severity{Severity::Info};
    /** @brief Stable machine-readable finding code. */
    std::string code;
    /** @brief Short user-facing title. */
    std::string title;
    /** @brief Relative folder path where the issue was found, using forward slashes. */
    std::string relative_path;
    /** @brief Explanation of the issue and why it matters. */
    std::string message;
    /** @brief Suggested manual fix. */
    std::string suggestion;
};

/**
 * @brief Read-only report for a scanned Johnny.Decimal archive root.
 */
struct Report {
    /** @brief Root folder that was requested for validation. */
    std::filesystem::path root;
    /** @brief True when the root was an existing directory and could be scanned. */
    bool scanned{false};
    /** @brief True when the scanned tree has at least one area and one category. */
    bool looks_johnny_decimal_like{false};
    /** @brief Count of valid top-level area range folders. */
    int area_count{0};
    /** @brief Count of valid direct category folders inside valid areas. */
    int category_count{0};
    /** @brief Validation findings. */
    std::vector<Issue> issues;

    /**
     * @brief Return whether any issue has error severity.
     * @return True when the report contains at least one error.
     */
    bool has_errors() const;
};

/**
 * @brief Validate a folder tree against the Johnny.Decimal area/category convention.
 * @param root Archive root to scan.
 * @param max_depth Maximum folder depth to scan below the root.
 * @param max_entries Maximum folder entries retained from the scan.
 * @return Read-only validation report.
 */
Report validate_archive(const std::filesystem::path& root, int max_depth = 8, std::size_t max_entries = 500);

/**
 * @brief Convert a severity to a stable English label.
 * @param severity Severity value.
 * @return Label suitable for logs and plain-text reports.
 */
std::string severity_label(Severity severity);

/**
 * @brief Format a validation report as plain text for a read-only UI panel.
 * @param report Report to format.
 * @return Plain text report.
 */
std::string format_report(const Report& report);

}  // namespace JohnnyDecimalValidator
