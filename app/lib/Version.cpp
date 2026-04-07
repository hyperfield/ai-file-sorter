#include "Version.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <string>

namespace {

std::string trim_copy(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string normalized_tag(std::string value)
{
    value = trim_copy(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool is_digit_segment(const std::string& segment)
{
    return !segment.empty()
        && std::all_of(segment.begin(), segment.end(), [](unsigned char ch) {
               return std::isdigit(ch) != 0;
           });
}

} // namespace

Version::Version(std::initializer_list<int> version_digits, std::string prerelease_tag)
    : digits(version_digits),
      prerelease_tag_(trim_copy(std::move(prerelease_tag))) {}


Version::Version(const std::vector<int>& version_digits, std::string prerelease_tag)
    : digits(version_digits),
      prerelease_tag_(trim_copy(std::move(prerelease_tag))) {}


Version Version::parse(const std::string& version_str)
{
    const std::string trimmed = trim_copy(version_str);
    if (trimmed.empty()) {
        return Version{0, 0, 0};
    }

    size_t numeric_end = 0;
    while (numeric_end < trimmed.size()) {
        const unsigned char ch = static_cast<unsigned char>(trimmed[numeric_end]);
        if (std::isdigit(ch) || ch == '.') {
            ++numeric_end;
            continue;
        }
        break;
    }

    if (numeric_end == 0) {
        throw std::runtime_error("Invalid version string: " + version_str);
    }

    const std::string numeric_part = trimmed.substr(0, numeric_end);
    std::string prerelease_tag = trim_copy(trimmed.substr(numeric_end));
    while (!prerelease_tag.empty() && prerelease_tag.front() == '-') {
        prerelease_tag.erase(prerelease_tag.begin());
        prerelease_tag = trim_copy(std::move(prerelease_tag));
    }

    std::vector<int> parsed_digits;
    std::istringstream stream(numeric_part);
    std::string segment;
    while (std::getline(stream, segment, '.')) {
        if (!is_digit_segment(segment)) {
            throw std::runtime_error("Invalid version string: " + version_str);
        }
        parsed_digits.push_back(std::stoi(segment));
    }

    if (parsed_digits.empty()) {
        return Version{0, 0, 0};
    }

    return Version(parsed_digits, prerelease_tag);
}


bool Version::operator>=(const Version& other) const {
    return compare(other) >= 0;
}


bool Version::operator>(const Version& other) const
{
    return compare(other) > 0;
}


bool Version::operator<=(const Version& other) const
{
    return compare(other) <= 0;
}


bool Version::has_prerelease() const
{
    return !prerelease_tag_.empty();
}


const std::string& Version::prerelease_tag() const
{
    return prerelease_tag_;
}


std::string Version::to_numeric_string() const
{
    if (digits.empty()) return "0";
    std::ostringstream oss;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0) oss << '.';
        oss << digits[i];
    }
    return oss.str();
}


std::string Version::to_string() const
{
    std::string value = to_numeric_string();
    if (has_prerelease()) {
        value += " " + prerelease_tag_;
    }
    return value;
}


int Version::compare(const Version& other) const
{
    for (size_t i = 0; i < std::max(digits.size(), other.digits.size()); ++i) {
        int lhs = (i < digits.size()) ? digits[i] : 0;
        int rhs = (i < other.digits.size()) ? other.digits[i] : 0;

        if (lhs > rhs) return 1;
        if (lhs < rhs) return -1;
    }

    const std::string lhs_tag = normalized_tag(prerelease_tag_);
    const std::string rhs_tag = normalized_tag(other.prerelease_tag_);
    if (lhs_tag.empty() && rhs_tag.empty()) {
        return 0;
    }
    if (lhs_tag.empty()) {
        return 1;
    }
    if (rhs_tag.empty()) {
        return -1;
    }
    if (lhs_tag > rhs_tag) {
        return 1;
    }
    if (lhs_tag < rhs_tag) {
        return -1;
    }
    return 0;
}
