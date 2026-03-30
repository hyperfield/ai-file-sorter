#include "CloudPathSupport.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace {

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string normalize_path_for_detection(const std::string& root_path)
{
    if (root_path.empty()) {
        return {};
    }

    try {
        auto normalized = Utils::path_to_utf8(Utils::utf8_to_path(root_path).lexically_normal());
        while (normalized.size() > 1 &&
               (normalized.back() == '/' || normalized.back() == '\\')) {
            normalized.pop_back();
        }
        return to_lower_copy(normalized);
    } catch (...) {
        return to_lower_copy(root_path);
    }
}

bool is_boundary_char(char ch)
{
    return ch == '/' || ch == '\\' || ch == ' ' || ch == '_' || ch == '-' ||
           ch == '(' || ch == ')' || ch == '[' || ch == ']';
}

bool contains_marker_segment(const std::string& normalized_path, const std::string& marker)
{
    if (normalized_path.empty() || marker.empty()) {
        return false;
    }

    const std::string normalized_marker = to_lower_copy(marker);
    std::size_t pos = normalized_path.find(normalized_marker);
    while (pos != std::string::npos) {
        const bool left_ok = pos == 0 || is_boundary_char(normalized_path[pos - 1]);
        const std::size_t right_pos = pos + normalized_marker.size();
        const bool right_ok =
            right_pos >= normalized_path.size() || is_boundary_char(normalized_path[right_pos]);
        if (left_ok && right_ok) {
            return true;
        }
        pos = normalized_path.find(normalized_marker, pos + 1);
    }

    return false;
}

bool is_same_or_child_path(const std::string& normalized_path, const std::string& normalized_root)
{
    if (normalized_path.empty() || normalized_root.empty()) {
        return false;
    }
    if (normalized_path == normalized_root) {
        return true;
    }
    if (!normalized_path.starts_with(normalized_root)) {
        return false;
    }
    if (normalized_path.size() <= normalized_root.size()) {
        return false;
    }
    const char separator = normalized_path[normalized_root.size()];
    return separator == '/' || separator == '\\';
}

} // namespace

CloudPathMatch detect_cloud_path_match(const std::string& root_path,
                                       const std::vector<std::string>& path_markers,
                                       const std::vector<std::string>& env_var_names)
{
    const std::string normalized_path = normalize_path_for_detection(root_path);
    if (normalized_path.empty()) {
        return {};
    }

    CloudPathMatch match;
    for (const auto& env_var_name : env_var_names) {
        const char* env_value = std::getenv(env_var_name.c_str());
        if (!env_value || *env_value == '\0') {
            continue;
        }
        if (is_same_or_child_path(normalized_path, normalize_path_for_detection(env_value))) {
            match.matched = true;
            match.confidence = std::max(match.confidence, 100);
        }
    }

    for (const auto& marker : path_markers) {
        if (contains_marker_segment(normalized_path, marker)) {
            match.matched = true;
            match.confidence = std::max(match.confidence, 70);
        }
    }

    return match;
}
