#include "Version.hpp"
#include <algorithm>
#include <sstream>
#include <string>


Version::Version(std::initializer_list<int> version_digits)
    : digits(version_digits) {}


Version::Version(const std::vector<int>& version_digits)
    : digits(version_digits) {}


bool Version::operator>=(const Version& other) const {
    for (size_t i = 0; i < std::max(digits.size(), other.digits.size()); ++i) {
        int lhs = (i < digits.size()) ? digits[i] : 0;
        int rhs = (i < other.digits.size()) ? other.digits[i] : 0;

        if (lhs > rhs) return true;
        if (lhs < rhs) return false;
    }
    return true;
}


bool Version::operator>(const Version& other) const
{
    for (size_t i = 0; i < std::max(digits.size(), other.digits.size()); ++i) {
        int lhs = (i < digits.size()) ? digits[i] : 0;
        int rhs = (i < other.digits.size()) ? other.digits[i] : 0;

        if (lhs > rhs) return true;
        if (lhs < rhs) return false;
    }
    return false;
}


bool Version::operator<=(const Version& other) const
{
    for (size_t i = 0; i < std::max(digits.size(), other.digits.size()); ++i) {
        int lhs = (i < digits.size()) ? digits[i] : 0;
        int rhs = (i < other.digits.size()) ? other.digits[i] : 0;

        if (lhs > rhs) return false;
        if (lhs < rhs) return true;
    }
    return true;
}


std::string Version::to_string() const
{
    if (digits.empty()) return "0";
    std::ostringstream oss;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0) oss << '.';
        oss << digits[i];
    }
    return oss.str();
}