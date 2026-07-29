#include "CategorizationResponseParser.hpp"

#include "Utils.hpp"

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#else
#error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

constexpr size_t kMaxLabelLength = 80;

std::string to_lower_copy_str(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trim_copy(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
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
        if (std::isspace(ch)) {
            if (!previous_space) {
                collapsed.push_back(' ');
            }
            previous_space = true;
            continue;
        }
        collapsed.push_back(static_cast<char>(ch));
        previous_space = false;
    }
    return trim_copy(std::move(collapsed));
}

std::string strip_wrapping_punctuation(std::string value)
{
    auto is_wrapping = [](unsigned char ch) {
        switch (ch) {
            case '"':
            case '\'':
            case '`':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '<':
            case '>':
                return true;
            default:
                return false;
        }
    };

    while (!value.empty() && (std::isspace(static_cast<unsigned char>(value.front())) ||
                              is_wrapping(static_cast<unsigned char>(value.front())))) {
        value.erase(value.begin());
    }
    while (!value.empty() && (std::isspace(static_cast<unsigned char>(value.back())) ||
                              is_wrapping(static_cast<unsigned char>(value.back())) ||
                              value.back() == '.' || value.back() == ',' ||
                              value.back() == ':' || value.back() == ';')) {
        value.pop_back();
    }
    return value;
}

std::string strip_trailing_parenthetical_gloss(std::string value)
{
    value = trim_copy(std::move(value));
    while (true) {
        const auto open = value.rfind(" (");
        if (open == std::string::npos) {
            break;
        }

        std::string gloss = trim_copy(value.substr(open + 2));
        if (!gloss.empty() && gloss.back() == ')') {
            gloss.pop_back();
            gloss = trim_copy(std::move(gloss));
        }

        const bool has_alpha_chars = std::any_of(gloss.begin(), gloss.end(), [](unsigned char ch) {
            return std::isalpha(ch);
        });
        if (!has_alpha_chars) {
            break;
        }

        value = trim_copy(value.substr(0, open));
    }
    return value;
}

bool starts_with_case_insensitive(std::string_view value, std::string_view prefix)
{
    if (prefix.size() > value.size()) {
        return false;
    }

    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

std::size_t find_case_insensitive(const std::string& value, std::string_view needle)
{
    const std::string lower_value = to_lower_copy_str(value);
    std::string lower_needle(needle);
    std::transform(lower_needle.begin(), lower_needle.end(), lower_needle.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower_value.find(lower_needle);
}

std::string strip_explanatory_suffix(std::string value)
{
    static const std::vector<std::string_view> markers = {
        " (based on",
        " (note",
        " (since",
        " - this ",
        " - based on",
        " because ",
        " based on ",
        " which ",
        " since ",
        " however ",
        " specifically ",
        " indicating ",
        " indicates ",
        " commonly ",
        " related to "
    };

    std::size_t cut = std::string::npos;
    for (const std::string_view marker : markers) {
        const auto pos = find_case_insensitive(value, marker);
        if (pos != std::string::npos && (cut == std::string::npos || pos < cut)) {
            cut = pos;
        }
    }
    if (cut != std::string::npos) {
        value.resize(cut);
    }

    return strip_wrapping_punctuation(collapse_spaces_copy(std::move(value)));
}

std::string extract_category_phrase(std::string value)
{
    struct PhrasePattern {
        std::string_view prefix;
        std::string_view suffix;
    };
    static const std::vector<PhrasePattern> patterns = {
        {"falls under the ", " category"},
        {"falls under ", " category"},
        {"belongs to the ", " category"},
        {"belongs to ", " category"},
        {"categorized as ", ""},
        {"classified as ", ""},
        {"category is ", ""},
        {"category would be ", ""}
    };

    const std::string lower = to_lower_copy_str(value);
    for (const auto& pattern : patterns) {
        const auto start = lower.find(pattern.prefix);
        if (start == std::string::npos) {
            continue;
        }
        const std::size_t content_start = start + pattern.prefix.size();
        std::size_t content_end = value.size();
        if (!pattern.suffix.empty()) {
            content_end = lower.find(pattern.suffix, content_start);
            if (content_end == std::string::npos || content_end <= content_start) {
                continue;
            }
        }
        return value.substr(content_start, content_end - content_start);
    }
    return value;
}

std::string strip_inline_label_artifacts(std::string value, bool category_label)
{
    const auto find_delimited_label = [](const std::string& text,
                                         const std::vector<std::string_view>& labels) {
        const std::string lower = to_lower_copy_str(text);
        for (std::size_t idx = 0; idx < lower.size(); ++idx) {
            const char delimiter = lower[idx];
            if (delimiter != ',' && delimiter != ';' && delimiter != '-') {
                continue;
            }

            std::size_t label_start = idx + 1;
            while (label_start < lower.size() &&
                   std::isspace(static_cast<unsigned char>(lower[label_start]))) {
                ++label_start;
            }

            for (const std::string_view label : labels) {
                if (label_start + label.size() > lower.size()) {
                    continue;
                }
                if (lower.compare(label_start, label.size(), label) != 0) {
                    continue;
                }

                const std::size_t after_label = label_start + label.size();
                if (after_label == lower.size() ||
                    std::isspace(static_cast<unsigned char>(lower[after_label])) ||
                    lower[after_label] == ':' ||
                    lower[after_label] == '=') {
                    return idx;
                }
            }
        }

        return std::string::npos;
    };

    const std::vector<std::string_view> markers = category_label
        ? std::vector<std::string_view>{
              ", subcategory",
              ", sub category",
              " - subcategory",
              " - sub category",
              "; subcategory",
              "; sub category",
              " subcategory:",
              " sub category:"
          }
        : std::vector<std::string_view>{
              ", category",
              ", main category",
              " - category",
              " - main category",
              "; category",
              "; main category",
              " category:",
              " main category:"
          };

    std::size_t cut = std::string::npos;
    for (const std::string_view marker : markers) {
        const auto pos = find_case_insensitive(value, marker);
        if (pos != std::string::npos && (cut == std::string::npos || pos < cut)) {
            cut = pos;
        }
    }
    if (cut != std::string::npos) {
        value.resize(cut);
    }

    const std::vector<std::string_view> delimited_labels = category_label
        ? std::vector<std::string_view>{"subcategory", "sub category", "sub_category"}
        : std::vector<std::string_view>{"category", "main category", "main_category"};
    if (const auto delimited_cut = find_delimited_label(value, delimited_labels);
        delimited_cut != std::string::npos) {
        value.resize(delimited_cut);
    }

    return trim_copy(std::move(value));
}

std::string strip_leading_label_artifacts(std::string value)
{
    static const std::vector<std::string_view> markers = {
        "category",
        "main category",
        "main_category",
        "subcategory",
        "sub category",
        "sub_category"
    };

    for (const std::string_view marker : markers) {
        if (!starts_with_case_insensitive(value, marker)) {
            continue;
        }

        std::size_t pos = marker.size();
        while (pos < value.size()) {
            const unsigned char ch = static_cast<unsigned char>(value[pos]);
            if (std::isspace(ch) || ch == ':' || ch == '=' || ch == '-' || ch == '>' ||
                ch == '"' || ch == '\'' || ch == '`') {
                ++pos;
                continue;
            }
            break;
        }
        return trim_copy(value.substr(pos));
    }

    return value;
}

std::string normalize_candidate_label(std::string value, bool category_label)
{
    value = strip_wrapping_punctuation(collapse_spaces_copy(trim_copy(std::move(value))));
    if (value.empty()) {
        return value;
    }
    if (category_label) {
        value = extract_category_phrase(std::move(value));
    }
    value = strip_leading_label_artifacts(std::move(value));
    value = strip_explanatory_suffix(std::move(value));
    value = strip_trailing_parenthetical_gloss(std::move(value));
    value = strip_inline_label_artifacts(std::move(value), category_label);
    return strip_wrapping_punctuation(collapse_spaces_copy(std::move(value)));
}

std::string strip_list_prefix(std::string line)
{
    line = trim_copy(std::move(line));
    if (line.empty()) {
        return line;
    }

    if ((line.front() == '-' || line.front() == '*') && line.size() > 1 &&
        std::isspace(static_cast<unsigned char>(line[1]))) {
        return trim_copy(line.substr(1));
    }

    size_t idx = 0;
    while (idx < line.size() && std::isdigit(static_cast<unsigned char>(line[idx]))) {
        ++idx;
    }
    if (idx > 0 && idx + 1 < line.size() &&
        (line[idx] == '.' || line[idx] == ')') &&
        std::isspace(static_cast<unsigned char>(line[idx + 1]))) {
        return trim_copy(line.substr(idx + 1));
    }

    return line;
}

bool has_alpha(const std::string& value)
{
    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalpha(ch);
    });
}

bool is_heading_like_label(const std::string& value)
{
    const std::string lower = to_lower_copy_str(strip_wrapping_punctuation(collapse_spaces_copy(trim_copy(value))));
    static const std::vector<std::string> exact_matches = {
        "category",
        "main category",
        "subcategory",
        "sub category",
        "categorization",
        "classification",
        "result",
        "answer",
        "note",
        "warning",
        "disclaimer",
        "reason",
        "explanation",
        "full path",
        "file name",
        "directory name"
    };
    if (std::find(exact_matches.begin(), exact_matches.end(), lower) != exact_matches.end()) {
        return true;
    }
    return lower.find("categorization") != std::string::npos ||
           lower.find("classification") != std::string::npos;
}

std::vector<std::string> split_segments(const std::string& line, std::string_view delimiter)
{
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto pos = line.find(delimiter, start);
        const std::string segment = trim_copy(line.substr(start, pos == std::string::npos ? pos : pos - start));
        if (!segment.empty()) {
            segments.push_back(segment);
        }
        if (pos == std::string::npos) {
            break;
        }
        start = pos + delimiter.size();
    }
    return segments;
}

std::optional<std::string> extract_labeled_value(const std::string& line,
                                                 std::initializer_list<std::string_view> labels,
                                                 bool category_label)
{
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    const std::string key = to_lower_copy_str(trim_copy(line.substr(0, colon)));
    for (const std::string_view label : labels) {
        if (key == label) {
            const std::string value = normalize_candidate_label(line.substr(colon + 1), category_label);
            if (!value.empty()) {
                return value;
            }
            break;
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_relaxed_labeled_value(const std::string& line,
                                                         std::initializer_list<std::string_view> labels,
                                                         bool category_label)
{
    const std::string cleaned = strip_wrapping_punctuation(collapse_spaces_copy(trim_copy(line)));
    if (cleaned.empty()) {
        return std::nullopt;
    }

    const auto try_variant = [&](std::string_view variant) -> std::optional<std::string> {
        if (!starts_with_case_insensitive(cleaned, variant)) {
            return std::nullopt;
        }

        std::size_t pos = variant.size();
        while (pos < cleaned.size()) {
            const unsigned char ch = static_cast<unsigned char>(cleaned[pos]);
            if (std::isspace(ch) || ch == ':' || ch == '=' || ch == '-' || ch == '>' ||
                ch == '"' || ch == '\'' || ch == '`') {
                ++pos;
                continue;
            }
            break;
        }

        const std::string value = normalize_candidate_label(cleaned.substr(pos), category_label);
        if (value.empty()) {
            return std::nullopt;
        }
        return value;
    };

    for (const std::string_view label : labels) {
        const std::string spaced = collapse_spaces_copy(trim_copy(std::string(label)));
        if (const auto value = try_variant(spaced)) {
            return value;
        }

        std::string underscored = spaced;
        std::replace(underscored.begin(), underscored.end(), ' ', '_');
        if (underscored != spaced) {
            if (const auto value = try_variant(underscored)) {
                return value;
            }
        }
    }

    return std::nullopt;
}

std::string strip_code_fence(std::string output)
{
    output = trim_copy(std::move(output));
    if (output.rfind("```", 0) != 0) {
        return output;
    }

    const auto first_newline = output.find('\n');
    if (first_newline == std::string::npos) {
        return output;
    }

    const auto last_fence = output.rfind("\n```");
    if (last_fence == std::string::npos || last_fence <= first_newline) {
        return output;
    }

    return trim_copy(output.substr(first_newline + 1, last_fence - first_newline - 1));
}

bool split_inline_pair(const std::string& line, std::string& category, std::string& subcategory)
{
    for (std::string_view delimiter : {std::string_view(" : "), std::string_view(":")}) {
        const auto segments = split_segments(line, delimiter);
        if (segments.size() < 2) {
            continue;
        }

        for (std::size_t idx = segments.size() - 1; idx > 0; --idx) {
            const std::string left = normalize_candidate_label(segments[idx - 1], true);
            const std::string right = normalize_candidate_label(segments[idx], false);
            if (left.size() < 2 || right.empty()) {
                continue;
            }
            if (!has_alpha(left) || !has_alpha(right)) {
                continue;
            }
            if (is_heading_like_label(left)) {
                continue;
            }
            category = left;
            subcategory = right;
            return true;
        }
    }
    return false;
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
    return std::isspace(first) || std::isspace(last);
}

bool is_reserved_windows_name(const std::string& value)
{
    static const std::vector<std::string> reserved = {
        "con","prn","aux","nul",
        "com1","com2","com3","com4","com5","com6","com7","com8","com9",
        "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"
    };
    const std::string lower = to_lower_copy_str(value);
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

namespace CategorizationResponseParser {

std::pair<std::string, std::string> split_category_subcategory(const std::string& input)
{
    std::vector<std::string> lines;
    lines.reserve(4);

    std::istringstream iss(input);
    std::string line;
    while (std::getline(iss, line)) {
        std::string cleaned = strip_list_prefix(std::move(line));
        if (!cleaned.empty()) {
            lines.push_back(std::move(cleaned));
        }
    }

    if (lines.empty()) {
        std::string fallback = Utils::sanitize_path_label(trim_copy(input));
        return {fallback, ""};
    }

    std::string category;
    std::string subcategory;

    for (const auto& entry : lines) {
        if (category.empty()) {
            if (auto value = extract_labeled_value(entry, {"category", "main category"}, true)) {
                category = std::move(*value);
            } else if (auto value = extract_relaxed_labeled_value(
                           entry,
                           {"category", "main category"},
                           true)) {
                category = std::move(*value);
            }
        }
        if (subcategory.empty()) {
            if (auto value = extract_labeled_value(entry, {"subcategory", "sub category"}, false)) {
                subcategory = std::move(*value);
            } else if (auto value = extract_relaxed_labeled_value(
                           entry,
                           {"subcategory", "sub category"},
                           false)) {
                subcategory = std::move(*value);
            }
        }
    }

    if (category.empty() || subcategory.empty()) {
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            std::string parsed_category;
            std::string parsed_subcategory;
            if (!split_inline_pair(*it, parsed_category, parsed_subcategory)) {
                continue;
            }
            if (category.empty()) {
                category = std::move(parsed_category);
            }
            if (subcategory.empty()) {
                subcategory = std::move(parsed_subcategory);
            }
            if (!category.empty() && !subcategory.empty()) {
                break;
            }
        }
    }

    if (category.empty() && subcategory.empty()) {
        category = normalize_candidate_label(lines.front(), true);
        if (category.empty()) {
            category = lines.front();
        }
    }

    return {Utils::sanitize_path_label(category), Utils::sanitize_path_label(subcategory)};
}

std::optional<std::pair<std::string, std::string>> parse_translated_category_response(
    const std::string& response)
{
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream stream(response);
    if (Json::parseFromStream(reader, stream, &root, &errors) && root.isObject()) {
        const std::string category = normalize_candidate_label(root.get("category", "").asString(), true);
        const std::string subcategory = normalize_candidate_label(root.get("subcategory", "").asString(), false);
        if (!category.empty()) {
            return std::make_pair(Utils::sanitize_path_label(category),
                                  Utils::sanitize_path_label(subcategory.empty() ? category : subcategory));
        }
    }

    auto [category, subcategory] = split_category_subcategory(response);
    if (category.empty()) {
        return std::nullopt;
    }
    if (subcategory.empty()) {
        subcategory = category;
    }
    return std::make_pair(category, subcategory);
}

LabelValidationResult validate_labels(const std::string& category,
                                      const std::string& subcategory)
{
    if (category.empty() || subcategory.empty()) {
        return {false, "Category or subcategory is empty"};
    }
    if (category.size() > kMaxLabelLength || subcategory.size() > kMaxLabelLength) {
        return {false, "Category or subcategory exceeds max length"};
    }
    if (!contains_only_allowed_chars(category) || !contains_only_allowed_chars(subcategory)) {
        return {false, "Category or subcategory contains disallowed characters"};
    }
    if (looks_like_extension_label(category) || looks_like_extension_label(subcategory)) {
        return {false, "Category or subcategory looks like a file extension"};
    }
    if (is_reserved_windows_name(category) || is_reserved_windows_name(subcategory)) {
        return {false, "Category or subcategory is a reserved name"};
    }
    if (has_leading_or_trailing_space_or_dot(category) || has_leading_or_trailing_space_or_dot(subcategory)) {
        return {false, "Category or subcategory has leading/trailing space or dot"};
    }
    if (to_lower_copy_str(category) == to_lower_copy_str(subcategory)) {
        return {false, "Category and subcategory are identical"};
    }
    return {true, {}};
}

} // namespace CategorizationResponseParser
