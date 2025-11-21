#ifndef WHITELIST_STORE_HPP
#define WHITELIST_STORE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

class Settings;

struct WhitelistEntry {
    std::vector<std::string> categories;
    std::vector<std::string> subcategories;
};

class WhitelistStore {
public:
    explicit WhitelistStore(std::string config_dir);

    bool load();
    bool save() const;

    std::vector<std::string> list_names() const;
    std::optional<WhitelistEntry> get(const std::string& name) const;
    void set(const std::string& name, WhitelistEntry entry);
    void remove(const std::string& name);
    bool empty() const { return entries_.empty(); }

    // Migration helper
    void ensure_default_from_legacy(const std::vector<std::string>& cats,
                                    const std::vector<std::string>& subs);
    void initialize_from_settings(Settings& settings);

    std::string default_name() const { return default_name_; }

private:
    std::string file_path_;
    std::unordered_map<std::string, WhitelistEntry> entries_;
    std::string default_name_ = "Default";
};

#endif
