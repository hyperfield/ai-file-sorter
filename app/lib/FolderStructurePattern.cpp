#include "FolderStructurePattern.hpp"

#include "FolderStructurePluginProfile.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

enum class PrefixKind {
    None,
    Number,
    Range,
    Alpha
};

struct SegmentPattern {
    PrefixKind kind{PrefixKind::None};
    std::string original;
    std::string prefix;
    std::string label;
    int number{-1};
    int range_start{-1};
    int range_end{-1};
    int width{0};
};

struct PathInfo {
    std::string relative_path;
    std::vector<std::string> segments;
    std::vector<SegmentPattern> patterns;
    std::string label_path;
};

struct ParentCandidate {
    PathInfo info;
    int score{0};
};

constexpr int kStrongParentScore = 10;

std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::string> split_path(std::string_view value)
{
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

std::string join_path(const std::vector<std::string>& segments)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            out << '/';
        }
        out << segments[i];
    }
    return out.str();
}

std::size_t consume_digits(const std::string& value, std::size_t pos)
{
    while (pos < value.size() && std::isdigit(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    return pos;
}

std::size_t skip_spaces(const std::string& value, std::size_t pos)
{
    while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    return pos;
}

std::optional<int> parse_int(std::string_view value)
{
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

SegmentPattern parse_segment(std::string segment)
{
    segment = trim_copy(std::move(segment));
    SegmentPattern parsed;
    parsed.original = segment;
    parsed.label = segment;
    if (segment.empty()) {
        return parsed;
    }

    const std::size_t first_number_end = consume_digits(segment, 0);
    if (first_number_end > 0) {
        const std::size_t after_first = skip_spaces(segment, first_number_end);
        if (after_first < segment.size() && segment[after_first] == '-') {
            const std::size_t second_start = skip_spaces(segment, after_first + 1);
            const std::size_t second_end = consume_digits(segment, second_start);
            const std::size_t label_start = skip_spaces(segment, second_end);
            if (second_end > second_start && label_start < segment.size()) {
                const auto first = parse_int(std::string_view(segment).substr(0, first_number_end));
                const auto second =
                    parse_int(std::string_view(segment).substr(second_start, second_end - second_start));
                if (first && second && *second >= *first) {
                    parsed.kind = PrefixKind::Range;
                    parsed.prefix = segment.substr(0, label_start);
                    parsed.label = trim_copy(segment.substr(label_start));
                    parsed.range_start = *first;
                    parsed.range_end = *second;
                    parsed.width = static_cast<int>(
                        std::max(first_number_end, second_end - second_start));
                    return parsed;
                }
            }
        }

        std::size_t prefix_end = first_number_end;
        if (prefix_end + 1 < segment.size() &&
            (segment[prefix_end] == '.' || segment[prefix_end] == '_' || segment[prefix_end] == '-') &&
            std::isdigit(static_cast<unsigned char>(segment[prefix_end + 1]))) {
            prefix_end = consume_digits(segment, prefix_end + 1);
        }
        const std::size_t label_start = skip_spaces(segment, prefix_end);
        if (label_start > prefix_end && label_start < segment.size()) {
            const auto number = parse_int(std::string_view(segment).substr(0, first_number_end));
            if (number) {
                parsed.kind = PrefixKind::Number;
                parsed.prefix = segment.substr(0, prefix_end);
                parsed.label = trim_copy(segment.substr(label_start));
                parsed.number = *number;
                parsed.width = static_cast<int>(first_number_end);
                return parsed;
            }
        }
    }

    std::size_t alpha_end = 0;
    while (alpha_end < segment.size() &&
           segment[alpha_end] >= 'A' &&
           segment[alpha_end] <= 'Z') {
        ++alpha_end;
    }
    if (alpha_end >= 2 &&
        alpha_end <= 4 &&
        alpha_end < segment.size() &&
        std::isspace(static_cast<unsigned char>(segment[alpha_end]))) {
        parsed.kind = PrefixKind::Alpha;
        parsed.prefix = segment.substr(0, alpha_end);
        parsed.label = trim_copy(segment.substr(skip_spaces(segment, alpha_end)));
    }

    return parsed;
}

PathInfo make_path_info(const FolderTreeCatalog::Entry& entry)
{
    PathInfo info;
    info.relative_path = entry.relative_path;
    info.segments = split_path(entry.relative_path);
    info.patterns.reserve(info.segments.size());
    std::vector<std::string> label_segments;
    label_segments.reserve(info.segments.size());
    for (const auto& segment : info.segments) {
        auto pattern = parse_segment(segment);
        label_segments.push_back(pattern.label.empty() ? pattern.original : pattern.label);
        info.patterns.push_back(std::move(pattern));
    }
    info.label_path = join_path(label_segments);
    return info;
}

std::vector<PathInfo> make_path_infos(const FolderTreeCatalog::Catalog& catalog)
{
    std::vector<PathInfo> infos;
    infos.reserve(catalog.entries().size());
    for (const auto& entry : catalog.entries()) {
        infos.push_back(make_path_info(entry));
    }
    return infos;
}

std::unordered_set<std::string> tokenize(std::string value)
{
    std::replace(value.begin(), value.end(), '/', ' ');
    std::replace(value.begin(), value.end(), '\\', ' ');
    for (char& ch : value) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
            ch = ' ';
        } else {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }

    std::unordered_set<std::string> tokens;
    std::istringstream stream(value);
    std::string token;
    while (stream >> token) {
        if (token.size() > 1) {
            tokens.insert(std::move(token));
        }
    }
    return tokens;
}

void add_aliases(std::unordered_set<std::string>& tokens)
{
    static const std::map<std::string, std::vector<std::string>> aliases = {
        {"admin", {"administration", "office", "records"}},
        {"applications", {"apps", "programs", "software", "tools"}},
        {"apps", {"applications", "programs", "software", "tools"}},
        {"archive", {"archives", "records"}},
        {"archives", {"archive", "records"}},
        {"document", {"documents", "files", "paperwork", "records"}},
        {"documents", {"document", "files", "paperwork", "records"}},
        {"finance", {"financial", "billing", "invoices", "receipts"}},
        {"financial", {"finance", "billing", "invoices", "receipts"}},
        {"image", {"images", "media", "photo", "photos", "pictures"}},
        {"images", {"image", "media", "photo", "photos", "pictures"}},
        {"installer", {"installers", "setup", "software", "programs"}},
        {"installers", {"installer", "setup", "software", "programs"}},
        {"media", {"image", "images", "photo", "photos", "video", "videos"}},
        {"photo", {"image", "images", "media", "photos", "pictures"}},
        {"photos", {"image", "images", "media", "photo", "pictures"}},
        {"program", {"programs", "software", "apps", "applications", "tools"}},
        {"programs", {"program", "software", "apps", "applications", "tools"}},
        {"software", {"programs", "apps", "applications", "installers", "tools"}},
        {"work", {"business", "clients", "office", "projects"}},
    };

    std::vector<std::string> additions;
    for (const auto& token : tokens) {
        if (auto it = aliases.find(token); it != aliases.end()) {
            additions.insert(additions.end(), it->second.begin(), it->second.end());
        }
    }
    tokens.insert(additions.begin(), additions.end());
}

bool is_general_subcategory(std::string_view value)
{
    std::string lowered = Utils::sanitize_path_label(std::string(value));
    lowered = lower_copy(std::move(lowered));
    return lowered.empty() ||
           lowered == "general" ||
           lowered == "misc" ||
           lowered == "miscellaneous" ||
           lowered == "uncategorized";
}

int label_semantic_score(const std::string& label,
                         std::string_view category,
                         std::string_view subcategory)
{
    const std::string sanitized_category =
        Utils::sanitize_path_label(std::string(category));
    const std::string sanitized_subcategory =
        Utils::sanitize_path_label(std::string(subcategory));
    const std::string lowered_label = lower_copy(Utils::sanitize_path_label(label));
    if (lowered_label.empty() || sanitized_category.empty()) {
        return 0;
    }

    int score = 0;
    const std::string lowered_category = lower_copy(sanitized_category);
    if (lowered_label == lowered_category) {
        score += 18;
    } else if (lowered_label.find(lowered_category) != std::string::npos ||
               lowered_category.find(lowered_label) != std::string::npos) {
        score += 8;
    }

    if (!is_general_subcategory(sanitized_subcategory)) {
        const std::string lowered_subcategory = lower_copy(sanitized_subcategory);
        if (lowered_label == lowered_subcategory) {
            score += 10;
        } else if (lowered_label.find(lowered_subcategory) != std::string::npos ||
                   lowered_subcategory.find(lowered_label) != std::string::npos) {
            score += 4;
        }
    }

    auto label_tokens = tokenize(label);
    auto category_tokens = tokenize(sanitized_category);
    auto subcategory_tokens = tokenize(sanitized_subcategory);
    add_aliases(category_tokens);
    add_aliases(subcategory_tokens);

    for (const auto& token : category_tokens) {
        if (label_tokens.contains(token)) {
            score += 10;
        }
    }
    for (const auto& token : subcategory_tokens) {
        if (label_tokens.contains(token)) {
            score += 3;
        }
    }
    return score;
}

std::vector<PathInfo> direct_children_of(const std::vector<PathInfo>& infos,
                                         const std::string& parent_path)
{
    std::vector<PathInfo> children;
    const auto parent_segments = split_path(parent_path);
    const std::size_t expected_depth = parent_segments.size() + 1;
    const std::string prefix = parent_path.empty() ? std::string() : parent_path + "/";
    for (const auto& info : infos) {
        if (info.segments.size() != expected_depth) {
            continue;
        }
        if (!prefix.empty() && info.relative_path.rfind(prefix, 0) != 0) {
            continue;
        }
        children.push_back(info);
    }
    return children;
}

std::vector<PathInfo> top_level_infos(const std::vector<PathInfo>& infos)
{
    std::vector<PathInfo> top_level;
    for (const auto& info : infos) {
        if (info.segments.size() == 1) {
            top_level.push_back(info);
        }
    }
    return top_level;
}

std::optional<ParentCandidate> best_parent_candidate(
    const std::vector<PathInfo>& infos,
    std::string_view category,
    std::string_view subcategory)
{
    std::optional<ParentCandidate> best;
    for (const auto& info : top_level_infos(infos)) {
        const int score = label_semantic_score(info.label_path, category, subcategory);
        if (!best || score > best->score ||
            (score == best->score && info.relative_path < best->info.relative_path)) {
            best = ParentCandidate{info, score};
        }
    }
    if (!best || best->score < kStrongParentScore) {
        return std::nullopt;
    }
    return best;
}

std::string format_number(int value, int width)
{
    std::ostringstream out;
    if (width > 1) {
        out << std::setw(width) << std::setfill('0');
    }
    out << value;
    return out.str();
}

struct NumberAllocation {
    int value{-1};
    int width{0};
};

std::optional<NumberAllocation> next_numbered_prefix(
    const std::vector<PathInfo>& siblings,
    std::optional<std::pair<int, int>> range)
{
    std::set<int> used;
    int width = 0;
    int numeric_count = 0;
    int min_number = range ? range->first : 0;
    int max_number = 0;
    for (const auto& sibling : siblings) {
        if (sibling.patterns.empty() ||
            sibling.patterns.back().kind != PrefixKind::Number) {
            continue;
        }
        const auto& pattern = sibling.patterns.back();
        used.insert(pattern.number);
        width = std::max(width, pattern.width);
        max_number = std::max(max_number, pattern.number);
        if (!range) {
            min_number = min_number == 0
                             ? pattern.number
                             : std::min(min_number, pattern.number);
        }
        ++numeric_count;
    }

    if (range) {
        width = std::max(width, static_cast<int>(std::to_string(range->first).size()));
        const int start = range->first + (range->second > range->first ? 1 : 0);
        for (int value = start; value <= range->second; ++value) {
            if (!used.contains(value)) {
                return NumberAllocation{value, width};
            }
        }
        return std::nullopt;
    }

    if (numeric_count < 2 || numeric_count * 2 < static_cast<int>(siblings.size())) {
        return std::nullopt;
    }
    if (min_number == 0) {
        min_number = 1;
    }
    for (int value = min_number; value <= max_number + 1; ++value) {
        if (!used.contains(value)) {
            return NumberAllocation{value, width};
        }
    }
    return std::nullopt;
}

std::optional<std::string> next_alpha_prefix(const std::vector<PathInfo>& siblings)
{
    std::vector<std::string> prefixes;
    for (const auto& sibling : siblings) {
        if (sibling.patterns.empty() ||
            sibling.patterns.back().kind != PrefixKind::Alpha) {
            continue;
        }
        prefixes.push_back(sibling.patterns.back().prefix);
    }
    if (prefixes.size() < 2 || prefixes.size() * 2 < siblings.size()) {
        return std::nullopt;
    }
    const std::size_t width = prefixes.front().size();
    if (!std::all_of(prefixes.begin(), prefixes.end(), [width](const std::string& prefix) {
            return prefix.size() == width;
        })) {
        return std::nullopt;
    }
    std::sort(prefixes.begin(), prefixes.end());
    std::string next = prefixes.back();
    for (auto it = next.rbegin(); it != next.rend(); ++it) {
        if (*it < 'Z') {
            ++(*it);
            return next;
        }
        *it = 'A';
    }
    return std::nullopt;
}

std::string make_child_segment(const PathInfo& parent,
                               const std::vector<PathInfo>& infos,
                               const std::string& child_label)
{
    const auto children = direct_children_of(infos, parent.relative_path);
    if (!parent.patterns.empty() && parent.patterns.back().kind == PrefixKind::Range) {
        const auto& pattern = parent.patterns.back();
        if (auto number = next_numbered_prefix(children, std::pair{pattern.range_start, pattern.range_end})) {
            return format_number(number->value, number->width) + " " + child_label;
        }
    }
    if (auto number = next_numbered_prefix(children, std::nullopt)) {
        return format_number(number->value, number->width) + " " + child_label;
    }
    if (auto alpha = next_alpha_prefix(children)) {
        return *alpha + " " + child_label;
    }
    return child_label;
}

struct TopRangePattern {
    int next_start{-1};
    int span{9};
    int width{2};
};

std::optional<TopRangePattern> infer_top_range_pattern(const std::vector<PathInfo>& top_level)
{
    std::vector<SegmentPattern> ranges;
    for (const auto& info : top_level) {
        if (!info.patterns.empty() && info.patterns.front().kind == PrefixKind::Range) {
            ranges.push_back(info.patterns.front());
        }
    }
    if (ranges.size() < 2 || ranges.size() * 2 < top_level.size()) {
        return std::nullopt;
    }

    int max_end = ranges.front().range_end;
    int span = ranges.front().range_end - ranges.front().range_start;
    int width = ranges.front().width;
    for (const auto& range : ranges) {
        max_end = std::max(max_end, range.range_end);
        width = std::max(width, range.width);
    }
    return TopRangePattern{max_end + 1, span, width};
}

std::string make_next_range_top_segment(const TopRangePattern& pattern,
                                        const std::string& category_label)
{
    const int end = pattern.next_start + pattern.span;
    return format_number(pattern.next_start, pattern.width) + "-" +
           format_number(end, pattern.width) + " " + category_label;
}

std::optional<std::string> make_top_level_numbered_path(
    const std::vector<PathInfo>& top_level,
    const std::string& category_label,
    const std::string& child_label)
{
    if (auto number = next_numbered_prefix(top_level, std::nullopt)) {
        std::string path = format_number(number->value, number->width) + " " + category_label;
        if (!child_label.empty()) {
            path += "/" + child_label;
        }
        return path;
    }
    return std::nullopt;
}

std::optional<std::string> make_top_level_alpha_path(
    const std::vector<PathInfo>& top_level,
    const std::string& category_label,
    const std::string& child_label)
{
    if (auto alpha = next_alpha_prefix(top_level)) {
        std::string path = *alpha + " " + category_label;
        if (!child_label.empty()) {
            path += "/" + child_label;
        }
        return path;
    }
    return std::nullopt;
}

bool validate_suggested_path(const std::string& path,
                             const FolderTreeCatalog::Catalog& catalog)
{
    const auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
    return validation.valid && !catalog.find_existing(validation.normalized_path).has_value();
}

bool contains_token(const std::vector<std::string>& values, std::string_view expected)
{
    const std::string lowered_expected = lower_copy(std::string(expected));
    return std::any_of(values.begin(), values.end(), [&](const std::string& value) {
        return lower_copy(value) == lowered_expected;
    });
}

bool profile_matches_tree(const FolderStructurePluginProfile& profile,
                          const FolderStructurePattern::Profile& inferred)
{
    const std::string kind = lower_copy(profile.structure_kind);
    if ((kind == "johnny_decimal" || kind == "johnny.decimal" ||
         contains_token(profile.detectors, "johnny_decimal_like")) &&
        inferred.has_johnny_decimal_like_ranges) {
        return true;
    }
    if ((kind == "numbered_prefix" || contains_token(profile.detectors, "numbered_prefix")) &&
        inferred.has_numbered_prefixes) {
        return true;
    }
    if ((kind == "alphabetic_prefix" || contains_token(profile.detectors, "alphabetic_prefix")) &&
        inferred.has_alphabetic_prefixes) {
        return true;
    }
    return false;
}

void append_plugin_profile_guidance(FolderStructurePattern::Profile& inferred,
                                    std::span<const FolderStructurePluginProfile> plugin_profiles)
{
    std::ostringstream prompt;
    bool appended_header = false;
    for (const auto& plugin_profile : plugin_profiles) {
        if (!profile_matches_tree(plugin_profile, inferred)) {
            continue;
        }
        inferred.has_recognized_conventions = true;
        inferred.matched_plugin_profile_ids.push_back(plugin_profile.id);
        if (!appended_header) {
            prompt << "\nInstalled folder-structure plugin guidance:\n";
            appended_header = true;
        }
        prompt << "- Matched profile: " << plugin_profile.name << ".\n";
        if (!plugin_profile.prompt_guidance.empty()) {
            prompt << plugin_profile.prompt_guidance;
        }
        if (!plugin_profile.new_folder_guidance.empty()) {
            prompt << plugin_profile.new_folder_guidance;
        }
    }

    if (appended_header) {
        inferred.prompt_guidance += prompt.str();
    }
}

std::vector<std::string> examples_for_kind(const std::vector<PathInfo>& infos,
                                           PrefixKind kind,
                                           std::size_t limit)
{
    std::vector<std::string> examples;
    for (const auto& info : infos) {
        if (examples.size() == limit) {
            break;
        }
        if (!info.patterns.empty() && info.patterns.back().kind == kind) {
            examples.push_back(info.segments.back());
        }
    }
    return examples;
}

std::string join_examples(const std::vector<std::string>& examples)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < examples.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << examples[i];
    }
    return out.str();
}

} // namespace

namespace FolderStructurePattern {

Profile infer_profile(const FolderTreeCatalog::Catalog& catalog,
                      std::span<const FolderStructurePluginProfile> plugin_profiles)
{
    const auto infos = make_path_infos(catalog);
    const auto top_level = top_level_infos(infos);
    const auto range_pattern = infer_top_range_pattern(top_level);

    int range_child_count = 0;
    for (const auto& parent : top_level) {
        if (parent.patterns.empty() || parent.patterns.front().kind != PrefixKind::Range) {
            continue;
        }
        const auto children = direct_children_of(infos, parent.relative_path);
        range_child_count += static_cast<int>(std::count_if(children.begin(),
                                                            children.end(),
                                                            [](const PathInfo& child) {
            return !child.patterns.empty() && child.patterns.back().kind == PrefixKind::Number;
        }));
    }

    const auto top_numeric = next_numbered_prefix(top_level, std::nullopt);
    const auto top_alpha = next_alpha_prefix(top_level);

    Profile profile;
    profile.has_johnny_decimal_like_ranges = range_pattern.has_value() && range_child_count >= 2;
    profile.has_numbered_prefixes = top_numeric.has_value();
    profile.has_alphabetic_prefixes = top_alpha.has_value();
    profile.has_recognized_conventions =
        profile.has_johnny_decimal_like_ranges ||
        profile.has_numbered_prefixes ||
        profile.has_alphabetic_prefixes;

    if (!profile.has_recognized_conventions) {
        append_plugin_profile_guidance(profile, plugin_profiles);
        return profile;
    }

    std::ostringstream prompt;
    prompt << "\nDetected folder structure conventions:\n";
    if (profile.has_johnny_decimal_like_ranges) {
        prompt << "- Top-level folders look Johnny.Decimal-like: numbered ranges followed by labels";
        const auto examples = examples_for_kind(top_level, PrefixKind::Range, 3);
        if (!examples.empty()) {
            prompt << " (examples: " << join_examples(examples) << ")";
        }
        prompt << ". Child folders commonly use numbers inside the parent range. Preserve those range and number prefixes when creating a missing folder.\n";
    } else if (profile.has_numbered_prefixes) {
        prompt << "- Sibling folders commonly use leading numeric prefixes before human labels";
        const auto examples = examples_for_kind(top_level, PrefixKind::Number, 3);
        if (!examples.empty()) {
            prompt << " (examples: " << join_examples(examples) << ")";
        }
        prompt << ". Continue that prefix style when suggesting a new peer folder.\n";
    }

    if (profile.has_alphabetic_prefixes) {
        prompt << "- Some sibling folders use leading alphabetic code prefixes before human labels";
        const auto examples = examples_for_kind(top_level, PrefixKind::Alpha, 3);
        if (!examples.empty()) {
            prompt << " (examples: " << join_examples(examples) << ")";
        }
        prompt << ". Treat those codes as literal folder-name prefixes and preserve the style when the pattern is clear.\n";
    }
    prompt << "- Match content against the human label after any leading code/range prefix, but return the full literal folder path including prefixes.\n";
    profile.prompt_guidance = prompt.str();
    append_plugin_profile_guidance(profile, plugin_profiles);
    return profile;
}

std::optional<SuggestedPath> suggest_new_folder(
    const FolderTreeCatalog::Catalog& catalog,
    std::string_view semantic_category,
    std::string_view semantic_subcategory,
    std::span<const FolderStructurePluginProfile> plugin_profiles)
{
    const std::string category_label =
        Utils::sanitize_path_label(std::string(semantic_category));
    const std::string subcategory_label =
        Utils::sanitize_path_label(std::string(semantic_subcategory));
    if (category_label.empty()) {
        return std::nullopt;
    }

    const std::string child_label =
        is_general_subcategory(subcategory_label) ||
                lower_copy(category_label) == lower_copy(subcategory_label)
            ? std::string()
            : subcategory_label;
    const auto infos = make_path_infos(catalog);
    if (auto parent = best_parent_candidate(infos, category_label, child_label)) {
        if (child_label.empty()) {
            return std::nullopt;
        }
        std::string path = parent->info.relative_path + "/" +
                           make_child_segment(parent->info, infos, child_label);
        const auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
        if (validation.valid && !catalog.find_existing(validation.normalized_path)) {
            return SuggestedPath{validation.normalized_path,
                                 true,
                                 "matched existing parent folder label"};
        }
    }

    const auto top_level = top_level_infos(infos);
    const auto profile = infer_profile(catalog, plugin_profiles);
    if (profile.has_johnny_decimal_like_ranges) {
        const auto range_pattern = infer_top_range_pattern(top_level);
        if (!range_pattern) {
            return std::nullopt;
        }
        const std::string top_segment = make_next_range_top_segment(*range_pattern, category_label);
        std::string path = top_segment;
        if (!child_label.empty()) {
            const int child_number =
                range_pattern->next_start +
                (range_pattern->span > 0 ? 1 : 0);
            path += "/" + format_number(child_number, range_pattern->width) + " " + child_label;
        }
        const auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
        if (validation.valid && !catalog.find_existing(validation.normalized_path)) {
            return SuggestedPath{validation.normalized_path,
                                 true,
                                 "continued top-level numbered range convention"};
        }
    }

    if (profile.has_numbered_prefixes) {
        if (auto path = make_top_level_numbered_path(top_level, category_label, child_label);
            path && validate_suggested_path(*path, catalog)) {
            return SuggestedPath{*path, true, "continued top-level numeric prefix convention"};
        }
    }

    if (profile.has_alphabetic_prefixes) {
        if (auto path = make_top_level_alpha_path(top_level, category_label, child_label);
            path && validate_suggested_path(*path, catalog)) {
            return SuggestedPath{*path, false, "continued top-level alphabetic prefix convention"};
        }
    }

    return std::nullopt;
}

std::string human_label_path(std::string_view relative_path)
{
    const auto validation = FolderTreeCatalog::validate_relative_folder_path(relative_path);
    const auto raw_segments = split_path(validation.valid
                                             ? validation.normalized_path
                                             : std::string(relative_path));
    std::vector<std::string> labels;
    labels.reserve(raw_segments.size());
    for (const auto& segment : raw_segments) {
        const auto parsed = parse_segment(segment);
        labels.push_back(parsed.label.empty() ? parsed.original : parsed.label);
    }
    return join_path(labels);
}

} // namespace FolderStructurePattern
