#include "JohnnyDecimalItemAnalysis.hpp"

#include <algorithm>
#include <cctype>
#include <map>

namespace {

constexpr std::size_t kCategoryDigits = 2;
constexpr std::size_t kStandardIdLength = 5;

bool ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

}  // namespace

namespace JohnnyDecimalItemAnalysis {

Name parse_name(std::string_view name) {
    name = trim(name);
    Name result;
    result.numbered = !name.empty() && ascii_digit(name.front());
    const auto end = name.find_first_of(" \t\r\n");
    const auto prefix = name.substr(0, end);
    const bool standard_id = prefix.size() == kStandardIdLength && prefix[kCategoryDigits] == '.' &&
                             ascii_digit(prefix[0]) && ascii_digit(prefix[1]) &&
                             ascii_digit(prefix[3]) && ascii_digit(prefix[4]);
    if (!standard_id) {
        result.label = std::string(name);
        return result;
    }
    result.id = std::string(prefix);
    if (end != std::string_view::npos) {
        result.label = std::string(trim(name.substr(end)));
    }
    result.valid = !result.label.empty();
    return result;
}

std::vector<Item> analyze(const JohnnyDecimalArchiveIndex::Index& index) {
    std::vector<Item> items;
    std::map<std::string, std::size_t> counts;
    for (const auto& area : index.areas) {
        for (const auto& category : area.categories) {
            for (const auto& item : category.items) {
                const auto name_start = item.relative_path.find_last_of('/');
                const auto name = parse_name(std::string_view(item.relative_path).substr(name_start + 1));
                items.push_back({name.id, item.relative_path, category.id, !name.numbered,
                                 name.numbered && !name.valid,
                                 !name.id.empty() && name.id.substr(0, kCategoryDigits) != category.id, false});
                if (!name.id.empty()) {
                    ++counts[name.id];
                }
            }
        }
    }
    for (auto& item : items) {
        item.duplicate_id = !item.id.empty() && counts[item.id] > 1;
    }
    return items;
}

}  // namespace JohnnyDecimalItemAnalysis
