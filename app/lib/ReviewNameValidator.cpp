#include "ReviewNameValidator.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

namespace {

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool contains_only_allowed_chars(const std::string& value)
{
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        static const std::string forbidden = R"(<>:"/\|?*)";
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool has_leading_or_trailing_space_or_dot(const std::string& value)
{
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    const unsigned char last = static_cast<unsigned char>(value.back());
    return std::isspace(first) || std::isspace(last) || value.front() == '.' ||
           value.back() == '.';
}

bool is_reserved_windows_name(const std::string& value)
{
    static const std::vector<std::string> reserved = {
        "con",  "prn",  "aux",  "nul",  "com1", "com2", "com3",
        "com4", "com5", "com6", "com7", "com8", "com9", "lpt1",
        "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8",
        "lpt9",
    };
    const std::string lower = to_lower_copy(value);
    return std::find(reserved.begin(), reserved.end(), lower) != reserved.end();
}

bool looks_like_extension_label(const std::string& value)
{
    const auto dot_pos = value.rfind('.');
    if (dot_pos == std::string::npos || dot_pos == value.size() - 1) {
        return false;
    }
    const std::string ext = value.substr(dot_pos + 1);
    if (ext.empty() || ext.size() > 5) {
        return false;
    }
    return std::all_of(ext.begin(), ext.end(), [](unsigned char ch) {
        return std::isalpha(ch);
    });
}

} // namespace

std::string ReviewNameValidator::trim_copy(const std::string& value)
{
    std::string result = value;
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), not_space));
    result.erase(std::find_if(result.rbegin(), result.rend(), not_space).base(), result.end());
    return result;
}

bool ReviewNameValidator::is_missing_category_label(const std::string& value)
{
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return true;
    }
    return to_lower_copy(trimmed) == "uncategorized";
}

std::string ReviewNameValidator::strip_history_description_label(std::string value)
{
    value = trim_copy(value);
    constexpr std::string_view image_prefix = "Image description: ";
    constexpr std::string_view document_prefix = "Document summary: ";
    if (value.starts_with(image_prefix)) {
        return trim_copy(value.substr(image_prefix.size()));
    }
    if (value.starts_with(document_prefix)) {
        return trim_copy(value.substr(document_prefix.size()));
    }
    return value;
}

bool ReviewNameValidator::validate_labels(const std::string& category,
                                          const std::string& subcategory,
                                          std::string& error,
                                          bool allow_identical)
{
    constexpr size_t kMaxLabelLength = 80;
    if (category.empty() || subcategory.empty()) {
        error = "Category or subcategory is empty";
        return false;
    }
    if (category.size() > kMaxLabelLength || subcategory.size() > kMaxLabelLength) {
        error = "Category or subcategory exceeds max length";
        return false;
    }
    if (!contains_only_allowed_chars(category) || !contains_only_allowed_chars(subcategory)) {
        error = "Category or subcategory contains disallowed characters";
        return false;
    }
    if (looks_like_extension_label(category) || looks_like_extension_label(subcategory)) {
        error = "Category or subcategory looks like a file extension";
        return false;
    }
    if (is_reserved_windows_name(category) || is_reserved_windows_name(subcategory)) {
        error = "Category or subcategory is a reserved name";
        return false;
    }
    if (!allow_identical && to_lower_copy(category) == to_lower_copy(subcategory)) {
        error = "Category and subcategory are identical";
        return false;
    }
    return true;
}

bool ReviewNameValidator::validate_filename(const std::string& name, std::string& error)
{
    if (name.empty()) {
        error = "Filename is empty";
        return false;
    }
    if (name == "." || name == "..") {
        error = "Filename is invalid";
        return false;
    }
    if (!contains_only_allowed_chars(name)) {
        error = "Filename contains disallowed characters";
        return false;
    }
    if (has_leading_or_trailing_space_or_dot(name)) {
        error = "Filename has leading/trailing space or dot";
        return false;
    }

    const auto path = Utils::utf8_to_path(name);
    const std::string stem = Utils::path_to_utf8(path.stem());
    if (stem.empty()) {
        error = "Filename is missing a base name";
        return false;
    }
    if (is_reserved_windows_name(stem)) {
        error = "Filename is a reserved name";
        return false;
    }
    return true;
}
