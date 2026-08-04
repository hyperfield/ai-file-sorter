#ifndef WHITELIST_STORE_HPP
#define WHITELIST_STORE_HPP

#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

class Settings;

/**
 * @brief Persisted category whitelist definition.
 */
struct WhitelistEntry {
    /** @brief Allowed main category labels. Empty means unrestricted. */
    std::vector<std::string> categories;
    /** @brief Legacy flat subcategory allow-list. Empty means unrestricted. */
    std::vector<std::string> subcategories;
    /**
     * @brief Category-specific allowed subcategories.
     *
     * When this map is empty, `subcategories` is used as a flat legacy list.
     * When a category has a mapped list, only that list is valid for the category.
     */
    std::unordered_map<std::string, std::vector<std::string>> subcategories_by_category;
};

/**
 * @brief Loads, saves, and migrates named category whitelists.
 */
class WhitelistStore {
public:
    /**
     * @brief Constructs the whitelist store for a config directory.
     * @param config_dir Directory containing `whitelists.ini`.
     */
    explicit WhitelistStore(std::string config_dir);

    /**
     * @brief Loads whitelist entries from disk and applies migrations.
     * @return True when loading and required migrations completed.
     */
    bool load();
    /**
     * @brief Saves all whitelist entries to disk.
     * @return True when QSettings reports no write error.
     */
    bool save() const;

    /**
     * @brief Lists saved whitelist names sorted alphabetically.
     * @return Sorted whitelist names.
     */
    std::vector<std::string> list_names() const;
    /**
     * @brief Returns a whitelist entry by name.
     * @param name Entry name to look up.
     * @return Matching entry, or std::nullopt when absent.
     */
    std::optional<WhitelistEntry> get(const std::string& name) const;
    /**
     * @brief Adds or replaces a whitelist entry.
     * @param name Entry name.
     * @param entry Whitelist data to store in memory.
     */
    void set(const std::string& name, WhitelistEntry entry);
    /**
     * @brief Removes a whitelist entry.
     * @param name Entry name to erase.
     */
    void remove(const std::string& name);
    /**
     * @brief Returns whether no entries are currently loaded.
     * @return True when the store is empty.
     */
    bool empty() const { return entries_.empty(); }

    /**
     * @brief Seeds default entries from legacy flat settings when the store is empty.
     * @param cats Legacy allowed main categories.
     * @param subs Legacy allowed subcategories.
     */
    void ensure_default_from_legacy(const std::vector<std::string>& cats,
                                    const std::vector<std::string>& subs);
    /**
     * @brief Loads the store and copies the active entry into application settings.
     * @param settings Settings object to initialize from the selected whitelist.
     */
    void initialize_from_settings(Settings& settings);

    /**
     * @brief Returns the default whitelist name.
     * @return Default whitelist name.
     */
    std::string default_name() const { return default_name_; }

private:
    std::string file_path_;
    std::unordered_map<std::string, WhitelistEntry> entries_;
    std::string default_name_ = "Default";
    int built_in_seed_version_{0};
};

#endif
