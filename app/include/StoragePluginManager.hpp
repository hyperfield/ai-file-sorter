#pragma once

#include "StoragePluginManifest.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Tracks which optional storage-provider plugins are installed for the app.
 */
class StoragePluginManager {
public:
    struct InstalledPluginRecord {
        std::string id;
        std::string version;
    };

    explicit StoragePluginManager(std::string config_dir);

    const std::vector<StoragePluginManifest>& available_plugins() const;
    std::optional<StoragePluginManifest> find_plugin(const std::string& plugin_id) const;
    std::optional<StoragePluginManifest> find_plugin_for_provider(const std::string& provider_id) const;
    bool is_installed(const std::string& plugin_id) const;
    std::vector<std::string> installed_plugin_ids() const;
    bool install(const std::string& plugin_id, std::string* error = nullptr);
    bool uninstall(const std::string& plugin_id, std::string* error = nullptr);
    bool reload(std::string* error = nullptr);

private:
    std::string install_state_path() const;
    bool save(std::string* error = nullptr) const;

    std::string config_dir_;
    std::unordered_map<std::string, InstalledPluginRecord> installed_plugins_;
};
