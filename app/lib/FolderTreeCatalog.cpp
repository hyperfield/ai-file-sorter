#include "FolderTreeCatalog.hpp"

#include "ReviewFileNaming.hpp"
#include "Utils.hpp"

#include <json/json.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace {

constexpr std::size_t kPromptCandidateLimit = 160;
constexpr std::size_t kMaxRelativePathLength = 240;
constexpr std::size_t kMaxSegmentLength = 100;

std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string unquote(std::string value)
{
    value = trim_copy(std::move(value));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return trim_copy(std::move(value));
}

std::string strip_code_fence(std::string value)
{
    value = trim_copy(std::move(value));
    if (value.rfind("```", 0) != 0) {
        return value;
    }

    const auto first_newline = value.find('\n');
    if (first_newline == std::string::npos) {
        return value;
    }

    const auto last_fence = value.rfind("\n```");
    if (last_fence == std::string::npos || last_fence <= first_newline) {
        return value;
    }

    return trim_copy(value.substr(first_newline + 1, last_fence - first_newline - 1));
}

std::string normalize_separators(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    while (value.find("//") != std::string::npos) {
        value.replace(value.find("//"), 2, "/");
    }
    while (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool is_reserved_windows_name(const std::string& value)
{
    static const std::vector<std::string> reserved = {
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
    const std::string lowered = lower_copy(value);
    const std::string stem = lowered.substr(0, lowered.find('.'));
    return std::find(reserved.begin(), reserved.end(), stem) != reserved.end();
}

bool is_hidden_directory(const std::filesystem::directory_entry& entry)
{
    const auto name = Utils::path_to_utf8(entry.path().filename());
    return !name.empty() && name.front() == '.';
}

int path_depth(const std::filesystem::path& relative)
{
    return static_cast<int>(std::distance(relative.begin(), relative.end()));
}

std::string relative_path_text(const std::filesystem::path& root,
                               const std::filesystem::path& path)
{
    std::error_code ec;
    const auto relative = std::filesystem::relative(path, root, ec);
    if (ec || relative.empty() || relative == ".") {
        return {};
    }
    const std::u8string generic = relative.generic_u8string();
    std::string text(reinterpret_cast<const char*>(generic.data()), generic.size());
    return normalize_separators(text);
}

std::vector<std::string> split_path(std::string_view value)
{
    std::vector<std::string> segments;
    std::string current;
    for (char ch : value) {
        if (ch == '/') {
            segments.push_back(std::move(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    segments.push_back(std::move(current));
    return segments;
}

bool contains_allowed_segment_chars(const std::string& value)
{
    static const std::string forbidden = R"(<>:"|?*)";
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
    }
    return true;
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

int candidate_score(const FolderTreeCatalog::Entry& entry,
                    const std::unordered_set<std::string>& item_tokens)
{
    if (item_tokens.empty()) {
        return -entry.depth;
    }

    int score = 0;
    const auto path_tokens = tokenize(entry.relative_path);
    for (const auto& token : path_tokens) {
        if (item_tokens.contains(token)) {
            score += 10;
            continue;
        }
        for (const auto& item : item_tokens) {
            if (item.size() >= 4 && token.find(item) != std::string::npos) {
                score += 3;
                break;
            }
            if (token.size() >= 4 && item.find(token) != std::string::npos) {
                score += 3;
                break;
            }
        }
    }
    return score - entry.depth;
}

std::string json_string_value(const Json::Value& object,
                              std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        if (object.isMember(key) && object[key].isString()) {
            return object[key].asString();
        }
    }
    return {};
}

bool json_bool_value(const Json::Value& object,
                     std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        if (object.isMember(key) && object[key].isBool()) {
            return object[key].asBool();
        }
    }
    return false;
}

bool is_key_separator_gap(std::string_view gap)
{
    return std::all_of(gap.begin(), gap.end(), [](unsigned char ch) {
        return std::isspace(ch) || ch == '"' || ch == '\'';
    });
}

bool is_identifier_char(char ch)
{
    const auto value = static_cast<unsigned char>(ch);
    return std::isalnum(value) || ch == '_';
}

bool has_unquoted_key_boundaries(const std::string& value,
                                 std::size_t key_pos,
                                 std::size_t key_size)
{
    const bool has_left_boundary = key_pos == 0 || !is_identifier_char(value[key_pos - 1]);
    const std::size_t after_key = key_pos + key_size;
    const bool has_right_boundary = after_key >= value.size() ||
                                    !is_identifier_char(value[after_key]);
    return has_left_boundary && has_right_boundary;
}

std::optional<std::string> extract_json_object_text(const std::string& response)
{
    const auto open = response.find('{');
    const auto close = response.rfind('}');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return std::nullopt;
    }
    return response.substr(open, close - open + 1);
}

std::optional<FolderTreeCatalog::Selection>
build_selection_from_path(std::string path,
                          const FolderTreeCatalog::Catalog& catalog,
                          bool allow_new_folders,
                          bool model_requested_new);

std::optional<std::string> extract_json_like_string_value(
    const std::string& response,
    std::initializer_list<const char*> keys)
{
    const std::string lowered = lower_copy(response);
    for (const char* raw_key : keys) {
        const std::string key = lower_copy(raw_key);
        for (const std::string& needle : {std::string("\"") + key + "\"", key}) {
            std::size_t search_from = 0;
            while (true) {
                const auto key_pos = lowered.find(needle, search_from);
                if (key_pos == std::string::npos) {
                    break;
                }

                const auto after_key = key_pos + needle.size();
                if (needle == key &&
                    !has_unquoted_key_boundaries(lowered, key_pos, needle.size())) {
                    search_from = after_key;
                    continue;
                }
                const auto colon = lowered.find(':', after_key);
                if (colon == std::string::npos) {
                    break;
                }
                if (!is_key_separator_gap(std::string_view(lowered).substr(after_key,
                                                                           colon - after_key))) {
                    search_from = after_key;
                    continue;
                }

                auto value_start = response.find_first_not_of(" \t\r\n", colon + 1);
                if (value_start == std::string::npos) {
                    return std::nullopt;
                }
                if (response[value_start] == '"' || response[value_start] == '\'') {
                    const char quote = response[value_start];
                    const auto value_end = response.find(quote, value_start + 1);
                    if (value_end == std::string::npos || value_end <= value_start + 1) {
                        return std::nullopt;
                    }
                    return trim_copy(response.substr(value_start + 1,
                                                     value_end - value_start - 1));
                }

                const auto value_end = response.find_first_of(",}\r\n", value_start);
                return trim_copy(response.substr(
                    value_start,
                    value_end == std::string::npos ? std::string::npos : value_end - value_start));
            }
        }
    }
    return std::nullopt;
}

std::optional<FolderTreeCatalog::Selection> parse_json_selection(
    const std::string& response,
    const FolderTreeCatalog::Catalog& catalog,
    bool allow_new_folders)
{
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream stream(response);
    if (!Json::parseFromStream(reader, stream, &root, &errors) || !root.isObject()) {
        return std::nullopt;
    }

    const std::string target =
        json_string_value(root, {"targetFolder", "target_folder", "folder", "path", "destination"});
    const bool create =
        json_bool_value(root, {"createFolder", "create_folder", "suggestNewFolder", "newFolder"});
    if (target.empty()) {
        return std::nullopt;
    }
    return build_selection_from_path(target, catalog, allow_new_folders, create);
}

std::string strip_json_property_suffix(std::string value)
{
    value = trim_copy(std::move(value));
    const auto quote = value.find('"');
    if (quote != std::string::npos && quote > 0) {
        const std::string suffix = lower_copy(value.substr(quote));
        if (suffix.find("createfolder") != std::string::npos ||
            suffix.find("create_folder") != std::string::npos ||
            suffix.find("suggestnewfolder") != std::string::npos ||
            suffix.find("newfolder") != std::string::npos) {
            return trim_copy(value.substr(0, quote));
        }
    }

    const auto comma = value.find(',');
    if (comma != std::string::npos && comma > 0) {
        const std::string suffix = lower_copy(value.substr(comma));
        if (suffix.find("create") != std::string::npos ||
            suffix.find("newfolder") != std::string::npos) {
            return trim_copy(value.substr(0, comma));
        }
    }
    return value;
}

std::string strip_label_prefix(std::string value)
{
    const std::string lower = lower_copy(value);
    for (const std::string& label : {
             "targetfolder:", "target folder:", "folder:", "destination:", "path:"}) {
        if (lower.starts_with(label)) {
            return trim_copy(value.substr(label.size()));
        }
    }
    return value;
}

std::string extract_text_selection(const std::string& response)
{
    std::istringstream stream(response);
    std::string line;
    while (std::getline(stream, line)) {
        line = strip_json_property_suffix(unquote(strip_label_prefix(trim_copy(std::move(line)))));
        if (!line.empty()) {
            return line;
        }
    }
    return strip_json_property_suffix(unquote(response));
}

std::optional<FolderTreeCatalog::Selection>
build_selection_from_path(std::string path,
                          const FolderTreeCatalog::Catalog& catalog,
                          bool allow_new_folders,
                          bool model_requested_new)
{
    auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
    if (!validation.valid) {
        return std::nullopt;
    }

    if (auto existing = catalog.find_existing(validation.normalized_path)) {
        return FolderTreeCatalog::Selection{*existing, false, true};
    }
    if (!allow_new_folders) {
        return std::nullopt;
    }
    (void)model_requested_new;
    return FolderTreeCatalog::Selection{validation.normalized_path, true, false};
}

} // namespace

namespace FolderTreeCatalog {

Catalog Catalog::scan(const std::filesystem::path& root,
                      int max_depth,
                      std::size_t max_entries)
{
    Catalog catalog;
    std::error_code ec;
    if (root.empty() || !std::filesystem::is_directory(root, ec) || ec) {
        return catalog;
    }

    std::filesystem::recursive_directory_iterator it(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        ec);
    const std::filesystem::recursive_directory_iterator end;
    while (!ec && it != end && catalog.entries_.size() < max_entries) {
        const auto current = *it;
        if (!current.is_directory(ec) || ec || is_hidden_directory(current)) {
            if (current.is_directory(ec)) {
                it.disable_recursion_pending();
            }
            it.increment(ec);
            continue;
        }

        const int depth = it.depth() + 1;
        if (depth > max_depth) {
            it.disable_recursion_pending();
            it.increment(ec);
            continue;
        }

        std::string relative = relative_path_text(root, current.path());
        auto validation = validate_relative_folder_path(relative);
        if (validation.valid) {
            catalog.entries_.push_back(Entry{validation.normalized_path, depth});
        }
        it.increment(ec);
    }

    std::sort(catalog.entries_.begin(), catalog.entries_.end(), [](const Entry& left, const Entry& right) {
        return left.relative_path < right.relative_path;
    });
    return catalog;
}

Catalog::Catalog(std::vector<Entry> entries)
    : entries_(std::move(entries))
{
    std::sort(entries_.begin(), entries_.end(), [](const Entry& left, const Entry& right) {
        return left.relative_path < right.relative_path;
    });
}

std::optional<std::string> Catalog::find_existing(std::string_view relative_path) const
{
    const auto validation = validate_relative_folder_path(relative_path);
    if (!validation.valid) {
        return std::nullopt;
    }
    const std::string normalized = lower_copy(validation.normalized_path);
    for (const Entry& entry : entries_) {
        if (lower_copy(entry.relative_path) == normalized) {
            return entry.relative_path;
        }
    }
    return std::nullopt;
}

std::vector<Entry> Catalog::ranked_candidates(const std::string& item_name,
                                              const std::string& item_path,
                                              std::size_t max_candidates) const
{
    std::vector<Entry> ranked = entries_;
    const auto item_tokens = tokenize(item_name + " " + item_path);
    std::stable_sort(ranked.begin(), ranked.end(), [&](const Entry& left, const Entry& right) {
        const int left_score = candidate_score(left, item_tokens);
        const int right_score = candidate_score(right, item_tokens);
        if (left_score != right_score) {
            return left_score > right_score;
        }
        if (left.depth != right.depth) {
            return left.depth > right.depth;
        }
        return left.relative_path < right.relative_path;
    });
    if (ranked.size() > max_candidates) {
        ranked.resize(max_candidates);
    }
    return ranked;
}

ValidationResult validate_relative_folder_path(std::string_view relative_path)
{
    std::string normalized = normalize_separators(unquote(std::string(relative_path)));
    if (normalized.empty()) {
        return {false, {}, "Target folder is empty."};
    }
    if (normalized.size() > kMaxRelativePathLength) {
        return {false, {}, "Target folder path is too long."};
    }

    const auto fs_path = Utils::utf8_to_path(normalized);
    if (fs_path.is_absolute() || fs_path.has_root_name() || fs_path.has_root_directory()) {
        return {false, {}, "Target folder must be relative."};
    }

    const auto segments = split_path(normalized);
    for (const std::string& segment : segments) {
        if (segment.empty() || segment == "." || segment == "..") {
            return {false, {}, "Target folder contains an invalid path segment."};
        }
        if (segment.size() > kMaxSegmentLength) {
            return {false, {}, "Target folder segment is too long."};
        }
        if (!contains_allowed_segment_chars(segment)) {
            return {false, {}, "Target folder contains disallowed characters."};
        }
        const unsigned char first = static_cast<unsigned char>(segment.front());
        const unsigned char last = static_cast<unsigned char>(segment.back());
        if (std::isspace(first) || std::isspace(last) || segment.front() == '.' || segment.back() == '.') {
            return {false, {}, "Target folder segment has leading/trailing space or dot."};
        }
        if (is_reserved_windows_name(segment)) {
            return {false, {}, "Target folder segment is a reserved name."};
        }
    }

    return {true, normalized, {}};
}

std::optional<Selection> parse_selection(const std::string& response,
                                         const Catalog& catalog,
                                         bool allow_new_folders)
{
    const std::string cleaned = strip_code_fence(response);
    if (auto selection = parse_json_selection(cleaned, catalog, allow_new_folders)) {
        return selection;
    }
    if (const auto json_object = extract_json_object_text(cleaned)) {
        if (auto selection = parse_json_selection(*json_object, catalog, allow_new_folders)) {
            return selection;
        }
    }
    if (const auto target =
            extract_json_like_string_value(cleaned,
                                           {"targetFolder", "target_folder", "folder", "path", "destination"})) {
        return build_selection_from_path(*target, catalog, allow_new_folders, false);
    }

    return build_selection_from_path(extract_text_selection(cleaned), catalog, allow_new_folders, false);
}

std::string build_prompt_context(const Catalog& catalog,
                                 const std::string& item_name,
                                 const std::string& item_path,
                                 bool allow_new_folders)
{
    std::ostringstream prompt;
    prompt << kPromptMarker << "\n";
    prompt << "Existing folder structure sorting mode:\n";
    prompt << "- Choose the destination folder for this item under the selected sorting root.\n";
    prompt << "- Return only JSON in this exact shape: {\"targetFolder\":\"relative/folder/path\",\"createFolder\":false}\n";
    prompt << "- Use forward slashes in targetFolder and do not include the file name.\n";
    prompt << "- Folder names are literal. Do not translate, rename, or simplify existing folder names.\n";
    if (allow_new_folders) {
        prompt << "- Prefer an existing folder. Set createFolder to true only when no existing folder fits well.\n";
        prompt << "- When suggesting a new folder, use a concise safe relative folder path under the sorting root.\n";
    } else {
        prompt << "- Use an existing folder only. targetFolder must exactly match one of the listed candidates.\n";
        prompt << "- Never suggest or invent a new folder.\n";
    }

    const std::vector<Entry> candidates =
        catalog.ranked_candidates(item_name, item_path, kPromptCandidateLimit);
    prompt << "\nExisting destination folder candidates:\n";
    if (candidates.empty()) {
        prompt << "(none)\n";
    } else {
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            prompt << std::setw(3) << (index + 1) << ". " << candidates[index].relative_path << "\n";
        }
    }
    if (catalog.entries().size() > candidates.size()) {
        prompt << "(Only the most relevant " << candidates.size()
               << " of " << catalog.entries().size() << " folders are shown.)\n";
    }
    return prompt.str();
}

std::pair<std::string, std::string> derive_category_pair(std::string_view relative_path)
{
    auto validation = validate_relative_folder_path(relative_path);
    if (!validation.valid) {
        return {"Uncategorized", "General"};
    }

    const auto segments = split_path(validation.normalized_path);
    std::string category = segments.empty() ? std::string("Uncategorized") : segments.front();
    std::string subcategory = segments.size() > 1 ? segments.back() : std::string("General");
    category = Utils::sanitize_path_label(category);
    subcategory = Utils::sanitize_path_label(subcategory);
    if (category.empty()) {
        category = "Uncategorized";
    }
    if (subcategory.empty() || ReviewFileNaming::to_lower_copy_str(category) ==
                                   ReviewFileNaming::to_lower_copy_str(subcategory)) {
        subcategory = "General";
    }
    return {category, subcategory};
}

} // namespace FolderTreeCatalog
