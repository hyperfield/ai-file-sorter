#include "JohnnyDecimalFolderSuggester.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "FolderStructurePattern.hpp"
#include "FolderTreeCatalog.hpp"
#include "Utils.hpp"

namespace {

void assign_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
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

std::string join_path(std::span<const std::string> segments) {
    std::ostringstream out;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            out << '/';
        }
        out << segments[i];
    }
    return out.str();
}

std::string basename_label(std::string_view relative_path) {
    const auto segments = split_path(relative_path);
    if (segments.empty()) {
        return {};
    }
    return FolderStructurePattern::human_label_path(segments.back());
}

std::string normalized_label(std::string_view value) {
    return lower_copy(Utils::sanitize_path_label(std::string(value)));
}

std::string parent_path_of(std::span<const std::string> segments) {
    if (segments.size() < 2) {
        return {};
    }
    return join_path(segments.first(segments.size() - 1));
}

bool is_direct_child_of(const FolderTreeCatalog::Entry& entry, std::string_view parent_path, std::size_t child_depth) {
    if (static_cast<std::size_t>(entry.depth) != child_depth) {
        return false;
    }
    if (parent_path.empty()) {
        return entry.relative_path.find('/') == std::string::npos;
    }
    const std::string parent_prefix = std::string(parent_path) + "/";
    return entry.relative_path.rfind(parent_prefix, 0) == 0;
}

std::optional<std::string> find_duplicate_sibling_label(const FolderTreeCatalog::Catalog& catalog,
                                                        std::string_view suggested_relative_path,
                                                        std::string_view folder_label) {
    const auto segments = split_path(suggested_relative_path);
    if (segments.size() < 2) {
        return std::nullopt;
    }

    const std::string parent_path = parent_path_of(segments);
    const std::string requested_label = normalized_label(folder_label);
    if (requested_label.empty()) {
        return std::nullopt;
    }

    for (const auto& entry : catalog.entries()) {
        if (!is_direct_child_of(entry, parent_path, segments.size())) {
            continue;
        }
        if (normalized_label(basename_label(entry.relative_path)) == requested_label) {
            return entry.relative_path;
        }
    }
    return std::nullopt;
}

std::optional<JohnnyDecimalFolderSuggester::Suggestion> build_suggestion(
    const std::filesystem::path& root, std::string_view area_label, std::string_view folder_label,
    std::span<const FolderStructurePluginProfile> plugin_profiles, std::string* error) {
    if (root.empty()) {
        assign_error(error, "Choose a destination folder.");
        return std::nullopt;
    }
    if (!root.is_absolute()) {
        assign_error(error, "Choose an absolute destination folder.");
        return std::nullopt;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec) || ec) {
        assign_error(error, "Choose an existing Johnny.Decimal archive folder.");
        return std::nullopt;
    }

    const std::string sanitized_area = Utils::sanitize_path_label(std::string(area_label));
    const std::string sanitized_folder = Utils::sanitize_path_label(std::string(folder_label));
    if (sanitized_area.empty()) {
        assign_error(error, "Enter an area name.");
        return std::nullopt;
    }
    if (sanitized_folder.empty()) {
        assign_error(error, "Enter a folder name.");
        return std::nullopt;
    }

    const auto catalog = FolderTreeCatalog::Catalog::scan(root);
    const auto profile = FolderStructurePattern::infer_profile(catalog, plugin_profiles);
    if (!profile.has_johnny_decimal_like_ranges) {
        assign_error(error, "The destination does not contain a recognizable Johnny.Decimal-like folder structure.");
        return std::nullopt;
    }

    const auto suggested =
        FolderStructurePattern::suggest_new_folder(catalog, sanitized_area, sanitized_folder, plugin_profiles);
    if (!suggested) {
        assign_error(error, "Could not find an unused Johnny.Decimal number for this folder.");
        return std::nullopt;
    }

    const auto validation = FolderTreeCatalog::validate_relative_folder_path(suggested->relative_path);
    if (!validation.valid) {
        assign_error(error, validation.error.empty() ? "The suggested folder path is not valid." : validation.error);
        return std::nullopt;
    }

    if (const auto duplicate = find_duplicate_sibling_label(catalog, validation.normalized_path, sanitized_folder)) {
        assign_error(error, "A folder with that label already exists: " + *duplicate);
        return std::nullopt;
    }

    const std::filesystem::path absolute_path = root / Utils::utf8_to_path(validation.normalized_path);
    ec.clear();
    if (std::filesystem::exists(absolute_path, ec) || ec) {
        assign_error(error, ec ? "Could not inspect the suggested folder path."
                               : "A file or folder already exists at the suggested path.");
        return std::nullopt;
    }

    return JohnnyDecimalFolderSuggester::Suggestion{validation.normalized_path, absolute_path, suggested->reason};
}

}  // namespace

namespace JohnnyDecimalFolderSuggester {

std::optional<Suggestion> preview_next_folder(const std::filesystem::path& root, std::string_view area_label,
                                              std::string_view folder_label,
                                              std::span<const FolderStructurePluginProfile> plugin_profiles,
                                              std::string* error) {
    return build_suggestion(root, area_label, folder_label, plugin_profiles, error);
}

CreationResult create_next_folder(const std::filesystem::path& root, std::string_view area_label,
                                  std::string_view folder_label,
                                  std::span<const FolderStructurePluginProfile> plugin_profiles) {
    CreationResult result;
    std::string error;
    auto suggestion = build_suggestion(root, area_label, folder_label, plugin_profiles, &error);
    if (!suggestion) {
        result.error = std::move(error);
        return result;
    }

    const auto segments = split_path(suggestion->relative_path);
    std::filesystem::path current = root;
    for (const auto& segment : segments) {
        current /= Utils::utf8_to_path(segment);
        std::error_code ec;
        if (std::filesystem::is_directory(current, ec) && !ec) {
            continue;
        }
        if (std::filesystem::exists(current, ec) || ec) {
            result.error = ec ? "Could not inspect a folder on the suggested path."
                              : "A file already exists where a Johnny.Decimal folder should be created.";
            return result;
        }
        ec.clear();
        if (!std::filesystem::create_directory(current, ec) || ec) {
            result.error = "Could not create the suggested Johnny.Decimal folder.";
            return result;
        }
        result.created_directories.push_back(current);
    }

    result.success = true;
    result.suggestion = std::move(*suggestion);
    return result;
}

}  // namespace JohnnyDecimalFolderSuggester
