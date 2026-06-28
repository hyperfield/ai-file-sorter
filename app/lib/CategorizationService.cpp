#include "CategorizationService.hpp"

#include "ArtifactCategoryPolicy.hpp"
#include "FileCategoryPolicy.hpp"
#include "Settings.hpp"
#include "CategoryLanguage.hpp"
#include "DatabaseManager.hpp"
#include "ILLMClient.hpp"
#include "LLMErrors.hpp"
#include "TestHooks.hpp"
#include "UserLearningStore.hpp"
#include "Utils.hpp"

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#else
#error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <sstream>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {
constexpr const char* kLocalTimeoutEnv = "AI_FILE_SORTER_LOCAL_LLM_TIMEOUT";
constexpr const char* kRemoteTimeoutEnv = "AI_FILE_SORTER_REMOTE_LLM_TIMEOUT";
constexpr const char* kCustomTimeoutEnv = "AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT";
constexpr const char* kRemoteRequestsPerMinuteEnv = "AI_FILE_SORTER_REMOTE_REQUESTS_PER_MINUTE";
constexpr size_t kMaxConsistencyHints = 5;
constexpr size_t kLargeWhitelistPromptThreshold = 30;
constexpr size_t kMaxLargeWhitelistPromptCandidates = 8;
constexpr size_t kMaxLabelLength = 80;
constexpr std::string_view kImageDescriptionMarker = "\nImage description: ";
constexpr std::string_view kDocumentSummaryMarker = "\nDocument summary: ";
constexpr int kMinimumLearnedPreferenceScore = 12;
constexpr int kCategoryTranslationCompletionTokens = 128;
std::string to_lower_copy_str(std::string value);
std::pair<std::string, std::string> split_category_subcategory(const std::string& input);
std::string strip_code_fence(std::string output);

#ifdef AI_FILE_SORTER_TEST_BUILD
TestHooks::CategorizationSleepProbe& categorization_sleep_probe()
{
    static TestHooks::CategorizationSleepProbe probe;
    return probe;
}
#endif

void sleep_for_categorization(std::chrono::milliseconds duration)
{
#ifdef AI_FILE_SORTER_TEST_BUILD
    if (auto& probe = categorization_sleep_probe()) {
        probe(duration);
        return;
    }
#endif

    std::this_thread::sleep_for(duration);
}
bool starts_with_case_insensitive(std::string_view value, std::string_view prefix);
std::optional<std::string> extract_relaxed_labeled_value_from_response(
    const std::string& response,
    std::initializer_list<std::string_view> labels,
    bool category_label);

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

std::string first_line_copy(std::string value) {
    const auto newline = value.find('\n');
    if (newline != std::string::npos) {
        value.resize(newline);
    }
    return trim_copy(std::move(value));
}

bool is_whitelist_learning_source(const std::string& source) {
    constexpr std::string_view kWhitelistSourcePrefix = "whitelist:";
    return source.size() >= kWhitelistSourcePrefix.size() &&
           source.compare(0, kWhitelistSourcePrefix.size(), kWhitelistSourcePrefix) == 0;
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
    const std::string lower_value = to_lower_copy_str(value);
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

std::string strip_inline_label_artifacts(std::string value, bool category_label) {
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

std::string strip_leading_label_artifacts(std::string value) {
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

std::string normalize_candidate_label(std::string value, bool category_label) {
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

std::string strip_list_prefix(std::string line) {
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

bool has_alpha(const std::string& value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalpha(ch);
    });
}

bool is_heading_like_label(const std::string& value) {
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

bool contains_any_substring(const std::string& haystack,
                            std::initializer_list<std::string_view> needles) {
    for (const std::string_view needle : needles) {
        if (!needle.empty() && haystack.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> tokenize_prompt_text(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string current;
    for (unsigned char ch : text) {
        if (std::isalnum(ch) || ch >= 128) {
            current.push_back(ch < 128 ? static_cast<char>(std::tolower(ch)) : static_cast<char>(ch));
            continue;
        }
        if (!current.empty()) {
            tokens.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }
    tokens.erase(std::remove_if(tokens.begin(),
                                tokens.end(),
                                [](const std::string& token) {
                                    return token.size() < 2;
                                }),
                 tokens.end());
    return tokens;
}

int score_label_against_query(const std::string& label, const std::vector<std::string>& query_tokens)
{
    if (label.empty() || query_tokens.empty()) {
        return 0;
    }

    const std::string lowered_label = to_lower_copy_str(label);
    int score = 0;
    for (const auto& token : query_tokens) {
        if (lowered_label == token) {
            score += 6;
        } else if (lowered_label.find(token) != std::string::npos) {
            score += 3;
        }
    }
    return score;
}

std::vector<std::string> rank_allowed_labels_for_query(const std::vector<std::string>& labels,
                                                       const std::string& query_text,
                                                       std::size_t limit)
{
    struct ScoredLabel {
        std::string label;
        int score{0};
        std::size_t order{0};
    };

    const auto query_tokens = tokenize_prompt_text(query_text);
    std::vector<ScoredLabel> scored;
    scored.reserve(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const int score = score_label_against_query(labels[i], query_tokens);
        if (score > 0) {
            scored.push_back(ScoredLabel{labels[i], score, i});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.order < rhs.order;
    });

    std::vector<std::string> ranked;
    ranked.reserve(std::min(limit, scored.size()));
    for (const auto& entry : scored) {
        if (ranked.size() == limit) {
            break;
        }
        if (std::find(ranked.begin(), ranked.end(), entry.label) == ranked.end()) {
            ranked.push_back(entry.label);
        }
    }
    return ranked;
}

bool has_image_description_context(const std::string& prompt_path) {
    return prompt_path.find(kImageDescriptionMarker) != std::string::npos;
}

bool is_image_prompt_context(const std::string& prompt_name,
                             FileType file_type) {
    return file_type == FileType::File &&
           FileCategoryPolicy::is_supported_image_file_name(prompt_name);
}

bool is_document_prompt_context(const std::string& prompt_name,
                                FileType file_type) {
    return file_type == FileType::File &&
           FileCategoryPolicy::is_supported_document_file_name(prompt_name);
}

std::string extract_learning_context_text(const std::string& prompt_path) {
    if (prompt_path.find(kImageDescriptionMarker) == std::string::npos &&
        prompt_path.find(kDocumentSummaryMarker) == std::string::npos) {
        return {};
    }
    const auto newline = prompt_path.find('\n');
    if (newline == std::string::npos || newline + 1 >= prompt_path.size()) {
        return {};
    }
    return trim_copy(prompt_path.substr(newline + 1));
}

std::string extract_image_description_text(const std::string& prompt_path) {
    const auto marker_pos = prompt_path.find(kImageDescriptionMarker);
    if (marker_pos == std::string::npos) {
        return {};
    }

    const std::size_t content_pos = marker_pos + kImageDescriptionMarker.size();
    if (content_pos >= prompt_path.size()) {
        return {};
    }

    return prompt_path.substr(content_pos);
}

std::string extract_document_summary_text(const std::string& prompt_path) {
    const auto marker_pos = prompt_path.find(kDocumentSummaryMarker);
    if (marker_pos == std::string::npos) {
        return {};
    }

    const std::size_t content_pos = marker_pos + kDocumentSummaryMarker.size();
    if (content_pos >= prompt_path.size()) {
        return {};
    }

    return prompt_path.substr(content_pos);
}

std::string extract_base_prompt_path(const std::string& prompt_path) {
    const auto newline = prompt_path.find('\n');
    if (newline == std::string::npos) {
        return prompt_path;
    }
    return prompt_path.substr(0, newline);
}

bool is_screenshot_like_image_context(const std::string& prompt_name,
                                      const std::string& prompt_path) {
    const std::string lowered_name = to_lower_copy_str(prompt_name);
    const std::string lowered_description = to_lower_copy_str(extract_image_description_text(prompt_path));
    return contains_any_substring(lowered_name, {
               "screenshot", "screen", "interface", "dashboard", "webpage", "website",
               "admin", "browser", "window", "mockup", "wireframe", "layout", "form"
           }) ||
           contains_any_substring(lowered_description, {
               "screenshot", "screen capture", "user interface", "ui", "dashboard",
               "webpage", "website", "admin panel", "browser window", "application window",
               "app interface", "desktop interface", "form", "layout", "mockup", "wireframe"
	           });
}

bool is_low_information_label(const std::string& label) {
    const std::string normalized = to_lower_copy_str(trim_copy(label));
    return normalized.empty() ||
           normalized == "documents" ||
           normalized == "document" ||
           normalized == "files" ||
           normalized == "file" ||
           normalized == "general" ||
           normalized == "miscellaneous" ||
           normalized == "misc" ||
           normalized == "other" ||
           normalized == "uncategorized";
}

bool is_low_information_image_label(const std::string& label)
{
    const std::string normalized = to_lower_copy_str(trim_copy(label));
    return is_low_information_label(label) ||
           normalized == "image" ||
           normalized == "images" ||
           normalized == "photo" ||
           normalized == "photos" ||
           normalized == "picture" ||
           normalized == "pictures" ||
           normalized == "graphic" ||
           normalized == "graphics" ||
           normalized == "screenshot" ||
           normalized == "screenshots" ||
           normalized == "wallpaper" ||
           normalized == "wallpapers";
}

bool is_stable_document_main_category(const std::string& label)
{
    const std::string normalized = to_lower_copy_str(trim_copy(label));
    return normalized == "documents" ||
           normalized == "presentations" ||
           normalized == "spreadsheets" ||
           normalized == "data exports" ||
           normalized == "configs";
}

std::string choose_document_subcategory(const std::string& stable_main_category,
                                        const std::string& category,
                                        const std::string& subcategory)
{
    const auto same_label = [](const std::string& lhs, const std::string& rhs) {
        return to_lower_copy_str(trim_copy(lhs)) == to_lower_copy_str(trim_copy(rhs));
    };
    const auto normalize_candidate = [&](const std::string& value, bool allow_stable_main) {
        const std::string sanitized = Utils::sanitize_path_label(trim_copy(value));
        if (sanitized.empty() || same_label(sanitized, stable_main_category)) {
            return std::string();
        }
        if (is_low_information_label(sanitized)) {
            return std::string();
        }
        if (!allow_stable_main && is_stable_document_main_category(sanitized)) {
            return std::string();
        }
        return sanitized;
    };

    if (const std::string candidate = normalize_candidate(subcategory, false); !candidate.empty()) {
        return candidate;
    }
    if (const std::string candidate = normalize_candidate(category, false); !candidate.empty()) {
        return candidate;
    }
    if (const std::string candidate = normalize_candidate(subcategory, true); !candidate.empty()) {
        return candidate;
    }
    if (const std::string candidate = normalize_candidate(category, true); !candidate.empty()) {
        return candidate;
    }
    return "General";
}

std::string choose_image_subcategory(const std::string& stable_main_category,
                                     const std::string& category,
                                     const std::string& subcategory)
{
    const auto same_label = [](const std::string& lhs, const std::string& rhs) {
        return to_lower_copy_str(trim_copy(lhs)) == to_lower_copy_str(trim_copy(rhs));
    };
    const auto normalize_candidate = [&](const std::string& value) {
        const std::string sanitized = Utils::sanitize_path_label(trim_copy(value));
        if (sanitized.empty() || same_label(sanitized, stable_main_category)) {
            return std::string();
        }
        if (is_low_information_image_label(sanitized)) {
            return std::string();
        }
        return sanitized;
    };

    if (const std::string candidate = normalize_candidate(subcategory); !candidate.empty()) {
        return candidate;
    }
    if (const std::string candidate = normalize_candidate(category); !candidate.empty()) {
        return candidate;
    }
    return "General";
}

// Returns true when the value appears in the allowed list (case-insensitive).
bool is_allowed(const std::string& value, const std::vector<std::string>& allowed) {
    if (allowed.empty()) {
        return true;
    }
    const std::string norm = to_lower_copy_str(value);
    for (const auto& item : allowed) {
        if (to_lower_copy_str(item) == norm) {
            return true;
        }
    }
    return false;
}

std::pair<std::string, std::string> normalize_image_category_labels(
    const std::string& prompt_name,
    FileType file_type,
    const std::string& category,
    const std::string& subcategory,
    bool whitelist_enabled,
    const std::vector<std::string>& allowed_categories)
{
    if (!is_image_prompt_context(prompt_name, file_type)) {
        return {category, subcategory};
    }
    if (whitelist_enabled && !is_allowed("Images", allowed_categories)) {
        return {category, subcategory};
    }

    const auto stable_main = FileCategoryPolicy::preferred_main_category_for_file_name(prompt_name);
    if (!stable_main) {
        return {category, subcategory};
    }

    return {*stable_main, choose_image_subcategory(*stable_main, category, subcategory)};
}

std::pair<std::string, std::string> normalize_document_category_labels(
    const std::string& prompt_name,
    FileType file_type,
    const std::string& category,
    const std::string& subcategory,
    bool whitelist_enabled)
{
    if (file_type != FileType::File || whitelist_enabled) {
        return {category, subcategory};
    }

    const auto stable_main = FileCategoryPolicy::preferred_main_category_for_file_name(prompt_name);
    if (!stable_main) {
        return {category, subcategory};
    }

    return {*stable_main, choose_document_subcategory(*stable_main, category, subcategory)};
}

std::pair<std::string, std::string> normalize_artifact_category_labels(
    const std::string& prompt_name,
    FileType file_type,
    const std::string& category,
    const std::string& subcategory,
    bool whitelist_enabled)
{
    if (file_type != FileType::File || whitelist_enabled) {
        return {category, subcategory};
    }

    const auto normalized = ArtifactCategoryPolicy::normalize_category_labels(prompt_name, category, subcategory);
    if (!normalized) {
        return {category, subcategory};
    }

    return {normalized->category, normalized->subcategory};
}

// Returns the first allowed entry or an empty string when the list is empty.
std::string first_allowed_or_blank(const std::vector<std::string>& allowed) {
    return allowed.empty() ? std::string() : allowed.front();
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

std::optional<std::string> extract_labeled_value(const std::string& line,
                                                 std::initializer_list<std::string_view> labels,
                                                 bool category_label) {
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

bool starts_with_case_insensitive(std::string_view value, std::string_view prefix) {
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

std::optional<std::string> extract_relaxed_labeled_value(const std::string& line,
                                                         std::initializer_list<std::string_view> labels,
                                                         bool category_label) {
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

std::optional<std::string> extract_relaxed_labeled_value_from_response(
    const std::string& response,
    std::initializer_list<std::string_view> labels,
    bool category_label) {
    const std::string cleaned = strip_code_fence(response);
    if (const auto value = extract_relaxed_labeled_value(cleaned, labels, category_label)) {
        return value;
    }

    std::istringstream iss(cleaned);
    for (std::string line; std::getline(iss, line); ) {
        if (const auto value = extract_relaxed_labeled_value(line, labels, category_label)) {
            return value;
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

bool split_inline_pair(const std::string& line, std::string& category, std::string& subcategory) {
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

// Splits common category/subcategory response variants and sanitizes the labels.
std::pair<std::string, std::string> split_category_subcategory(const std::string& input) {
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

std::optional<std::pair<std::string, std::string>> parse_translated_category_response(const std::string& response) {
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

// Returns a lowercase copy of the input string.
std::string to_lower_copy_str(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

// Returns true when the label contains only allowed characters.
bool contains_only_allowed_chars(const std::string& value) {
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        static const std::string forbidden = R"(<>:"/\|?*)";
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
        // Everything else is allowed (including non-ASCII letters and punctuation).
    }
    return true;
}

// Returns true when the label has leading/trailing whitespace.
bool has_leading_or_trailing_space_or_dot(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    const unsigned char last = static_cast<unsigned char>(value.back());
    // Only guard leading/trailing whitespace; dots are allowed.
    return std::isspace(first) || std::isspace(last);
}

// Returns true when the label matches a reserved Windows device name.
bool is_reserved_windows_name(const std::string& value) {
    static const std::vector<std::string> reserved = {
        "con","prn","aux","nul",
        "com1","com2","com3","com4","com5","com6","com7","com8","com9",
        "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"
    };
    const std::string lower = to_lower_copy_str(value);
    return std::find(reserved.begin(), reserved.end(), lower) != reserved.end();
}

// Returns true when the label looks like a file extension.
bool looks_like_extension_label(const std::string& value) {
    const auto dot_pos = value.rfind('.');
    if (dot_pos == std::string::npos || dot_pos == value.size() - 1) {
        return false;
    }
    const std::string ext = value.substr(dot_pos + 1);
    if (ext.empty() || ext.size() > 5) {
        return false;
    }
    return std::all_of(ext.begin(), ext.end(), [](unsigned char ch) { return std::isalpha(ch); });
}

// Result for category/subcategory validation.
struct LabelValidationResult {
    bool valid{false};
    std::string error;
};

// Validates category/subcategory labels for length and invalid content.
LabelValidationResult validate_labels(const std::string& category, const std::string& subcategory) {
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

}

CategorizationService::CategorizationService(Settings& settings,
                                             DatabaseManager& db_manager,
                                             std::shared_ptr<spdlog::logger> core_logger,
                                             UserLearningStore* user_learning_store)
    : settings(settings),
      db_manager(db_manager),
      core_logger(std::move(core_logger)),
      user_learning_store_(user_learning_store) {}

bool CategorizationService::ensure_remote_credentials(std::string* error_message) const
{
    const LLMChoice choice = settings.get_llm_choice();
    if (!is_remote_choice(choice)) {
        return true;
    }

    if (choice == LLMChoice::Remote_Custom) {
        const auto id = settings.get_active_custom_api_id();
        const CustomApiEndpoint endpoint = settings.find_custom_api_endpoint(id);
        if (is_valid_custom_api_endpoint(endpoint)) {
            return true;
        }
        if (core_logger) {
            core_logger->error("Custom API endpoint selected but is missing required settings.");
        }
        if (error_message) {
            *error_message = "Custom API endpoint is missing required settings. Please edit it in the Select LLM dialog.";
        }
        return false;
    }

    const bool has_key = (choice == LLMChoice::Remote_OpenAI)
        ? !settings.get_openai_api_key().empty()
        : !settings.get_gemini_api_key().empty();
    if (has_key) {
        return true;
    }

    const char* provider = choice == LLMChoice::Remote_OpenAI ? "OpenAI" : "Gemini";
    if (core_logger) {
        core_logger->error("Remote LLM selected but {} API key is not configured.", provider);
    }
    if (error_message) {
        *error_message = fmt::format("Remote model credentials are missing. Enter your {} API key in the Select LLM dialog.", provider);
    }
    return false;
}

std::vector<CategorizedFile> CategorizationService::prune_empty_cached_entries(const std::string& directory_path)
{
    return db_manager.remove_empty_categorizations(directory_path);
}

std::vector<CategorizedFile> CategorizationService::load_cached_entries(const std::string& directory_path) const
{
    auto cached = settings.get_include_subdirectories()
        ? db_manager.get_categorized_files_recursive(directory_path)
        : db_manager.get_categorized_files(directory_path);

    const CategoryLanguage language = settings.get_category_language();
    for (auto& entry : cached) {
        entry = db_manager.localize_categorized_file(entry, language);
    }
    return cached;
}

std::vector<CategorizedFile> CategorizationService::categorize_entries(
    const std::vector<FileEntry>& files,
    bool is_local_llm,
    std::atomic<bool>& stop_flag,
    const ProgressCallback& progress_callback,
    const QueueCallback& queue_callback,
    const CompletionCallback& completion_callback,
    const RecategorizationCallback& recategorization_callback,
    std::function<std::unique_ptr<ILLMClient>()> llm_factory,
    const PromptOverrideProvider& prompt_override,
    const SuggestedNameProvider& suggested_name_provider) const
{
    std::vector<CategorizedFile> categorized;
    if (files.empty()) {
        return categorized;
    }
    if (stop_flag.load()) {
        return categorized;
    }

    auto llm = llm_factory ? llm_factory() : nullptr;
    if (!llm) {
        throw std::runtime_error("Failed to create LLM client.");
    }

    categorized.reserve(files.size());
    SessionHistoryMap session_history;
    const int remote_requests_per_minute = is_local_llm ? 0 : resolve_remote_requests_per_minute();
    std::chrono::steady_clock::time_point next_remote_request = std::chrono::steady_clock::now();
    RemoteThrottleCallback remote_throttle_callback;
    if (remote_requests_per_minute > 0) {
        const auto interval_ms = std::chrono::milliseconds(
            std::max(1, (60000 + remote_requests_per_minute - 1) / remote_requests_per_minute));
        remote_throttle_callback = [&](const std::string& item_name) {
            auto now = std::chrono::steady_clock::now();
            if (now < next_remote_request) {
                const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    next_remote_request - now).count();
                if (progress_callback && wait_ms >= 1000) {
                    progress_callback(fmt::format(
                        "[REMOTE] Waiting {:.1f}s to respect the configured request limit before {}...",
                        static_cast<double>(wait_ms) / 1000.0,
                        item_name));
                }
                while (now < next_remote_request) {
                    if (stop_flag.load()) {
                        return false;
                    }
                    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                        next_remote_request - now);
                    sleep_for_categorization(std::min(remaining, std::chrono::milliseconds(250)));
                    now = std::chrono::steady_clock::now();
                }
            }
            next_remote_request = std::chrono::steady_clock::now() + interval_ms;
            return true;
        };
    }

    for (const auto& entry : files) {
        if (stop_flag.load()) {
            break;
        }

        if (queue_callback) {
            queue_callback(entry);
        }

        const std::string suggested_name = suggested_name_provider
            ? suggested_name_provider(entry)
            : std::string();
        const auto override_value = prompt_override ? prompt_override(entry) : std::nullopt;
        if (auto categorized_entry = categorize_single_entry(*llm,
                                                             is_local_llm,
                                                             entry,
                                                             override_value,
                                                             suggested_name,
                                                             stop_flag,
                                                              progress_callback,
                                                              recategorization_callback,
                                                              session_history,
                                                              remote_throttle_callback)) {
            categorized.push_back(*categorized_entry);
        }

        if (completion_callback) {
            completion_callback(entry);
        }
    }

    return categorized;
}

std::string CategorizationService::build_whitelist_context() const
{
    std::ostringstream oss;
    const auto cats = settings.get_allowed_categories();
    const auto subs = settings.get_allowed_subcategories();
    if (!cats.empty()) {
        oss << "Allowed main categories (pick exactly one label from the numbered list):\n";
        for (size_t i = 0; i < cats.size(); ++i) {
            oss << (i + 1) << ") " << cats[i] << "\n";
        }
    }
    if (!subs.empty()) {
        oss << "Allowed subcategories (pick exactly one label from the numbered list):\n";
        for (size_t i = 0; i < subs.size(); ++i) {
            oss << (i + 1) << ") " << subs[i] << "\n";
        }
    } else {
        oss << "Allowed subcategories: any (pick a specific, relevant subcategory; do not repeat the main category).";
    }
    return oss.str();
}

std::string CategorizationService::build_main_category_candidate_context(const std::string& prompt_name,
                                                                         FileType file_type) const
{
    if (settings.get_use_whitelist()) {
        return std::string();
    }

    const auto selection = FileCategoryPolicy::determine_main_category_selection(prompt_name, file_type);
    if (selection.categories.empty()) {
        return std::string();
    }

    std::ostringstream oss;
    oss << "Allowed main categories (pick exactly one label from the numbered list):\n";
    for (std::size_t i = 0; i < selection.categories.size(); ++i) {
        oss << (i + 1) << ") " << selection.categories[i] << "\n";
    }

    if (std::find(selection.categories.begin(), selection.categories.end(), "Other") !=
        selection.categories.end()) {
        oss << "Use Other only when none of the listed family categories clearly fits the file.\n";
    }

    oss << "Allowed subcategories: any (pick a specific, relevant subcategory; do not repeat the main category).";
    return oss.str();
}

std::string CategorizationService::build_whitelist_context_for_prompt(const std::string& prompt_name,
                                                                      const std::string& prompt_path) const
{
    const auto cats = settings.get_allowed_categories();
    const auto subs = settings.get_allowed_subcategories();
    if (cats.size() + subs.size() <= kLargeWhitelistPromptThreshold) {
        return build_whitelist_context();
    }
    return build_large_whitelist_candidate_context(prompt_name, prompt_path);
}

std::string CategorizationService::build_large_whitelist_candidate_context(const std::string& prompt_name,
                                                                          const std::string& prompt_path) const
{
    const auto cats = settings.get_allowed_categories();
    const auto subs = settings.get_allowed_subcategories();
    if (cats.empty()) {
        return build_whitelist_context();
    }

    std::string query = prompt_name;
    if (!prompt_path.empty()) {
        query += "\n";
        query += prompt_path;
    }

    std::vector<CategoryPair> candidates;
    candidates.reserve(kMaxLargeWhitelistPromptCandidates);
    const auto allowed_contains = [](const std::string& value, const std::vector<std::string>& allowed) {
        if (allowed.empty()) {
            return true;
        }
        const std::string normalized = to_lower_copy_str(value);
        return std::any_of(allowed.begin(), allowed.end(), [&normalized](const std::string& entry) {
            return to_lower_copy_str(entry) == normalized;
        });
    };
    const auto append_candidate = [&](std::string category, std::string subcategory) {
        category = Utils::sanitize_path_label(std::move(category));
        subcategory = Utils::sanitize_path_label(std::move(subcategory));
        if (category.empty() || !allowed_contains(category, cats)) {
            return;
        }
        if (!subcategory.empty() && !allowed_contains(subcategory, subs)) {
            subcategory.clear();
        }
        const CategoryPair pair{category, subcategory};
        if (std::find(candidates.begin(), candidates.end(), pair) == candidates.end()) {
            candidates.push_back(pair);
        }
    };

    if (user_learning_store_ && user_learning_store_->is_open()) {
        for (const auto& candidate :
             user_learning_store_->retrieve_taxonomy_candidates(query, kMaxLargeWhitelistPromptCandidates * 2)) {
            append_candidate(candidate.category, candidate.subcategory);
            if (candidates.size() == kMaxLargeWhitelistPromptCandidates) {
                break;
            }
        }
    }

    if (candidates.size() < kMaxLargeWhitelistPromptCandidates) {
        for (const auto& category :
             rank_allowed_labels_for_query(cats, query, kMaxLargeWhitelistPromptCandidates)) {
            append_candidate(category, {});
            if (candidates.size() == kMaxLargeWhitelistPromptCandidates) {
                break;
            }
        }
    }

    std::ostringstream oss;
    oss << "Selected whitelist is large, so only the most relevant allowed candidates are shown.\n";
    if (!candidates.empty()) {
        oss << "Allowed category candidates (pick exactly one when it fits):\n";
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            oss << (i + 1) << ") " << candidates[i].first;
            if (!candidates[i].second.empty()) {
                oss << " : " << candidates[i].second;
            }
            oss << "\n";
        }
    } else {
        oss << "No strong whitelist candidate matched this file. Choose the best category from the selected whitelist if you know it; otherwise suggest a new category for review.\n";
    }

    if (!subs.empty()) {
        const auto ranked_subs = rank_allowed_labels_for_query(subs, query, kMaxLargeWhitelistPromptCandidates);
        if (!ranked_subs.empty()) {
            oss << "Allowed subcategory candidates:\n";
            for (std::size_t i = 0; i < ranked_subs.size(); ++i) {
                oss << (i + 1) << ") " << ranked_subs[i] << "\n";
            }
        } else {
            oss << "Allowed subcategories are restricted by the selected whitelist, but no strong subcategory candidate matched this file.\n";
        }
    } else {
        oss << "Allowed subcategories: any (pick a specific, relevant subcategory; do not repeat the main category).";
    }

    return oss.str();
}

std::string CategorizationService::build_learned_candidate_context(const std::string& prompt_name,
                                                                   const std::string& prompt_path,
                                                                   FileType file_type) const
{
    if (!user_learning_store_ || !user_learning_store_->is_open()) {
        return std::string();
    }

    std::string query = prompt_name;
    if (!prompt_path.empty()) {
        query += "\n";
        query += prompt_path;
    }

    const auto candidates = user_learning_store_->retrieve_taxonomy_candidates(query, 5);
    std::ostringstream oss;
    const auto family_selection =
        settings.get_use_whitelist()
            ? FileCategoryPolicy::MainCategorySelection{}
            : FileCategoryPolicy::determine_main_category_selection(prompt_name, file_type);
    const auto normalize_candidate_for_prompt = [&](std::string category, std::string subcategory) {
        std::tie(category, subcategory) = normalize_image_category_labels(prompt_name,
                                                                          file_type,
                                                                          category,
                                                                          subcategory,
                                                                          settings.get_use_whitelist(),
                                                                          settings.get_allowed_categories());
        std::tie(category, subcategory) = normalize_document_category_labels(prompt_name,
                                                                             file_type,
                                                                             category,
                                                                             subcategory,
                                                                             settings.get_use_whitelist());
        std::tie(category, subcategory) = normalize_artifact_category_labels(prompt_name,
                                                                             file_type,
                                                                             category,
                                                                             subcategory,
                                                                             settings.get_use_whitelist());
        return CategoryPair{std::move(category), std::move(subcategory)};
    };

    std::size_t emitted = 0;
    for (const auto& candidate : candidates) {
        if (is_whitelist_learning_source(candidate.source)) {
            continue;
        }
        if (candidate.score < kMinimumLearnedPreferenceScore) {
            continue;
        }
        auto [normalized_category, normalized_subcategory] =
            normalize_candidate_for_prompt(candidate.category, candidate.subcategory);
        if (!family_selection.categories.empty() &&
            !is_allowed(normalized_category, family_selection.categories)) {
            continue;
        }
        if (emitted == 0) {
            oss << "User-learned category candidates from approved behavior:\n";
        }
        oss << (emitted + 1) << ") " << normalized_category;
        if (!normalized_subcategory.empty()) {
            oss << " : " << normalized_subcategory;
        } else {
            oss << " : choose a specific relevant subcategory";
        }
        oss << "\n";
        ++emitted;
    }

    if (emitted == 0) {
        return std::string();
    }

    oss << "Prefer one of these user-learned candidates when it fits the file. "
           "If none fits, choose a better category and subcategory for review.";
    return oss.str();
}

DatabaseManager::ResolvedCategory CategorizationService::prefer_learned_candidate_for_generic_result(
    const DatabaseManager::ResolvedCategory& resolved,
    const std::string& prompt_name,
    const std::string& prompt_path,
    FileType file_type) const
{
    if (!user_learning_store_ || !user_learning_store_->is_open()) {
        return resolved;
    }

    std::string query = prompt_name;
    if (!prompt_path.empty()) {
        query += "\n";
        query += prompt_path;
    }

    const auto candidates = user_learning_store_->retrieve_taxonomy_candidates(query, 1);
    if (candidates.empty() ||
        is_whitelist_learning_source(candidates.front().source) ||
        candidates.front().score < kMinimumLearnedPreferenceScore) {
        return resolved;
    }

    auto candidate = candidates.front();
    std::tie(candidate.category, candidate.subcategory) = normalize_image_category_labels(prompt_name,
                                                                                          file_type,
                                                                                          candidate.category,
                                                                                          candidate.subcategory,
                                                                                          settings.get_use_whitelist(),
                                                                                          settings.get_allowed_categories());
    std::tie(candidate.category, candidate.subcategory) = normalize_document_category_labels(prompt_name,
                                                                                             file_type,
                                                                                             candidate.category,
                                                                                             candidate.subcategory,
                                                                                             settings.get_use_whitelist());
    std::tie(candidate.category, candidate.subcategory) = normalize_artifact_category_labels(prompt_name,
                                                                                             file_type,
                                                                                             candidate.category,
                                                                                             candidate.subcategory,
                                                                                             settings.get_use_whitelist());
    const auto allowed_categories = settings.get_allowed_categories();
    const auto allowed_subcategories = settings.get_allowed_subcategories();
    if (!settings.get_use_whitelist()) {
        const auto family_selection =
            FileCategoryPolicy::determine_main_category_selection(prompt_name, file_type);
        if (!family_selection.categories.empty() &&
            !is_allowed(candidate.category, family_selection.categories)) {
            return resolved;
        }
    }
    if (settings.get_use_whitelist() && !is_allowed(candidate.category, allowed_categories)) {
        return resolved;
    }
    if (settings.get_use_whitelist() &&
        !candidate.subcategory.empty() &&
        !is_allowed(candidate.subcategory, allowed_subcategories)) {
        return resolved;
    }

    const bool generic_category = is_low_information_label(resolved.category);
    const bool same_category = to_lower_copy_str(resolved.category) ==
                               to_lower_copy_str(candidate.category);
    const bool generic_subcategory =
        is_low_information_label(resolved.subcategory) ||
        to_lower_copy_str(resolved.category) == to_lower_copy_str(resolved.subcategory);
    if (!generic_category && !(same_category && generic_subcategory)) {
        return resolved;
    }

    std::string learned_subcategory = candidate.subcategory;
    if (learned_subcategory.empty()) {
        learned_subcategory = resolved.subcategory.empty() ? "General" : resolved.subcategory;
    }
    if (to_lower_copy_str(candidate.category) == to_lower_copy_str(learned_subcategory)) {
        learned_subcategory = "General";
    }

    auto learned = db_manager.resolve_category(candidate.category, learned_subcategory);
    if (core_logger) {
        core_logger->info("Preferred learned category '{}'/'{}' over generic result '{}'/'{}'",
                          learned.category,
                          learned.subcategory,
                          resolved.category,
                          resolved.subcategory);
    }
    return learned;
}

std::string CategorizationService::build_category_language_context() const
{
    const CategoryLanguage lang = settings.get_category_language();
    if (lang == CategoryLanguage::English) {
        return std::string();
    }
    const std::string name = categoryLanguageDisplay(lang);
    return fmt::format(
        "Determine the canonical main category and subcategory in English only. "
        "Do not translate, do not add bilingual text, do not use parentheses, and do not explain. "
        "The final labels will be translated to {} later.",
        name);
}

std::optional<DatabaseManager::ResolvedCategory> CategorizationService::try_cached_categorization(
    const std::string& item_name,
    const std::string& current_path,
    const std::string& categorization_path,
    const std::string& dir_path,
    FileType file_type,
    const ProgressCallback& progress_callback) const
{
    const auto cached = db_manager.get_categorization_from_db(dir_path, item_name, file_type);
    if (cached.size() < 2) {
        return std::nullopt;
    }

    const std::string sanitized_category = Utils::sanitize_path_label(cached[0]);
    const std::string sanitized_subcategory = Utils::sanitize_path_label(cached[1]);
    if (sanitized_category.empty() || sanitized_subcategory.empty()) {
        if (core_logger) {
            core_logger->warn("Ignoring cached categorization with empty values for '{}'", item_name);
        }
        return std::nullopt;
    }
    const auto validation = validate_labels(sanitized_category, sanitized_subcategory);
    if (!validation.valid) {
        if (core_logger) {
            core_logger->warn("Ignoring cached categorization for '{}' due to validation error: {} (cat='{}', sub='{}')",
                              item_name,
                              validation.error,
                              sanitized_category,
                              sanitized_subcategory);
        }
        return std::nullopt;
    }

    (void)item_name;
    (void)current_path;
    (void)categorization_path;
    (void)progress_callback;
    return db_manager.resolve_category(sanitized_category, sanitized_subcategory);
}

bool CategorizationService::ensure_remote_credentials_for_request(
    const std::string& item_name,
    const ProgressCallback& progress_callback) const
{
    if (!is_remote_choice(settings.get_llm_choice())) {
        return true;
    }

    const LLMChoice choice = settings.get_llm_choice();
    if (choice == LLMChoice::Remote_Custom) {
        const auto id = settings.get_active_custom_api_id();
        const CustomApiEndpoint endpoint = settings.find_custom_api_endpoint(id);
        if (is_valid_custom_api_endpoint(endpoint)) {
            return true;
        }
        const std::string err_msg = fmt::format("[REMOTE] {} (missing custom API settings)", item_name);
        if (progress_callback) {
            progress_callback(err_msg);
        }
        if (core_logger) {
            core_logger->error("{}", err_msg);
        }
        return false;
    }

    const bool has_key = (choice == LLMChoice::Remote_OpenAI)
        ? !settings.get_openai_api_key().empty()
        : !settings.get_gemini_api_key().empty();
    if (has_key) {
        return true;
    }

    const std::string provider = choice == LLMChoice::Remote_OpenAI ? "OpenAI" : "Gemini";
    const std::string err_msg = fmt::format("[REMOTE] {} (missing {} API key)", item_name, provider);
    if (progress_callback) {
        progress_callback(err_msg);
    }
    if (core_logger) {
        core_logger->error("{}", err_msg);
    }
    return false;
}

DatabaseManager::ResolvedCategory CategorizationService::categorize_via_llm(
    ILLMClient& llm,
    bool is_local_llm,
    const std::string& display_name,
    const std::string& display_path,
    const std::string& prompt_name,
    const std::string& prompt_path,
    FileType file_type,
    const ProgressCallback& progress_callback,
    const std::string& consistency_context) const
{
    try {
        const std::string category_subcategory =
            run_llm_with_timeout(llm, prompt_name, prompt_path, file_type, is_local_llm, consistency_context);
        auto [category, subcategory] = split_category_subcategory(category_subcategory);

        const auto allowed_categories = settings.get_allowed_categories();
        const auto allowed_subcategories = settings.get_allowed_subcategories();
        const std::string original_category = category;
        const std::string original_subcategory = subcategory;
        std::tie(category, subcategory) = normalize_image_category_labels(prompt_name,
                                                                          file_type,
                                                                          category,
                                                                          subcategory,
                                                                          settings.get_use_whitelist(),
                                                                          allowed_categories);
        if (core_logger &&
            (category != original_category || subcategory != original_subcategory)) {
            core_logger->info("Normalized image category for '{}' from '{}'/'{}' to '{}'/'{}'",
                              display_name,
                              original_category,
                              original_subcategory,
                              category,
                              subcategory);
        }
        const std::string pre_document_category = category;
        const std::string pre_document_subcategory = subcategory;
        std::tie(category, subcategory) = normalize_document_category_labels(prompt_name,
                                                                             file_type,
                                                                             category,
                                                                             subcategory,
                                                                             settings.get_use_whitelist());
        if (core_logger &&
            (category != pre_document_category || subcategory != pre_document_subcategory)) {
            core_logger->info("Normalized document category for '{}' from '{}'/'{}' to '{}'/'{}'",
                              display_name,
                              pre_document_category,
                              pre_document_subcategory,
                              category,
                              subcategory);
        }
        const std::string pre_artifact_category = category;
        const std::string pre_artifact_subcategory = subcategory;
        std::tie(category, subcategory) = normalize_artifact_category_labels(prompt_name,
                                                                             file_type,
                                                                             category,
                                                                             subcategory,
                                                                             settings.get_use_whitelist());
        if (core_logger &&
            (category != pre_artifact_category || subcategory != pre_artifact_subcategory)) {
            core_logger->info("Normalized artifact category for '{}' from '{}'/'{}' to '{}'/'{}'",
                              display_name,
                              pre_artifact_category,
                              pre_artifact_subcategory,
                              category,
                              subcategory);
        }
        auto resolved = db_manager.resolve_category(category, subcategory);
        if (settings.get_use_whitelist()) {
            if (!is_allowed(resolved.category, allowed_categories)) {
                resolved.category = first_allowed_or_blank(allowed_categories);
            }
            if (!is_allowed(resolved.subcategory, allowed_subcategories)) {
                resolved.subcategory = first_allowed_or_blank(allowed_subcategories);
            }
        }
        resolved = prefer_learned_candidate_for_generic_result(resolved, prompt_name, prompt_path, file_type);
        const auto validation = validate_labels(resolved.category, resolved.subcategory);
        if (!validation.valid) {
            if (progress_callback) {
                progress_callback(fmt::format("[LLM-ERROR] {} (invalid category/subcategory: {})",
                                              display_name,
                                              validation.error));
            }
            if (core_logger) {
                core_logger->warn("Invalid LLM output for '{}': {} (cat='{}', sub='{}')",
                                  display_name,
                                  validation.error,
                                  resolved.category,
                                  resolved.subcategory);
            }
            return DatabaseManager::ResolvedCategory{-1, "", ""};
        }
        if (resolved.category.empty()) {
            resolved.category = "Uncategorized";
        }
        const auto display_resolved = localize_resolved_category(llm, resolved);
        emit_progress_message(progress_callback, "AI", display_name, display_resolved, display_path, prompt_path);
        return resolved;
    } catch (const std::exception& ex) {
        const std::string err_msg = fmt::format("[LLM-ERROR] {} ({})", display_name, ex.what());
        if (progress_callback) {
            progress_callback(err_msg);
        }
        if (core_logger) {
            core_logger->error("LLM error while categorizing '{}': {}", display_name, ex.what());
        }
        throw;
    }
}

void CategorizationService::emit_progress_message(const ProgressCallback& progress_callback,
                                                  std::string_view source,
                                                  const std::string& item_name,
                                                  const DatabaseManager::ResolvedCategory& resolved,
                                                  const std::string& current_path,
                                                  const std::string& categorization_path) const
{
    if (!progress_callback) {
        return;
    }
    const std::string sub = resolved.subcategory.empty() ? "-" : resolved.subcategory;
    const std::string current_path_display = current_path.empty() ? "-" : current_path;
    const std::string categorization_path_display =
        categorization_path.empty() ? current_path_display : first_line_copy(categorization_path);

    progress_callback(fmt::format(
        "[{}] {}\n"
        "    Category            : {}\n"
        "    Subcat              : {}\n"
        "    Current Path        : {}\n"
        "    Categorization Path : {}",
        source, item_name, resolved.category, sub, current_path_display, categorization_path_display));
}

DatabaseManager::ResolvedCategory CategorizationService::categorize_with_cache(
    ILLMClient& llm,
    bool is_local_llm,
    const std::string& display_name,
    const std::string& display_path,
    const std::string& dir_path,
    const std::string& prompt_name,
    const std::string& prompt_path,
    FileType file_type,
    const ProgressCallback& progress_callback,
    const std::string& consistency_context,
    const RemoteThrottleCallback& remote_throttle_callback) const
{
    if (auto cached = try_cached_categorization(display_name,
                                                display_path,
                                                prompt_path,
                                                dir_path,
                                                file_type,
                                                progress_callback)) {
        const auto display_resolved = localize_resolved_category(llm, *cached);
        emit_progress_message(progress_callback, "CACHE", display_name, display_resolved, display_path, prompt_path);
        return *cached;
    }

    if (!is_local_llm && !ensure_remote_credentials_for_request(display_name, progress_callback)) {
        return DatabaseManager::ResolvedCategory{-1, "", ""};
    }

    if (!is_local_llm && remote_throttle_callback && !remote_throttle_callback(display_name)) {
        return DatabaseManager::ResolvedCategory{-1, "", ""};
    }

    return categorize_via_llm(llm,
                              is_local_llm,
                              display_name,
                              display_path,
                              prompt_name,
                              prompt_path,
                              file_type,
                              progress_callback,
                              consistency_context);
}

std::optional<CategorizedFile> CategorizationService::categorize_single_entry(
    ILLMClient& llm,
    bool is_local_llm,
    const FileEntry& entry,
    const std::optional<PromptOverride>& prompt_override,
    const std::string& suggested_name,
    std::atomic<bool>& stop_flag,
    const ProgressCallback& progress_callback,
    const RecategorizationCallback& recategorization_callback,
    SessionHistoryMap& session_history,
    const RemoteThrottleCallback& remote_throttle_callback) const
{
    const std::filesystem::path entry_path = Utils::utf8_to_path(entry.full_path);
    const std::string dir_path = Utils::path_to_utf8(entry_path.parent_path());
    const std::string display_path = Utils::abbreviate_user_path(entry.full_path);
    const std::string prompt_name = prompt_override ? prompt_override->name : entry.file_name;
    const std::string prompt_path = prompt_override ? prompt_override->path : entry.full_path;
    const std::string prompt_path_display = Utils::abbreviate_user_path(prompt_path);
    const bool use_consistency_hints = settings.get_use_consistency_hints();
    const bool rich_image_context = has_image_description_context(prompt_path);
    const std::string extension = extract_extension(entry.file_name);
    const std::string signature = make_file_signature(entry.type, extension);
    std::string hint_block;
    if (use_consistency_hints && !rich_image_context) {
        const auto hints = collect_consistency_hints(signature, session_history, extension, entry.type);
        hint_block = format_hint_block(hints);
    }
    const std::string combined_context = build_combined_context(hint_block,
                                                                prompt_name,
                                                                prompt_path,
                                                                entry.type);

    DatabaseManager::ResolvedCategory resolved;
    bool retried_after_backoff = false;
    while (true) {
        try {
            resolved = run_categorization_with_cache(llm,
                                                     is_local_llm,
                                                     entry,
                                                     display_path,
                                                     dir_path,
                                                     prompt_name,
                                                      prompt_path_display,
                                                      progress_callback,
                                                      combined_context,
                                                      remote_throttle_callback);
            break;
        } catch (const BackoffError& backoff) {
            const int wait_seconds = backoff.retry_after_seconds() > 0 ? backoff.retry_after_seconds() : 60;
            if (progress_callback) {
                progress_callback(fmt::format(
                    "[REMOTE] Rate limit hit. Waiting {}s before retrying {}...",
                    wait_seconds,
                    entry.file_name));
            }
            if (core_logger) {
                core_logger->warn("Rate limit hit for '{}'; retrying in {}s", entry.file_name, wait_seconds);
            }
            for (int remaining = wait_seconds; remaining > 0; --remaining) {
                if (stop_flag.load()) {
                    return std::nullopt;
                }
                if (progress_callback && (remaining % 10 == 0 || remaining <= 3)) {
                    progress_callback(fmt::format("[REMOTE] Retrying {} in {}s...", entry.file_name, remaining));
                }
                sleep_for_categorization(std::chrono::seconds(1));
            }
            if (retried_after_backoff) {
                throw;
            }
            retried_after_backoff = true;
        }
    }

    if (auto retry = handle_empty_result(entry,
                                         dir_path,
                                         resolved,
                                         use_consistency_hints,
                                         is_local_llm,
                                         recategorization_callback)) {
        return retry;
    }

    update_storage_with_result(entry,
                               dir_path,
                               resolved,
                               use_consistency_hints,
                               suggested_name,
                               session_history);

    const auto display_resolved = db_manager.localize_category(resolved, settings.get_category_language());
    CategorizedFile result{dir_path, entry.file_name, entry.type,
                           display_resolved.category, display_resolved.subcategory, resolved.taxonomy_id};
    result.used_consistency_hints = use_consistency_hints;
    result.suggested_name = suggested_name;
    result.canonical_category = resolved.category;
    result.canonical_subcategory = resolved.subcategory;
    result.learning_context = extract_learning_context_text(prompt_path);
    return result;
}

std::string CategorizationService::build_combined_context(const std::string& hint_block,
                                                          const std::string& prompt_name,
                                                          const std::string& prompt_path,
                                                          FileType file_type) const
{
    std::string combined_context;
    const auto allowed_categories = settings.get_allowed_categories();
    const auto allowed_subcategories = settings.get_allowed_subcategories();
    const bool image_context = is_image_prompt_context(prompt_name, file_type);
    const bool document_context = is_document_prompt_context(prompt_name, file_type);
    const bool use_specialized_prompt_context = image_context || document_context;
    const bool large_whitelist =
        settings.get_use_whitelist() &&
        allowed_categories.size() + allowed_subcategories.size() > kLargeWhitelistPromptThreshold;
    const std::string whitelist_block = settings.get_use_whitelist()
        ? build_whitelist_context_for_prompt(prompt_name, prompt_path)
        : std::string();
    const std::string learned_candidate_block = (!use_specialized_prompt_context || large_whitelist)
        ? std::string()
        : build_learned_candidate_context(prompt_name, prompt_path, file_type);
    const std::string language_block = build_category_language_context();
    const std::string family_candidate_block = use_specialized_prompt_context
        ? build_main_category_candidate_context(prompt_name, file_type)
        : std::string();
    const bool rich_image_context =
        image_context && has_image_description_context(prompt_path);
    const bool artifact_context =
        !use_specialized_prompt_context &&
        ArtifactCategoryPolicy::is_supported_artifact_file_name(prompt_name);
    std::string image_block;
    std::string document_block;
    std::string artifact_block;

    if (image_context) {
        std::ostringstream image_guidance;
        image_guidance
            << "Image categorization guidance:\n"
            << "- Keep the main category stable and filesystem-oriented.\n";

        if (settings.get_use_whitelist() && !is_allowed("Images", allowed_categories)) {
            image_guidance
                << "- Respect the active whitelist if one is provided.\n"
                << "- Prefer image-focused labels, and put the depicted subject, scene, or on-screen content in the subcategory when the whitelist allows it.\n";
        } else {
            image_guidance
                << "- Always use Images as the main category, and put the depicted subject, scene, or on-screen content in the subcategory.\n";
        }

        image_guidance
            << "- Categorize the subject matter shown in the image, not merely the file format.\n"
            << "- Keep the subcategory concise and leaf-like: prefer labels such as Pets, Small Mammals, Wildlife, Landscapes, or Dashboard Interfaces, and do not prefix them with Images.\n"
            << "- Treat screenshots, webpage captures, dashboards, forms, mockups, and app interfaces as images depicting content.\n"
            << "- Do not classify a PNG/JPG/WebP screenshot as Software, Operating Systems, Installers, Databases, or similar artifact categories unless the file itself is actually such an artifact.\n"
            << "- Use the filename, extension, and directory context as supporting clues.\n";

        if (rich_image_context) {
            image_guidance
                << "- Use the image description as the primary evidence when it is available.\n";
        }

        if (is_screenshot_like_image_context(prompt_name, prompt_path)) {
            image_guidance
                << "- For UI-like screenshots, prefer labels that describe what is on screen over generic software or operating-system buckets.\n"
                << "- A screenshot of software is usually not the installer, package, ISO, or operating system itself.\n";
        }

        image_block = image_guidance.str();
    }

    if (document_context) {
        std::ostringstream document_guidance;
        document_guidance
            << "Document categorization guidance:\n"
            << "- Categorize the document by its subject matter and content, not merely by its file extension or the application that may have created it.\n"
            << "- Use any provided document summary as the primary evidence when available, and use the filename, extension, and directory context only as supporting clues.\n";

        if (settings.get_use_whitelist()) {
            document_guidance
                << "- Respect the active whitelist if one is provided.\n"
                << "- Keep the main category broad and filesystem-friendly, and put the specific topic in the subcategory when the whitelist allows it.\n";
        } else {
            document_guidance
                << "- Keep the main category stable and filesystem-oriented.\n"
                << "- Prefer one of these main categories when it clearly fits: Documents, Presentations, Spreadsheets, Data Exports, Configs.\n"
                << "- Default to Documents for ordinary PDFs, word-processing files, notes, manuals, letters, brochures, certificates, policies, reports, and articles.\n"
                << "- Use Presentations only for slide decks, Spreadsheets only for workbook-like tabular files, Data Exports only for export-style tabular data files, and Configs only for configuration files.\n"
                << "- Keep the subcategory concise and leaf-like: prefer labels such as PCI DSS, Financial Documents, Camera Guides, or Vendor Services, and do not prefix them with Documents.\n"
                << "- Put the specific topic or subject matter in the subcategory instead of inventing a topical main category like Security, Marketing, or Computing.\n";
        }

        document_block = document_guidance.str();
    }

    if (artifact_context && settings.get_use_whitelist()) {
        std::ostringstream artifact_guidance;
        artifact_guidance
            << "Software and archive artifact guidance:\n"
            << "- Keep the main category stable and filesystem-oriented.\n"
            << "- Use the main category for the broad family, and put the specific software purpose in the subcategory.\n"
            << "- Keep the subcategory concise and leaf-like: prefer labels such as Version Control, Database Tools, Graphics Drivers, or Installer Tools, and do not stack multiple family labels.\n"
            << "- Prefer specific subcategories like Version Control, Database Tools, Screen Recording, Process Monitoring, Graphics Drivers, Virtualization, or Installer Tools when they fit.\n"
            << "- Do not answer with a generic subcategory that merely repeats the main category or another top-level family like Software, Installers, Drivers, Operating Systems, Archives, Data Exports, or Other.\n"
            << "- Use the filename, extension, and directory context as supporting clues.\n";
        artifact_block = artifact_guidance.str();
    }

    if (!language_block.empty()) {
        combined_context += language_block;
    }
    if (!image_block.empty()) {
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += image_block;
    }
    if (!document_block.empty()) {
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += document_block;
    }
    if (!artifact_block.empty()) {
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += artifact_block;
    }
    if (!family_candidate_block.empty()) {
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += family_candidate_block;
    }
    if (settings.get_use_whitelist() && !whitelist_block.empty()) {
        if (core_logger) {
            core_logger->debug("Applying category whitelist ({} cats, {} subs)",
                               settings.get_allowed_categories().size(),
                               settings.get_allowed_subcategories().size());
        }
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += whitelist_block;
    }
    if (!learned_candidate_block.empty()) {
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += learned_candidate_block;
    }
    if (!hint_block.empty()) {
        if (!combined_context.empty()) {
            combined_context += "\n\n";
        }
        combined_context += hint_block;
    }
    return combined_context;
}

DatabaseManager::ResolvedCategory CategorizationService::localize_resolved_category(
    ILLMClient& llm,
    const DatabaseManager::ResolvedCategory& resolved) const
{
    const CategoryLanguage language = settings.get_category_language();
    if (language == CategoryLanguage::English || resolved.taxonomy_id <= 0) {
        return resolved;
    }

    if (!db_manager.get_category_translation(resolved.taxonomy_id, language)) {
        if (const auto translated = translate_resolved_category(llm, resolved)) {
            db_manager.upsert_category_translation(resolved.taxonomy_id,
                                                  language,
                                                  translated->category,
                                                  translated->subcategory);
        }
    }

    return db_manager.localize_category(resolved, language);
}

std::optional<DatabaseManager::ResolvedCategory> CategorizationService::translate_resolved_category(
    ILLMClient& llm,
    const DatabaseManager::ResolvedCategory& resolved) const
{
    const CategoryLanguage language = settings.get_category_language();
    if (language == CategoryLanguage::English || resolved.taxonomy_id <= 0 ||
        resolved.category.empty() || resolved.subcategory.empty()) {
        return std::nullopt;
    }

    const std::string language_name = categoryLanguageDisplay(language);
    const std::string prompt = fmt::format(
        "Translate the following taxonomy labels into {}.\n"
        "Return only JSON in this exact shape: {{\"category\":\"...\",\"subcategory\":\"...\"}}\n"
        "Rules:\n"
        "- Keep the meaning precise.\n"
        "- No English.\n"
        "- No parentheses.\n"
        "- No explanations.\n"
        "- No trailing periods.\n"
        "- Keep the main category broad and concise.\n"
        "- Keep the subcategory specific and concise.\n\n"
        "category: {}\n"
        "subcategory: {}",
        language_name,
        resolved.category,
        resolved.subcategory);

    try {
        const std::string response =
            llm.complete_prompt(prompt, kCategoryTranslationCompletionTokens);
        const auto translated = parse_translated_category_response(response);
        if (!translated) {
            return std::nullopt;
        }

        DatabaseManager::ResolvedCategory translated_resolved{
            resolved.taxonomy_id,
            translated->first,
            translated->second
        };
        const auto validation = validate_labels(translated_resolved.category, translated_resolved.subcategory);
        if (!validation.valid) {
            if (core_logger) {
                core_logger->warn("Ignoring invalid translated category pair for taxonomy {}: {} (cat='{}', sub='{}')",
                                  resolved.taxonomy_id,
                                  validation.error,
                                  translated_resolved.category,
                                  translated_resolved.subcategory);
            }
            return std::nullopt;
        }
        return translated_resolved;
    } catch (const std::exception& ex) {
        if (core_logger) {
            core_logger->warn("Category translation failed for taxonomy {} to {}: {}",
                              resolved.taxonomy_id,
                              language_name,
                              ex.what());
        }
        return std::nullopt;
    }
}

DatabaseManager::ResolvedCategory CategorizationService::run_categorization_with_cache(
    ILLMClient& llm,
    bool is_local_llm,
    const FileEntry& entry,
    const std::string& display_path,
    const std::string& dir_path,
    const std::string& prompt_name,
    const std::string& prompt_path,
    const ProgressCallback& progress_callback,
    const std::string& combined_context,
    const RemoteThrottleCallback& remote_throttle_callback) const
{
    return categorize_with_cache(llm,
                                 is_local_llm,
                                 entry.file_name,
                                 display_path,
                                 dir_path,
                                 prompt_name,
                                 prompt_path,
                                 entry.type,
                                 progress_callback,
                                 combined_context,
                                 remote_throttle_callback);
}

std::optional<CategorizedFile> CategorizationService::handle_empty_result(
    const FileEntry& entry,
    const std::string& dir_path,
    const DatabaseManager::ResolvedCategory& resolved,
    bool used_consistency_hints,
    bool is_local_llm,
    const RecategorizationCallback& recategorization_callback) const
{
    const bool invalid = resolved.taxonomy_id == -1;
    if (!resolved.category.empty() && !resolved.subcategory.empty() && !invalid) {
        return std::nullopt;
    }

    const std::string reason = invalid
        ? "Categorization returned invalid category/subcategory and was skipped."
        : "Categorization returned no result.";

    if (core_logger) {
        core_logger->warn("{} for '{}'.", reason, entry.file_name);
    }

    db_manager.remove_file_categorization(dir_path, entry.file_name, entry.type);

    if (recategorization_callback) {
        CategorizedFile retry_entry{dir_path,
                                    entry.file_name,
                                    entry.type,
                                    resolved.category,
                                    resolved.subcategory,
                                    resolved.taxonomy_id};
        retry_entry.used_consistency_hints = used_consistency_hints;
        recategorization_callback(retry_entry, reason);
    }
    return std::nullopt;
}

void CategorizationService::update_storage_with_result(const FileEntry& entry,
                                                       const std::string& dir_path,
                                                       const DatabaseManager::ResolvedCategory& resolved,
                                                       bool used_consistency_hints,
                                                       const std::string& suggested_name,
                                                       SessionHistoryMap& session_history) const
{
    if (core_logger) {
        core_logger->info("Categorized '{}' as '{} / {}'.",
                          entry.file_name,
                          resolved.category,
                          resolved.subcategory.empty() ? "<none>" : resolved.subcategory);
    }

    db_manager.insert_or_update_file_with_categorization(
        entry.file_name,
        entry.type == FileType::File ? "F" : "D",
        dir_path,
        resolved,
        used_consistency_hints,
        suggested_name);

    const std::string signature = make_file_signature(entry.type, extract_extension(entry.file_name));
    if (!signature.empty()) {
        record_session_assignment(session_history[signature], {resolved.category, resolved.subcategory});
    }
}

std::string CategorizationService::run_llm_with_timeout(
    ILLMClient& llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    bool is_local_llm,
    const std::string& consistency_context) const
{
    const int timeout_seconds = resolve_llm_timeout(is_local_llm);

    auto future = start_llm_future(llm, item_name, item_path, file_type, consistency_context);

    if (future.wait_for(std::chrono::seconds(timeout_seconds)) == std::future_status::timeout) {
        throw std::runtime_error("Timed out waiting for LLM response");
    }

    return future.get();
}

int CategorizationService::resolve_llm_timeout(bool is_local_llm) const
{
    int timeout_seconds = is_local_llm ? 60 : 10;
    const char* timeout_env = std::getenv(is_local_llm ? kLocalTimeoutEnv : kRemoteTimeoutEnv);
    if (!is_local_llm && settings.get_llm_choice() == LLMChoice::Remote_Custom) {
        timeout_seconds = 60;
        timeout_env = std::getenv(kCustomTimeoutEnv);
    }
    if (!timeout_env || *timeout_env == '\0') {
        return timeout_seconds;
    }

    try {
        const int parsed = std::stoi(timeout_env);
        if (parsed > 0) {
            timeout_seconds = parsed;
        } else if (core_logger) {
            core_logger->warn("Ignoring non-positive LLM timeout '{}'", timeout_env);
        }
    } catch (const std::exception& ex) {
        if (core_logger) {
            core_logger->warn("Failed to parse LLM timeout '{}': {}", timeout_env, ex.what());
        }
    }

    if (core_logger) {
        core_logger->debug("Using {} LLM timeout of {} second(s)",
                           is_local_llm ? "local" : "remote",
                           timeout_seconds);
    }

    return timeout_seconds;
}

int CategorizationService::resolve_remote_requests_per_minute() const
{
    int requests_per_minute = settings.get_remote_requests_per_minute();
    const char* limit_env = std::getenv(kRemoteRequestsPerMinuteEnv);
    if (limit_env && *limit_env != '\0') {
        try {
            requests_per_minute = std::max(0, std::stoi(limit_env));
        } catch (const std::exception& ex) {
            if (core_logger) {
                core_logger->warn("Failed to parse remote request limit '{}': {}", limit_env, ex.what());
            }
        }
    }

    if (requests_per_minute > 0 && core_logger) {
        core_logger->debug("Using remote LLM request limit of {} request(s) per minute",
                           requests_per_minute);
    }
    return requests_per_minute;
}

std::future<std::string> CategorizationService::start_llm_future(
    ILLMClient& llm,
    const std::string& item_name,
    const std::string& item_path,
    FileType file_type,
    const std::string& consistency_context) const
{
    auto promise = std::make_shared<std::promise<std::string>>();
    std::future<std::string> future = promise->get_future();

    std::thread([&llm, promise, item_name, item_path, file_type, consistency_context]() mutable {
        try {
            promise->set_value(llm.categorize_file(item_name, item_path, file_type, consistency_context));
        } catch (...) {
            try {
                promise->set_exception(std::current_exception());
            } catch (...) {
                // no-op
            }
        }
    }).detach();

    return future;
}

std::vector<CategorizationService::CategoryPair> CategorizationService::collect_consistency_hints(
    const std::string& signature,
    const SessionHistoryMap& session_history,
    const std::string& extension,
    FileType file_type) const
{
    std::vector<CategoryPair> hints;
    if (signature.empty()) {
        return hints;
    }

    if (auto it = session_history.find(signature); it != session_history.end()) {
        for (const auto& entry : it->second) {
            if (append_unique_hint(hints, entry) && hints.size() == kMaxConsistencyHints) {
                return hints;
            }
        }
    }

    if (hints.size() < kMaxConsistencyHints) {
        const size_t remaining = kMaxConsistencyHints - hints.size();
        const auto db_hints = db_manager.get_recent_categories_for_extension(extension, file_type, remaining);
        for (const auto& entry : db_hints) {
            if (append_unique_hint(hints, entry) && hints.size() == kMaxConsistencyHints) {
                break;
            }
        }
    }

    return hints;
}

std::string CategorizationService::make_file_signature(FileType file_type, const std::string& extension)
{
    const std::string type_tag = (file_type == FileType::Directory) ? "DIR" : "FILE";
    const std::string normalized_extension = extension.empty() ? std::string("<none>") : extension;
    return type_tag + ":" + normalized_extension;
}

std::string CategorizationService::extract_extension(const std::string& file_name)
{
    const auto pos = file_name.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= file_name.size()) {
        return std::string();
    }
    std::string ext = file_name.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

bool CategorizationService::append_unique_hint(std::vector<CategoryPair>& target, const CategoryPair& candidate)
{
    CategoryPair normalized{Utils::sanitize_path_label(candidate.first), Utils::sanitize_path_label(candidate.second)};
    if (normalized.first.empty()) {
        return false;
    }
    if (normalized.second.empty()) {
        normalized.second = normalized.first;
    }
    for (const auto& existing : target) {
        if (existing.first == normalized.first && existing.second == normalized.second) {
            return false;
        }
    }
    target.push_back(std::move(normalized));
    return true;
}

void CategorizationService::record_session_assignment(HintHistory& history, const CategoryPair& assignment)
{
    CategoryPair normalized{Utils::sanitize_path_label(assignment.first), Utils::sanitize_path_label(assignment.second)};
    if (normalized.first.empty()) {
        return;
    }
    if (normalized.second.empty()) {
        normalized.second = normalized.first;
    }

    history.erase(std::remove(history.begin(), history.end(), normalized), history.end());
    history.push_front(normalized);
    if (history.size() > kMaxConsistencyHints) {
        history.pop_back();
    }
}

std::string CategorizationService::format_hint_block(const std::vector<CategoryPair>& hints) const
{
    if (hints.empty()) {
        return std::string();
    }

    std::ostringstream oss;
    oss << "Recent assignments for similar items:\n";
    for (const auto& hint : hints) {
        const std::string sub = hint.second.empty() ? hint.first : hint.second;
        oss << "- " << hint.first << " : " << sub << "\n";
    }
    oss << "Prefer one of the above when it fits; otherwise, choose the closest consistent alternative.";
    return oss.str();
}

#ifdef AI_FILE_SORTER_TEST_BUILD
namespace TestHooks {

void set_categorization_sleep_probe(CategorizationSleepProbe probe)
{
    categorization_sleep_probe() = std::move(probe);
}

void reset_categorization_sleep_probe()
{
    categorization_sleep_probe() = CategorizationSleepProbe{};
}

} // namespace TestHooks
#endif
