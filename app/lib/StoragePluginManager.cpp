#include "StoragePluginManager.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

StoragePluginManager::StoragePluginManager(std::string config_dir)
    : config_dir_(std::move(config_dir))
{
    reload();
}

const std::vector<StoragePluginManifest>& StoragePluginManager::available_plugins() const
{
    return builtin_storage_plugin_manifests();
}

std::optional<StoragePluginManifest> StoragePluginManager::find_plugin(const std::string& plugin_id) const
{
    return find_storage_plugin_manifest(plugin_id);
}

std::optional<StoragePluginManifest> StoragePluginManager::find_plugin_for_provider(
    const std::string& provider_id) const
{
    return find_storage_plugin_manifest_for_provider(provider_id);
}

bool StoragePluginManager::is_installed(const std::string& plugin_id) const
{
    return installed_plugins_.contains(plugin_id);
}

std::vector<std::string> StoragePluginManager::installed_plugin_ids() const
{
    std::vector<std::string> plugin_ids;
    plugin_ids.reserve(installed_plugins_.size());
    for (const auto& [plugin_id, record] : installed_plugins_) {
        (void)record;
        plugin_ids.push_back(plugin_id);
    }
    return plugin_ids;
}

bool StoragePluginManager::install(const std::string& plugin_id, std::string* error)
{
    const auto manifest = find_plugin(plugin_id);
    if (!manifest.has_value()) {
        if (error) {
            *error = "Unknown plugin id.";
        }
        return false;
    }

    installed_plugins_[plugin_id] = InstalledPluginRecord{
        .id = manifest->id,
        .version = manifest->version
    };
    return save(error);
}

bool StoragePluginManager::uninstall(const std::string& plugin_id, std::string* error)
{
    installed_plugins_.erase(plugin_id);
    return save(error);
}

bool StoragePluginManager::reload(std::string* error)
{
    installed_plugins_.clear();

    QFile file(QString::fromStdString(install_state_path()));
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error) {
            *error = "Invalid plugin state file.";
        }
        return false;
    }

    const QJsonArray plugins = doc.object().value("installed_plugins").toArray();
    for (const auto& value : plugins) {
        std::string plugin_id;
        std::string plugin_version;

        if (value.isString()) {
            plugin_id = value.toString().toStdString();
        } else if (value.isObject()) {
            const QJsonObject plugin_object = value.toObject();
            plugin_id = plugin_object.value("id").toString().toStdString();
            plugin_version = plugin_object.value("version").toString().toStdString();
        }

        if (plugin_id.empty()) {
            continue;
        }

        const auto manifest = find_plugin(plugin_id);
        if (!manifest.has_value()) {
            continue;
        }

        if (plugin_version.empty()) {
            plugin_version = manifest->version;
        }

        installed_plugins_[plugin_id] = InstalledPluginRecord{
            .id = plugin_id,
            .version = plugin_version
        };
    }

    return true;
}

std::string StoragePluginManager::install_state_path() const
{
    return config_dir_ + "/plugins/storage_plugins.json";
}

bool StoragePluginManager::save(std::string* error) const
{
    QDir plugin_dir(QString::fromStdString(config_dir_ + "/plugins"));
    if (!plugin_dir.exists() && !plugin_dir.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = "Failed to create plugin directory.";
        }
        return false;
    }

    QJsonArray plugins;
    for (const auto& [plugin_id, record] : installed_plugins_) {
        QJsonObject plugin;
        plugin["id"] = QString::fromStdString(plugin_id);
        plugin["version"] = QString::fromStdString(record.version);
        plugin["installed_at_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        plugins.push_back(plugin);
    }

    QJsonObject root;
    root["version"] = 2;
    root["installed_plugins"] = plugins;

    QFile file(QString::fromStdString(install_state_path()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
