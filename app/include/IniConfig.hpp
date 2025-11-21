#ifndef INICONFIG_HPP
#define INICONFIG_HPP

#include <string>
#include <map>
#include <fstream>
#include <sstream>


class IniConfig {
public:
    bool load(const std::string &filename);
    std::string getValue(const std::string &section, const std::string &key, const std::string &default_value = "") const;
    void setValue(const std::string &section, const std::string &key, const std::string &value);
    bool save(const std::string &filename) const;
    bool hasValue(const std::string& section, const std::string& key) const;

private:
    std::map<std::string, std::map<std::string, std::string>> data;
};

#endif
