#include "ReviewFileNaming.hpp"

#include "DocumentTextAnalyzer.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace ReviewFileNaming {
namespace {

struct NumericSuffix {
    std::string base;
    int value{0};
    bool has_suffix{false};
};

NumericSuffix split_numeric_suffix(const std::string& stem)
{
    NumericSuffix result{stem, 0, false};
    const auto pos = stem.rfind('_');
    if (pos == std::string::npos || pos + 1 >= stem.size()) {
        return result;
    }
    const std::string suffix = stem.substr(pos + 1);
    if (suffix.empty()) {
        return result;
    }
    for (unsigned char ch : suffix) {
        if (!std::isdigit(ch)) {
            return result;
        }
    }
    int value = 0;
    try {
        value = std::stoi(suffix);
    } catch (...) {
        return result;
    }
    if (value <= 0) {
        return result;
    }
    const std::string base = stem.substr(0, pos);
    if (base.empty()) {
        return result;
    }
    result.base = base;
    result.value = value;
    result.has_suffix = true;
    return result;
}

struct ParentheticalSuffix {
    std::string base;
    int value{0};
    bool has_suffix{false};
};

ParentheticalSuffix split_parenthetical_suffix(const std::string& stem)
{
    ParentheticalSuffix result{stem, 0, false};
    if (stem.size() < 4) {
        return result;
    }
    if (stem.back() != ')') {
        return result;
    }
    const auto open_pos = stem.rfind('(');
    if (open_pos == std::string::npos || open_pos == 0) {
        return result;
    }
    if (open_pos == 0 || stem[open_pos - 1] != ' ') {
        return result;
    }
    const std::string number = stem.substr(open_pos + 1, stem.size() - open_pos - 2);
    if (number.empty()) {
        return result;
    }
    for (unsigned char ch : number) {
        if (!std::isdigit(ch)) {
            return result;
        }
    }
    int value = 0;
    try {
        value = std::stoi(number);
    } catch (...) {
        return result;
    }
    if (value <= 0) {
        return result;
    }
    const std::string base = stem.substr(0, open_pos - 1);
    if (base.empty()) {
        return result;
    }
    result.base = base;
    result.value = value;
    result.has_suffix = true;
    return result;
}

bool file_exists_in_target_dir(const std::filesystem::path& target_dir,
                               const std::string& candidate)
{
    if (target_dir.empty()) {
        return false;
    }

    std::error_code ec;
    const auto candidate_path = target_dir / Utils::utf8_to_path(candidate);
    return std::filesystem::exists(candidate_path, ec);
}

bool should_deduplicate_suggested_name(const CategorizedFile& file)
{
    if (file.type != FileType::File) {
        return false;
    }
    if (file.suggested_name.empty()) {
        return false;
    }
    if (file.rename_applied) {
        return false;
    }
    return to_lower_copy_str(file.suggested_name) != to_lower_copy_str(file.file_name);
}

} // namespace

std::string to_lower_copy_str(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool is_supported_image_entry(const std::string& file_path,
                              const std::string& file_name,
                              FileType file_type)
{
    if (file_type != FileType::File) {
        return false;
    }
    const auto full_path = Utils::utf8_to_path(file_path) / Utils::utf8_to_path(file_name);
    return LlavaImageAnalyzer::is_supported_image(full_path);
}

bool is_supported_document_entry(const std::string& file_path,
                                 const std::string& file_name,
                                 FileType file_type)
{
    if (file_type != FileType::File) {
        return false;
    }
    const auto full_path = Utils::utf8_to_path(file_path) / Utils::utf8_to_path(file_name);
    return DocumentTextAnalyzer::is_supported_document(full_path);
}

std::filesystem::path build_suggested_target_dir(const CategorizedFile& file,
                                                 const std::string& base_dir_override,
                                                 bool use_subcategory,
                                                 bool move_categorized_entries)
{
    const auto source_dir = Utils::utf8_to_path(file.file_path);
    const auto base_dir = base_dir_override.empty()
        ? source_dir
        : Utils::utf8_to_path(base_dir_override);
    const std::string category = file.category.empty() ? file.canonical_category : file.category;
    if (file.rename_only || category.empty() || !move_categorized_entries) {
        return source_dir;
    }

    const auto category_path = Utils::utf8_to_path(category);
    const std::string subcategory = file.subcategory.empty()
        ? file.canonical_subcategory
        : file.subcategory;
    if (use_subcategory && !subcategory.empty()) {
        const auto subcategory_path = Utils::utf8_to_path(subcategory);
        return base_dir / category_path / subcategory_path;
    }
    return base_dir / category_path;
}

std::string build_unique_suggested_name(const std::string& desired_name,
                                        const std::filesystem::path& target_dir,
                                        std::unordered_set<std::string>& used_names,
                                        std::unordered_map<std::string, int>& next_index,
                                        bool force_numbering)
{
    auto conflicts = [&](const std::string& candidate) -> bool {
        const std::string candidate_lower = to_lower_copy_str(candidate);
        return used_names.count(candidate_lower) > 0 ||
               file_exists_in_target_dir(target_dir, candidate);
    };

    const auto desired_path = Utils::utf8_to_path(desired_name);
    std::string stem = Utils::path_to_utf8(desired_path.stem());
    std::string ext = Utils::path_to_utf8(desired_path.extension());
    if (stem.empty()) {
        stem = desired_name;
        ext.clear();
    }

    const NumericSuffix suffix = split_numeric_suffix(stem);
    std::string base_stem = stem;
    int index = 1;
    bool has_suffix = false;
    if (suffix.has_suffix) {
        base_stem = suffix.base;
        index = suffix.value;
        has_suffix = true;
    }

    if (!force_numbering && !conflicts(desired_name)) {
        return desired_name;
    }
    if (has_suffix && !conflicts(desired_name)) {
        return desired_name;
    }

    const std::string key = has_suffix
                                ? to_lower_copy_str(base_stem + ext)
                                : to_lower_copy_str(desired_name);
    auto it = next_index.find(key);
    if (it != next_index.end()) {
        index = it->second;
    }

    while (true) {
        std::string candidate = base_stem + "_" + std::to_string(index) + ext;
        if (!conflicts(candidate)) {
            next_index[key] = index + 1;
            return candidate;
        }
        ++index;
    }
}

std::string build_unique_move_name(const std::string& desired_name,
                                   const std::filesystem::path& target_dir,
                                   std::unordered_set<std::string>& used_names,
                                   std::unordered_map<std::string, int>& next_index)
{
    auto conflicts = [&](const std::string& candidate) -> bool {
        const std::string candidate_lower = to_lower_copy_str(candidate);
        return used_names.count(candidate_lower) > 0 ||
               file_exists_in_target_dir(target_dir, candidate);
    };

    if (!conflicts(desired_name)) {
        return desired_name;
    }

    const auto desired_path = Utils::utf8_to_path(desired_name);
    std::string stem = Utils::path_to_utf8(desired_path.stem());
    std::string ext = Utils::path_to_utf8(desired_path.extension());
    if (stem.empty()) {
        stem = desired_name;
        ext.clear();
    }

    const ParentheticalSuffix suffix = split_parenthetical_suffix(stem);
    std::string base_stem = suffix.has_suffix ? suffix.base : stem;
    int index = suffix.has_suffix ? suffix.value : 1;

    const std::string key = to_lower_copy_str(base_stem + ext);
    auto it = next_index.find(key);
    if (it != next_index.end()) {
        index = std::max(index, it->second);
    }

    while (true) {
        std::string candidate = base_stem + " (" + std::to_string(index) + ")" + ext;
        if (!conflicts(candidate)) {
            next_index[key] = index + 1;
            return candidate;
        }
        ++index;
    }
}

void ensure_unique_suggested_names(std::vector<CategorizedFile>& files,
                                   const std::string& base_dir,
                                   bool use_subcategory,
                                   bool move_categorized_entries)
{
    std::unordered_map<std::string, std::unordered_map<std::string, int>> counts;
    counts.reserve(files.size());

    for (const auto& file : files) {
        if (!should_deduplicate_suggested_name(file)) {
            continue;
        }
        const auto target_dir = build_suggested_target_dir(file,
                                                           base_dir,
                                                           use_subcategory,
                                                           move_categorized_entries);
        const std::string dir_key = Utils::path_to_utf8(target_dir);
        const std::string name_key = to_lower_copy_str(file.suggested_name);
        counts[dir_key][name_key] += 1;
    }

    std::unordered_map<std::string, std::unordered_set<std::string>> used_names;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> next_index;

    for (auto& file : files) {
        if (!should_deduplicate_suggested_name(file)) {
            continue;
        }
        const auto target_dir = build_suggested_target_dir(file,
                                                           base_dir,
                                                           use_subcategory,
                                                           move_categorized_entries);
        const std::string dir_key = Utils::path_to_utf8(target_dir);
        const std::string name_key = to_lower_copy_str(file.suggested_name);
        const bool force_numbering = counts[dir_key][name_key] > 1;
        auto& dir_used = used_names[dir_key];
        auto& dir_next = next_index[dir_key];

        const std::string unique_name = build_unique_suggested_name(file.suggested_name,
                                                                    target_dir,
                                                                    dir_used,
                                                                    dir_next,
                                                                    force_numbering);
        file.suggested_name = unique_name;
        dir_used.insert(to_lower_copy_str(unique_name));
    }
}

} // namespace ReviewFileNaming
