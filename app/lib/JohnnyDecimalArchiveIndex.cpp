#include "JohnnyDecimalArchiveIndex.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "FolderTreeCatalog.hpp"
#include "Utils.hpp"

namespace {

struct ParsedArea {
    bool valid{false};
    int start{-1};
    int end{-1};
    int width{0};
    std::string id;
    std::string label;
};

struct ParsedCategory {
    bool valid{false};
    int number{-1};
    std::string id;
    std::string label;
};

struct DisplayName {
    std::string id;
    std::string label;
};

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
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

std::string basename(std::string_view relative_path) {
    const auto segments = split_path(relative_path);
    return segments.empty() ? std::string() : segments.back();
}

std::size_t consume_digits(const std::string& value, std::size_t pos) {
    while (pos < value.size() && std::isdigit(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    return pos;
}

std::size_t skip_spaces(const std::string& value, std::size_t pos) {
    while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    return pos;
}

std::optional<int> parse_int(std::string_view value) {
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

ParsedArea parse_area(std::string segment) {
    segment = trim_copy(std::move(segment));
    ParsedArea parsed;
    if (segment.empty()) {
        return parsed;
    }

    const std::size_t first_end = consume_digits(segment, 0);
    if (first_end == 0) {
        return parsed;
    }
    const std::size_t hyphen_pos = skip_spaces(segment, first_end);
    if (hyphen_pos >= segment.size() || segment[hyphen_pos] != '-') {
        return parsed;
    }

    const std::size_t second_start = skip_spaces(segment, hyphen_pos + 1);
    const std::size_t second_end = consume_digits(segment, second_start);
    if (second_end == second_start) {
        return parsed;
    }

    const std::size_t label_start = skip_spaces(segment, second_end);
    if (label_start >= segment.size()) {
        return parsed;
    }

    const auto start = parse_int(std::string_view(segment).substr(0, first_end));
    const auto end = parse_int(std::string_view(segment).substr(second_start, second_end - second_start));
    if (!start || !end) {
        return parsed;
    }

    parsed.start = *start;
    parsed.end = *end;
    parsed.width = static_cast<int>(std::max(first_end, second_end - second_start));
    parsed.label = trim_copy(segment.substr(label_start));
    parsed.id = segment.substr(0, second_end);
    parsed.valid = first_end == second_end - second_start &&
                   parsed.width == 2 &&
                   parsed.start >= 0 &&
                   parsed.end <= 99 &&
                   parsed.start % 10 == 0 &&
                   parsed.end == parsed.start + 9 &&
                   !parsed.label.empty();
    return parsed;
}

ParsedCategory parse_category(std::string segment, const ParsedArea& area) {
    segment = trim_copy(std::move(segment));
    ParsedCategory parsed;
    if (segment.empty()) {
        return parsed;
    }

    const std::size_t first_end = consume_digits(segment, 0);
    if (first_end == 0) {
        return parsed;
    }

    if (first_end + 1 < segment.size() &&
        (segment[first_end] == '.' || segment[first_end] == '_' || segment[first_end] == '-') &&
        std::isdigit(static_cast<unsigned char>(segment[first_end + 1]))) {
        return parsed;
    }

    const std::size_t label_start = skip_spaces(segment, first_end);
    if (label_start <= first_end || label_start >= segment.size()) {
        return parsed;
    }

    const auto number = parse_int(std::string_view(segment).substr(0, first_end));
    if (!number) {
        return parsed;
    }

    parsed.number = *number;
    parsed.id = segment.substr(0, first_end);
    parsed.label = trim_copy(segment.substr(label_start));
    parsed.valid = static_cast<int>(first_end) == area.width &&
                   parsed.number >= area.start &&
                   parsed.number <= area.end &&
                   !parsed.label.empty();
    return parsed;
}

DisplayName parse_display_name(std::string segment) {
    segment = trim_copy(std::move(segment));
    DisplayName parsed;
    if (segment.empty()) {
        return parsed;
    }

    const std::size_t first_end = consume_digits(segment, 0);
    if (first_end == 0) {
        parsed.label = segment;
        return parsed;
    }

    std::size_t prefix_end = first_end;
    if (prefix_end + 1 < segment.size() &&
        segment[prefix_end] == '.' &&
        std::isdigit(static_cast<unsigned char>(segment[prefix_end + 1]))) {
        prefix_end = consume_digits(segment, prefix_end + 1);
    }

    const std::size_t label_start = skip_spaces(segment, prefix_end);
    if (label_start <= prefix_end || label_start >= segment.size()) {
        parsed.label = segment;
        return parsed;
    }

    parsed.id = segment.substr(0, prefix_end);
    parsed.label = trim_copy(segment.substr(label_start));
    return parsed;
}

bool is_direct_child_of(const FolderTreeCatalog::Entry& entry, std::string_view parent_path, int parent_depth) {
    if (entry.depth != parent_depth + 1) {
        return false;
    }
    const std::string prefix = std::string(parent_path) + "/";
    return entry.relative_path.rfind(prefix, 0) == 0;
}

JohnnyDecimalArchiveIndex::Item make_item(const FolderTreeCatalog::Catalog& catalog,
                                          const FolderTreeCatalog::Entry& entry) {
    const DisplayName display = parse_display_name(basename(entry.relative_path));
    JohnnyDecimalArchiveIndex::Item item;
    item.id = display.id;
    item.label = display.label;
    item.relative_path = entry.relative_path;
    item.depth = entry.depth;
    for (const auto& child : catalog.entries()) {
        if (is_direct_child_of(child, entry.relative_path, entry.depth)) {
            item.children.push_back(make_item(catalog, child));
        }
    }
    return item;
}

std::vector<JohnnyDecimalArchiveIndex::Item> build_items(const FolderTreeCatalog::Catalog& catalog,
                                                         std::string_view parent_path,
                                                         int parent_depth) {
    std::vector<JohnnyDecimalArchiveIndex::Item> items;
    for (const auto& entry : catalog.entries()) {
        if (is_direct_child_of(entry, parent_path, parent_depth)) {
            items.push_back(make_item(catalog, entry));
        }
    }
    return items;
}

std::size_t count_items_recursive(const std::vector<JohnnyDecimalArchiveIndex::Item>& items) {
    std::size_t count = 0;
    for (const auto& item : items) {
        ++count;
        count += count_items_recursive(item.children);
    }
    return count;
}

std::string path_to_text(const std::filesystem::path& path) {
    return Utils::path_to_utf8(path);
}

void format_item(std::ostringstream& out, const JohnnyDecimalArchiveIndex::Item& item, int indent_level) {
    out << std::string(static_cast<std::size_t>(indent_level * 2), ' ') << "- ";
    if (!item.id.empty()) {
        out << item.id << " ";
    }
    out << (item.label.empty() ? basename(item.relative_path) : item.label) << "\n";
    for (const auto& child : item.children) {
        format_item(out, child, indent_level + 1);
    }
}

}  // namespace

namespace JohnnyDecimalArchiveIndex {

Index build_index(const std::filesystem::path& root, int max_depth, std::size_t max_entries) {
    Index index;
    index.root = root;

    std::error_code ec;
    if (root.empty() || !std::filesystem::is_directory(root, ec) || ec) {
        return index;
    }

    index.scanned = true;
    const auto catalog = FolderTreeCatalog::Catalog::scan(root, max_depth, max_entries);
    index.scan_limit_reached = catalog.entries().size() >= max_entries;

    for (const auto& entry : catalog.entries()) {
        if (entry.depth != 1) {
            continue;
        }
        const ParsedArea parsed = parse_area(entry.relative_path);
        if (!parsed.valid) {
            index.other_top_level.push_back(entry.relative_path);
            continue;
        }

        Area area;
        area.id = parsed.id;
        area.label = parsed.label;
        area.relative_path = entry.relative_path;
        area.depth = entry.depth;

        for (const auto& child : catalog.entries()) {
            if (!is_direct_child_of(child, entry.relative_path, entry.depth)) {
                continue;
            }
            const ParsedCategory category_id = parse_category(basename(child.relative_path), parsed);
            if (!category_id.valid) {
                area.other_direct_children.push_back(child.relative_path);
                continue;
            }

            Category category;
            category.id = category_id.id;
            category.label = category_id.label;
            category.relative_path = child.relative_path;
            category.depth = child.depth;
            category.items = build_items(catalog, child.relative_path, child.depth);
            area.categories.push_back(std::move(category));
        }

        index.areas.push_back(std::move(area));
    }

    return index;
}

std::size_t category_count(const Index& index) {
    std::size_t count = 0;
    for (const auto& area : index.areas) {
        count += area.categories.size();
    }
    return count;
}

std::size_t item_count(const Index& index) {
    std::size_t count = 0;
    for (const auto& area : index.areas) {
        for (const auto& category : area.categories) {
            count += count_items_recursive(category.items);
        }
    }
    return count;
}

std::string format_index(const Index& index) {
    std::ostringstream out;
    out << "Johnny.Decimal archive index\n";
    if (!index.root.empty()) {
        out << "Root: " << path_to_text(index.root) << "\n";
    }
    out << "Areas: " << index.areas.size()
        << " | Categories: " << category_count(index)
        << " | Items: " << item_count(index) << "\n";
    if (index.scan_limit_reached) {
        out << "Note: scan limit reached; the index may be incomplete.\n";
    }
    out << "\n";

    if (!index.scanned) {
        out << "Choose an existing archive folder to build the index.\n";
        return out.str();
    }
    if (index.areas.empty()) {
        out << "No Johnny.Decimal area folders were found.\n";
    }

    for (const auto& area : index.areas) {
        out << area.id << " " << area.label << "\n";
        for (const auto& category : area.categories) {
            out << "  " << category.id << " " << category.label << "\n";
            for (const auto& item : category.items) {
                format_item(out, item, 2);
            }
        }
        if (!area.other_direct_children.empty()) {
            out << "  Other direct folders:\n";
            for (const auto& child : area.other_direct_children) {
                out << "    - " << basename(child) << "\n";
            }
        }
        out << "\n";
    }

    if (!index.other_top_level.empty()) {
        out << "Other top-level folders:\n";
        for (const auto& path : index.other_top_level) {
            out << "- " << basename(path) << "\n";
        }
    }

    return out.str();
}

}  // namespace JohnnyDecimalArchiveIndex
