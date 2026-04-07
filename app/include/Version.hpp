#ifndef VERSION_HPP
#define VERSION_HPP

#include <initializer_list>
#include <string>
#include <vector>


class Version {
 public:
    explicit Version(std::initializer_list<int> version_digits, std::string prerelease_tag = {});
    explicit Version(const std::vector<int>& version_digits, std::string prerelease_tag = {});
    static Version parse(const std::string& version_str);
    bool operator>=(const Version& other) const;
    bool operator<=(const Version& other) const;
    bool operator>(const Version &other) const;
    bool has_prerelease() const;
    const std::string& prerelease_tag() const;
    std::string to_numeric_string() const;
    std::string to_string() const;

 private:
    int compare(const Version& other) const;
    std::vector<int> digits;
    std::string prerelease_tag_;
};

#endif
