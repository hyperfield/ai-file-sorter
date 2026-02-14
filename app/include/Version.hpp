#ifndef VERSION_HPP
#define VERSION_HPP

#include <initializer_list>
#include <string>
#include <vector>


class Version {
 public:
    explicit Version(std::initializer_list<int> version_digits);
    explicit Version(const std::vector<int>& version_digits);
    bool operator>=(const Version& other) const;
    bool operator<=(const Version& other) const;
    std::string to_string() const;
    bool operator>(const Version &other) const;

 private:
    std::vector<int> digits;
};

#endif
