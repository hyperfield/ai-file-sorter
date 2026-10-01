#include "JohnnyDecimalValidator.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "FolderTreeCatalog.hpp"
#include "Utils.hpp"

namespace {

struct AreaId {
    bool looks_like_id{false};
    bool valid{false};
    int start{-1};
    int end{-1};
    int width{0};
    std::string label;
    std::string reason;
};

struct CategoryId {
    bool looks_like_id{false};
    bool valid{false};
    int number{-1};
    int width{0};
    std::string label;
    std::string reason;
};

struct AreaRecord {
    FolderTreeCatalog::Entry entry;
    AreaId id;
};

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::vector<std::string> split_path(std::string_view value) {
    std::vector<std::string> segments;
    std::string current;
    for (char ch : value) {
        if (ch == '/' || ch == '\\') {
            if (!current.empty()) {
                segments.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        segments.push_back(std::move(current));
    }
    return segments;
}

std::string basename(std::string_view relative_path) {
    const auto segments = split_path(relative_path);
    return segments.empty() ? std::string() : segments.back();
}

std::size_t consume_digits(const std::string& value, std::size_t pos) {
    while (pos < value.size() && std::isdigit(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    return pos;
}

std::size_t skip_spaces(const std::string& value, std::size_t pos) {
    while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    return pos;
}

std::optional<int> parse_int(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    int result = 0;
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return std::nullopt;
        }
        result = result * 10 + (ch - '0');
    }
    return result;
}

std::string normalized_label(std::string_view value) {
    return lower_copy(Utils::sanitize_path_label(std::string(value)));
}

std::string range_key(const AreaId& id) {
    return std::to_string(id.start) + "-" + std::to_string(id.end);
}

AreaId parse_area_id(std::string segment) {
    segment = trim_copy(std::move(segment));
    AreaId parsed;
    if (segment.empty()) {
        return parsed;
    }

    const std::size_t first_end = consume_digits(segment, 0);
    if (first_end == 0) {
        return parsed;
    }
    parsed.looks_like_id = true;

    const std::size_t hyphen_pos = skip_spaces(segment, first_end);
    if (hyphen_pos >= segment.size() || segment[hyphen_pos] != '-') {
        parsed.reason = "Expected an area range like 20-29 Work.";
        return parsed;
    }

    const std::size_t second_start = skip_spaces(segment, hyphen_pos + 1);
    const std::size_t second_end = consume_digits(segment, second_start);
    if (second_end == second_start) {
        parsed.reason = "Expected a range end after the hyphen.";
        return parsed;
    }

    const std::size_t label_start = skip_spaces(segment, second_end);
    if (label_start >= segment.size()) {
        parsed.reason = "Expected a label after the area range.";
        return parsed;
    }

    const auto start = parse_int(std::string_view(segment).substr(0, first_end));
    const auto end = parse_int(std::string_view(segment).substr(second_start, second_end - second_start));
    if (!start || !end) {
        parsed.reason = "Expected numeric area range bounds.";
        return parsed;
    }

    parsed.start = *start;
    parsed.end = *end;
    parsed.width = static_cast<int>(std::max(first_end, second_end - second_start));
    parsed.label = trim_copy(segment.substr(label_start));

    if (first_end != second_end - second_start || parsed.width != 2) {
        parsed.reason = "Use two-digit area ranges such as 20-29 Work.";
        return parsed;
    }
    if (parsed.start < 0 || parsed.end > 99 || parsed.start % 10 != 0 || parsed.end != parsed.start + 9) {
        parsed.reason = "Use decade area ranges such as 20-29 Work.";
        return parsed;
    }
    if (parsed.label.empty()) {
        parsed.reason = "Expected a label after the area range.";
        return parsed;
    }

    parsed.valid = true;
    return parsed;
}

CategoryId parse_category_id(std::string segment, int expected_width) {
    segment = trim_copy(std::move(segment));
    CategoryId parsed;
    if (segment.empty()) {
        return parsed;
    }

    const std::size_t first_end = consume_digits(segment, 0);
    if (first_end == 0) {
        return parsed;
    }
    parsed.looks_like_id = true;
    parsed.width = static_cast<int>(first_end);
    const auto number = parse_int(std::string_view(segment).substr(0, first_end));
    if (number) {
        parsed.number = *number;
    }

    bool nested_id_at_category_level = false;
    std::size_t prefix_end = first_end;
    if (prefix_end + 1 < segment.size() &&
        (segment[prefix_end] == '.' || segment[prefix_end] == '_' || segment[prefix_end] == '-') &&
        std::isdigit(static_cast<unsigned char>(segment[prefix_end + 1]))) {
        nested_id_at_category_level = true;
        prefix_end = consume_digits(segment, prefix_end + 1);
    }

    const std::size_t label_start = skip_spaces(segment, prefix_end);
    if (label_start <= prefix_end || label_start >= segment.size()) {
        parsed.reason = "Expected a label after the category number.";
        return parsed;
    }
    parsed.label = trim_copy(segment.substr(label_start));

    if (nested_id_at_category_level) {
        parsed.reason = "Use a two-digit category ID at the area level.";
        return parsed;
    }
    if (parsed.width != expected_width) {
        parsed.reason = "Use a two-digit category ID inside the area range.";
        return parsed;
    }
    if (parsed.label.empty()) {
        parsed.reason = "Expected a label after the category number.";
        return parsed;
    }

    parsed.valid = true;
    return parsed;
}

bool is_direct_child_of(const FolderTreeCatalog::Entry& entry, std::string_view parent_path, int parent_depth) {
    if (entry.depth != parent_depth + 1) {
        return false;
    }
    const std::string prefix = std::string(parent_path) + "/";
    return entry.relative_path.rfind(prefix, 0) == 0;
}

void add_issue(JohnnyDecimalValidator::Report& report, JohnnyDecimalValidator::Severity severity, std::string code,
               std::string title, std::string relative_path, std::string message, std::string suggestion) {
    report.issues.push_back(JohnnyDecimalValidator::Issue{
        severity,
        std::move(code),
        std::move(title),
        std::move(relative_path),
        std::move(message),
        std::move(suggestion),
    });
}

void validate_top_level(const FolderTreeCatalog::Catalog& catalog, JohnnyDecimalValidator::Report& report,
                        std::vector<AreaRecord>& areas) {
    std::map<std::string, std::vector<std::string>> ranges;
    std::map<std::string, std::vector<std::string>> labels;

    for (const auto& entry : catalog.entries()) {
        if (entry.depth != 1) {
            continue;
        }

        const AreaId area = parse_area_id(entry.relative_path);
        if (area.valid) {
            areas.push_back(AreaRecord{entry, area});
            ++report.area_count;
            ranges[range_key(area)].push_back(entry.relative_path);
            labels[normalized_label(area.label)].push_back(entry.relative_path);
            continue;
        }

        if (area.looks_like_id) {
            add_issue(report, JohnnyDecimalValidator::Severity::Error, "area_id_malformed", "Malformed area ID",
                      entry.relative_path,
                      area.reason.empty()
                          ? "This top-level folder looks like a Johnny.Decimal area but its ID is not valid."
                          : area.reason,
                      "Rename it to a two-digit decade range followed by a label, for example 20-29 Work.");
            const CategoryId root_category = parse_category_id(entry.relative_path, 2);
            if (root_category.looks_like_id) {
                add_issue(report, JohnnyDecimalValidator::Severity::Warning, "category_without_area",
                          "Category-like folder at archive root", entry.relative_path,
                          "This folder looks like a Johnny.Decimal category, but categories should live inside an "
                          "area range.",
                          "Move it under a matching area folder such as 10-19 Admin.");
            }
            continue;
        }

        const CategoryId root_category = parse_category_id(entry.relative_path, 2);
        if (root_category.looks_like_id) {
            add_issue(
                report, JohnnyDecimalValidator::Severity::Warning, "category_without_area",
                "Category-like folder at archive root", entry.relative_path,
                "This folder looks like a Johnny.Decimal category, but categories should live inside an area range.",
                "Move it under a matching area folder such as 10-19 Admin.");
        }
    }

    for (const auto& [range, paths] : ranges) {
        if (paths.size() < 2) {
            continue;
        }
        add_issue(report, JohnnyDecimalValidator::Severity::Error, "duplicate_area_range", "Duplicate area range",
                  paths.front(), "The area range " + range + " is used by more than one top-level folder.",
                  "Keep one folder for this range and move or rename the duplicate area.");
    }

    for (const auto& [label, paths] : labels) {
        if (label.empty() || paths.size() < 2) {
            continue;
        }
        add_issue(report, JohnnyDecimalValidator::Severity::Warning, "duplicate_area_label", "Duplicate area label",
                  paths.front(), "More than one area uses the same human label.",
                  "Keep labels distinct so routing can choose the intended area.");
    }
}

void validate_area_children(const FolderTreeCatalog::Catalog& catalog, const AreaRecord& area,
                            JohnnyDecimalValidator::Report& report) {
    int direct_child_count = 0;
    int valid_category_count = 0;
    std::map<int, std::vector<std::string>> category_numbers;
    std::map<std::string, std::vector<std::string>> category_labels;

    for (const auto& entry : catalog.entries()) {
        if (!is_direct_child_of(entry, area.entry.relative_path, area.entry.depth)) {
            continue;
        }
        ++direct_child_count;
        const CategoryId category = parse_category_id(basename(entry.relative_path), area.id.width);
        if (!category.looks_like_id) {
            add_issue(report, JohnnyDecimalValidator::Severity::Warning, "missing_category_id", "Missing category ID",
                      entry.relative_path,
                      "This direct child of an area does not start with a Johnny.Decimal category number.",
                      "Rename it to the next free number inside " + area.entry.relative_path + ", for example " +
                          std::to_string(area.id.start + 1) + " Label.");
            continue;
        }

        category_numbers[category.number].push_back(entry.relative_path);
        if (!category.label.empty()) {
            category_labels[normalized_label(category.label)].push_back(entry.relative_path);
        }

        if (!category.valid) {
            add_issue(report, JohnnyDecimalValidator::Severity::Error, "category_id_malformed", "Malformed category ID",
                      entry.relative_path,
                      category.reason.empty()
                          ? "This folder looks like a Johnny.Decimal category but its ID is not valid."
                          : category.reason,
                      "Use a two-digit category number followed by a label, for example 21 Projects.");
        }

        if (category.number < area.id.start || category.number > area.id.end) {
            add_issue(report, JohnnyDecimalValidator::Severity::Error, "category_outside_area_range",
                      "Category outside area range", entry.relative_path,
                      "This category number does not belong inside the parent area range.",
                      "Move it under the matching area or rename it to a free number inside " +
                          area.entry.relative_path + ".");
            continue;
        }

        if (category.valid) {
            ++valid_category_count;
            ++report.category_count;
        }
    }

    for (const auto& [number, paths] : category_numbers) {
        if (paths.size() < 2) {
            continue;
        }
        add_issue(report, JohnnyDecimalValidator::Severity::Error, "duplicate_category_number",
                  "Duplicate category number", paths.front(),
                  "The category number " + std::to_string(number) + " is used more than once inside this area.",
                  "Keep one folder for the number and rename the duplicate to a free number in the same range.");
    }

    for (const auto& [label, paths] : category_labels) {
        if (label.empty() || paths.size() < 2) {
            continue;
        }
        add_issue(report, JohnnyDecimalValidator::Severity::Warning, "duplicate_category_label",
                  "Duplicate category label", paths.front(),
                  "More than one sibling category uses the same human label.",
                  "Keep category labels distinct so routing can choose the intended destination.");
    }

    if (direct_child_count == 0 || valid_category_count == 0) {
        add_issue(report, JohnnyDecimalValidator::Severity::Warning, "area_missing_categories",
                  "Area has no category folders", area.entry.relative_path,
                  "This area does not contain any recognizable direct category folders.",
                  "Add category folders with two-digit numbers inside the area range.");
    }
}

void validate_loose_root_folders(const FolderTreeCatalog::Catalog& catalog, const std::vector<AreaRecord>& areas,
                                 JohnnyDecimalValidator::Report& report) {
    if (areas.empty()) {
        add_issue(
            report, JohnnyDecimalValidator::Severity::Error, "missing_area_structure", "No Johnny.Decimal areas found",
            {}, "The selected folder does not contain top-level area ranges.",
            "Create area folders such as 00-09 System, 10-19 Admin, and 20-29 Work before relying on J.D routing.");
        return;
    }

    for (const auto& entry : catalog.entries()) {
        if (entry.depth != 1) {
            continue;
        }
        const AreaId area = parse_area_id(entry.relative_path);
        if (area.valid || area.looks_like_id) {
            continue;
        }
        const CategoryId root_category = parse_category_id(entry.relative_path, 2);
        if (root_category.looks_like_id) {
            continue;
        }
        add_issue(
            report, JohnnyDecimalValidator::Severity::Warning, "folder_outside_area_structure",
            "Folder outside area structure", entry.relative_path,
            "This top-level folder is outside the Johnny.Decimal area structure.",
            "Move it into a numbered category or rename it as an area range if it should be part of the archive.");
    }
}

std::size_t severity_count(const JohnnyDecimalValidator::Report& report, JohnnyDecimalValidator::Severity severity) {
    return static_cast<std::size_t>(std::count_if(report.issues.begin(), report.issues.end(),
                                                  [&](const auto& issue) { return issue.severity == severity; }));
}

std::string path_to_text(const std::filesystem::path& path) {
    return Utils::path_to_utf8(path);
}

}  // namespace

namespace JohnnyDecimalValidator {

bool Report::has_errors() const {
    return std::any_of(issues.begin(), issues.end(),
                       [](const Issue& issue) { return issue.severity == Severity::Error; });
}

Report validate_archive(const std::filesystem::path& root, int max_depth, std::size_t max_entries) {
    Report report;
    report.root = root;

    if (root.empty()) {
        add_issue(report, Severity::Error, "missing_root", "No archive selected", {},
                  "Choose a folder before running the Johnny.Decimal validation report.",
                  "Select the root folder of the archive.");
        return report;
    }
    if (!root.is_absolute()) {
        add_issue(report, Severity::Error, "relative_root", "Archive path is not absolute", {},
                  "The validation report needs an absolute folder path.", "Choose the archive root through Browse.");
        return report;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec) || ec) {
        add_issue(report, Severity::Error, "root_not_found", "Archive folder not found", {},
                  "The selected path is not an existing folder.", "Choose an existing Johnny.Decimal archive root.");
        return report;
    }

    report.scanned = true;
    const auto catalog = FolderTreeCatalog::Catalog::scan(root, max_depth, max_entries);
    if (catalog.entries().size() >= max_entries) {
        add_issue(report, Severity::Warning, "scan_limit_reached", "Scan limit reached", {},
                  "The validation report scanned the maximum configured number of folders.",
                  "Run the report on a smaller subtree if you need to inspect deeper folders.");
    }

    std::vector<AreaRecord> areas;
    validate_top_level(catalog, report, areas);
    validate_loose_root_folders(catalog, areas, report);
    for (const AreaRecord& area : areas) {
        validate_area_children(catalog, area, report);
    }
    report.looks_johnny_decimal_like = report.area_count > 0 && report.category_count > 0;
    return report;
}

std::string severity_label(Severity severity) {
    switch (severity) {
        case Severity::Error:
            return "ERROR";
        case Severity::Warning:
            return "WARNING";
        case Severity::Info:
            return "INFO";
    }
    return "INFO";
}

std::string format_report(const Report& report) {
    std::ostringstream out;
    out << "Johnny.Decimal validation report\n";
    if (!report.root.empty()) {
        out << "Root: " << path_to_text(report.root) << "\n";
    }
    out << "Areas: " << report.area_count << " | Categories: " << report.category_count << "\n";
    out << "Errors: " << severity_count(report, Severity::Error)
        << " | Warnings: " << severity_count(report, Severity::Warning)
        << " | Info: " << severity_count(report, Severity::Info) << "\n\n";

    if (report.issues.empty()) {
        out << "No issues found. The scanned tree has a recognizable Johnny.Decimal area/category structure.\n";
        return out.str();
    }

    for (const Issue& issue : report.issues) {
        out << severity_label(issue.severity) << " - " << issue.title << " [" << issue.code << "]\n";
        if (!issue.relative_path.empty()) {
            out << "Path: " << issue.relative_path << "\n";
        }
        out << "Details: " << issue.message << "\n";
        if (!issue.suggestion.empty()) {
            out << "Suggested fix: " << issue.suggestion << "\n";
        }
        out << "\n";
    }
    return out.str();
}

}  // namespace JohnnyDecimalValidator
