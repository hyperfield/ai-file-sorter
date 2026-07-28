#include "LocalLLMResponseSanitizer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string collapse_spaces_copy(std::string value) {
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

std::string strip_wrapping_punctuation(std::string value) {
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

std::string strip_trailing_parenthetical_gloss(std::string value) {
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

std::size_t find_case_insensitive(const std::string& value, std::string_view needle) {
    const std::string lower_value = to_lower_copy(value);
    std::string lower_needle(needle);
    std::transform(lower_needle.begin(), lower_needle.end(), lower_needle.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower_value.find(lower_needle);
}

std::string strip_explanatory_suffix(std::string value) {
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

std::string extract_category_phrase(std::string value) {
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

    const std::string lower = to_lower_copy(value);
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

std::string strip_inline_label_artifacts(std::string value, bool category_label) {
    const auto find_delimited_label = [](const std::string& text,
                                         const std::vector<std::string_view>& labels) {
        const std::string lower = to_lower_copy(text);
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

    const auto markers = category_label
        ? std::array<std::string_view, 8>{
              ", subcategory",
              ", sub category",
              " - subcategory",
              " - sub category",
              "; subcategory",
              "; sub category",
              " subcategory:",
              " sub category:"
          }
        : std::array<std::string_view, 8>{
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

std::string normalize_candidate_label(std::string value, bool category_label) {
    value = strip_wrapping_punctuation(collapse_spaces_copy(trim_copy(std::move(value))));
    if (value.empty()) {
        return value;
    }
    if (category_label) {
        value = extract_category_phrase(std::move(value));
    }
    value = strip_explanatory_suffix(std::move(value));
    value = strip_trailing_parenthetical_gloss(std::move(value));
    value = strip_inline_label_artifacts(std::move(value), category_label);
    return strip_wrapping_punctuation(collapse_spaces_copy(std::move(value)));
}

bool has_alpha(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalpha(ch);
    });
}

bool case_insensitive_contains(std::string_view text, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }

    std::string text_lower(text);
    std::string needle_lower(needle);
    std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(needle_lower.begin(), needle_lower.end(), needle_lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text_lower.find(needle_lower) != std::string::npos;
}

bool is_heading_like_label(const std::string& value) {
    const std::string lower = to_lower_copy(strip_wrapping_punctuation(collapse_spaces_copy(trim_copy(value))));
    static const std::array<std::string_view, 16> exact_matches = {
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
    for (const std::string_view candidate : exact_matches) {
        if (lower == candidate) {
            return true;
        }
    }
    return case_insensitive_contains(lower, "categorization") ||
           case_insensitive_contains(lower, "classification");
}

std::vector<std::string> split_segments(const std::string& line, std::string_view delimiter) {
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

std::optional<std::pair<std::string, std::string>> extract_inline_pair_from_line(const std::string& line) {
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
            return std::make_pair(left, right);
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_labeled_value(const std::string& line,
                                                 std::initializer_list<std::string_view> labels,
                                                 bool category_label) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    const std::string key = to_lower_copy(trim_copy(line.substr(0, colon)));
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

std::string strip_code_fence(std::string output) {
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

} // namespace

namespace LocalLLMResponseSanitizer {

std::string sanitize_categorization_output(std::string output) {
    output = strip_code_fence(std::move(output));
    if (output.empty()) {
        return output;
    }

    std::vector<std::string> lines;
    std::istringstream input(output);
    for (std::string line; std::getline(input, line); ) {
        line = trim_copy(std::move(line));
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
    }

    if (lines.empty()) {
        return output;
    }

    std::string category;
    std::string subcategory;
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (subcategory.empty()) {
            if (auto value = extract_labeled_value(*it, {"subcategory", "sub category"}, false)) {
                subcategory = std::move(*value);
            }
        }
        if (category.empty()) {
            if (auto value = extract_labeled_value(*it, {"category", "main category"}, true)) {
                category = std::move(*value);
            }
        }
        if (!category.empty() && !subcategory.empty()) {
            return category + " : " + subcategory;
        }
    }

    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (auto pair = extract_inline_pair_from_line(*it)) {
            return pair->first + " : " + pair->second;
        }
    }

    if (!category.empty()) {
        return category;
    }

    return output;
}

} // namespace LocalLLMResponseSanitizer
