#include "TextEncoding.hpp"
#include "ArtifactCategoryPolicy.hpp"

#include "FileCategoryPolicy.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace {
constexpr char kSoftwareCategory[] = "Software";
constexpr char kInstallersCategory[] = "Installers";
constexpr char kDriversCategory[] = "Drivers";
constexpr char kOperatingSystemsCategory[] = "Operating Systems";
constexpr char kArchivesCategory[] = "Archives";
constexpr char kDataExportsCategory[] = "Data Exports";
constexpr char kOtherCategory[] = "Other";
constexpr char kGeneralSubcategory[] = "General";

enum class ArtifactFamily {
    None,
    Software,
    Archive
};

std::string trim_copy(std::string value)
{
    auto not_space = [](unsigned char ch) { return !TextEncoding::ascii_space(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string collapse_spaces_copy(std::string value)
{
    std::string collapsed;
    collapsed.reserve(value.size());
    bool previous_space = false;
    for (unsigned char ch : value) {
        if (TextEncoding::ascii_alnum(ch)) {
            collapsed.push_back(static_cast<char>(TextEncoding::ascii_lower(ch)));
            previous_space = false;
            continue;
        }
        if (!previous_space && !collapsed.empty()) {
            collapsed.push_back(' ');
        }
        previous_space = true;
    }
    return trim_copy(std::move(collapsed));
}

std::string normalize_match_text(const std::string& value)
{
    return collapse_spaces_copy(value);
}

bool matches_exact_alias(const std::string& normalized_text,
                         std::initializer_list<std::string_view> aliases)
{
    return std::any_of(aliases.begin(), aliases.end(), [&](std::string_view alias) {
        return normalized_text == alias;
    });
}

bool is_other_label(const std::string& normalized_text)
{
    return matches_exact_alias(normalized_text, {"other", "others", "misc", "miscellaneous", "uncategorized"});
}

bool is_low_information_artifact_label(const std::string& normalized_text)
{
    return normalized_text.empty() ||
           is_other_label(normalized_text) ||
           matches_exact_alias(
               normalized_text,
               {"general", "file", "files", "software", "application", "applications", "app", "apps",
                "program", "programs", "tool", "tools", "utility", "utilities", "installers", "installer",
                "drivers", "driver", "operating system", "operating systems", "archives", "archive",
                "data export", "data exports"});
}

ArtifactFamily artifact_family_for_file_name(const std::string& file_name)
{
    const auto selection = FileCategoryPolicy::determine_main_category_selection(file_name, FileType::File);
    if (selection.family_name == "software") {
        return ArtifactFamily::Software;
    }
    if (selection.family_name == "archive") {
        return ArtifactFamily::Archive;
    }
    return ArtifactFamily::None;
}

std::optional<std::string> canonicalize_artifact_main_category(const std::string& category)
{
    const std::string normalized_category = normalize_match_text(category);
    if (normalized_category.empty()) {
        return std::nullopt;
    }

    if (matches_exact_alias(normalized_category,
                            {"software", "application", "applications", "app", "apps",
                             "program", "programs"})) {
        return std::string(kSoftwareCategory);
    }
    if (matches_exact_alias(normalized_category,
                            {"installer", "installers", "installation", "installations",
                             "setup", "setups", "setup file", "setup files"})) {
        return std::string(kInstallersCategory);
    }
    if (matches_exact_alias(normalized_category, {"driver", "drivers"})) {
        return std::string(kDriversCategory);
    }
    if (matches_exact_alias(normalized_category, {"operating system", "operating systems"})) {
        return std::string(kOperatingSystemsCategory);
    }
    if (matches_exact_alias(normalized_category, {"archive", "archives"})) {
        return std::string(kArchivesCategory);
    }
    if (matches_exact_alias(normalized_category, {"data export", "data exports"})) {
        return std::string(kDataExportsCategory);
    }
    if (is_other_label(normalized_category)) {
        return std::string(kOtherCategory);
    }
    return std::nullopt;
}

std::string choose_artifact_subcategory(const std::string& stable_main_category,
                                        const std::string& subcategory)
{
    const std::string sanitized_subcategory = Utils::sanitize_path_label(trim_copy(subcategory));
    if (sanitized_subcategory.empty()) {
        return kGeneralSubcategory;
    }

    const std::string normalized_main = normalize_match_text(stable_main_category);
    const std::string normalized_subcategory = normalize_match_text(sanitized_subcategory);
    if (normalized_subcategory.empty() || is_low_information_artifact_label(normalized_subcategory)) {
        return kGeneralSubcategory;
    }

    const auto canonical_subcategory = canonicalize_artifact_main_category(sanitized_subcategory);
    if (normalized_subcategory == normalized_main ||
        (canonical_subcategory.has_value() &&
         normalize_match_text(*canonical_subcategory) == normalized_main)) {
        return kGeneralSubcategory;
    }

    return sanitized_subcategory;
}

} // namespace

namespace ArtifactCategoryPolicy {

bool is_supported_artifact_file_name(const std::string& file_name)
{
    return artifact_family_for_file_name(file_name) != ArtifactFamily::None;
}

std::optional<NormalizedCategoryLabels> normalize_category_labels(const std::string& file_name,
                                                                  const std::string& category,
                                                                  const std::string& subcategory)
{
    if (artifact_family_for_file_name(file_name) == ArtifactFamily::None) {
        return std::nullopt;
    }

    const auto stable_main_category = canonicalize_artifact_main_category(category);
    if (!stable_main_category) {
        return std::nullopt;
    }

    return NormalizedCategoryLabels{
        *stable_main_category,
        choose_artifact_subcategory(*stable_main_category, subcategory)
    };
}

} // namespace ArtifactCategoryPolicy
