#include "CategoryDateSuffix.hpp"

#include <cctype>

namespace {

bool is_digit_at(std::string_view value, std::size_t index)
{
    return index < value.size() &&
           std::isdigit(static_cast<unsigned char>(value[index])) != 0;
}

bool has_suffix(std::string_view value, std::string_view suffix)
{
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::size_t suffix_length_for_kind(CategoryDateSuffix::Kind kind)
{
    switch (kind) {
        case CategoryDateSuffix::Kind::Image:
            return 11; // _YYYY-MM-DD
        case CategoryDateSuffix::Kind::Document:
            return 8; // _YYYY-MM
    }
    return 0;
}

bool matches_date_suffix(std::string_view suffix, CategoryDateSuffix::Kind kind)
{
    if (kind == CategoryDateSuffix::Kind::Document) {
        return suffix.size() == 8 &&
               suffix[0] == '_' &&
               is_digit_at(suffix, 1) &&
               is_digit_at(suffix, 2) &&
               is_digit_at(suffix, 3) &&
               is_digit_at(suffix, 4) &&
               suffix[5] == '-' &&
               is_digit_at(suffix, 6) &&
               is_digit_at(suffix, 7);
    }

    return suffix.size() == 11 &&
           suffix[0] == '_' &&
           is_digit_at(suffix, 1) &&
           is_digit_at(suffix, 2) &&
           is_digit_at(suffix, 3) &&
           is_digit_at(suffix, 4) &&
           suffix[5] == '-' &&
           is_digit_at(suffix, 6) &&
           is_digit_at(suffix, 7) &&
           suffix[8] == '-' &&
           is_digit_at(suffix, 9) &&
           is_digit_at(suffix, 10);
}

} // namespace

namespace CategoryDateSuffix {

std::string append_date_suffix(std::string_view category, std::string_view date)
{
    if (category.empty() || date.empty()) {
        return std::string(category);
    }

    const std::string suffix = "_" + std::string(date);
    if (has_suffix(category, suffix)) {
        return std::string(category);
    }

    std::string result(category);
    result += suffix;
    return result;
}

std::string strip_date_suffix(std::string_view category, std::string_view date)
{
    if (category.empty() || date.empty()) {
        return std::string(category);
    }

    const std::string suffix = "_" + std::string(date);
    if (!has_suffix(category, suffix)) {
        return std::string(category);
    }
    return std::string(category.substr(0, category.size() - suffix.size()));
}

std::optional<std::string> strip_generated_suffix(std::string_view category, Kind kind)
{
    const std::size_t suffix_length = suffix_length_for_kind(kind);
    if (category.size() <= suffix_length) {
        return std::nullopt;
    }

    const std::string_view suffix = category.substr(category.size() - suffix_length);
    if (!matches_date_suffix(suffix, kind)) {
        return std::nullopt;
    }

    return std::string(category.substr(0, category.size() - suffix_length));
}

} // namespace CategoryDateSuffix
