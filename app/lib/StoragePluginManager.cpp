#include "StoragePluginManager.hpp"
#include "StoragePluginArchiveExtractor.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace {

#ifndef AIFS_STORAGE_PLUGIN_CATALOG_URL
#define AIFS_STORAGE_PLUGIN_CATALOG_URL ""
#endif

std::string env_or_default(const char* env_name, const char* fallback)
{
    const QByteArray env_value = qgetenv(env_name);
    if (!env_value.isEmpty()) {
        return env_value.toStdString();
    }
    return fallback ? std::string(fallback) : std::string();
}

std::string remote_catalog_url()
{
    return env_or_default("AI_FILE_SORTER_STORAGE_PLUGIN_CATALOG_URL",
                          AIFS_STORAGE_PLUGIN_CATALOG_URL);
}

std::filesystem::path resolve_plugin_source_path(const StoragePluginManifest& manifest,
                                                 const std::string& path_spec)
{
    const std::filesystem::path path(path_spec);
    if (path.is_absolute()) {
        return path;
    }
    if (!manifest.source_path.empty()) {
        return manifest.source_path.parent_path() / path;
    }

    const QString app_dir = QCoreApplication::applicationDirPath();
    if (!app_dir.isEmpty()) {
        const auto app_relative = std::filesystem::path(app_dir.toStdString()) / path;
        std::error_code ec;
        if (std::filesystem::exists(app_relative, ec) && !ec) {
            return app_relative;
        }
    }
    return path;
}

std::filesystem::path relative_install_target(const StoragePluginManifest& manifest,
                                              const std::string& path_spec)
{
    const std::filesystem::path path(path_spec);
    if (path.is_absolute()) {
        return std::filesystem::path(path.filename());
    }
    return path;
}

bool copy_plugin_asset(const std::filesystem::path& source,
                       const std::filesystem::path& destination,
                       std::string* error)
{
    std::error_code ec;
    if (!std::filesystem::exists(source, ec) || ec) {
        if (error) {
            *error = "Plugin asset is missing: " + source.string();
        }
        return false;
    }

    if (std::filesystem::is_directory(source, ec) && !ec) {
        if (!std::filesystem::create_directories(destination, ec) && ec) {
            if (error) {
                *error = ec.message();
            }
            return false;
        }

        for (std::filesystem::recursive_directory_iterator it(source, ec), end;
             it != end && !ec;
             it.increment(ec)) {
            const auto relative = std::filesystem::relative(it->path(), source, ec);
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return false;
            }

            const auto target = destination / relative;
            if (it->is_directory(ec) && !ec) {
                std::filesystem::create_directories(target, ec);
                if (ec) {
                    if (error) {
                        *error = ec.message();
                    }
                    return false;
                }
                continue;
            }

            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return false;
            }

            std::filesystem::copy_file(
                it->path(),
                target,
                std::filesystem::copy_options::overwrite_existing,
                ec);
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return false;
            }

            const auto source_status = std::filesystem::status(it->path(), ec);
            if (!ec) {
                std::filesystem::permissions(
                    target,
                    source_status.permissions(),
                    std::filesystem::perm_options::replace,
                    ec);
            }
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return false;
            }
        }

        if (ec) {
            if (error) {
                *error = ec.message();
            }
            return false;
        }

        return true;
    }

    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    const auto source_status = std::filesystem::status(source, ec);
    if (!ec) {
        std::filesystem::permissions(
            destination,
            source_status.permissions(),
            std::filesystem::perm_options::replace,
            ec);
    }
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    return true;
}

bool is_relative_path_spec(const std::string& path_spec)
{
    if (path_spec.empty()) {
        return false;
    }
    const std::filesystem::path path(path_spec);
    return !path.is_absolute();
}

std::vector<int> parse_version_digits(const std::string& version)
{
    std::vector<int> digits;
    std::string current;
    for (char ch : version) {
        if (ch == '.') {
            if (current.empty()) {
                return {};
            }
            digits.push_back(std::stoi(current));
            current.clear();
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return {};
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        digits.push_back(std::stoi(current));
    }
    return digits;
}

bool version_is_newer(const std::string& candidate, const std::string& installed)
{
    const auto lhs = parse_version_digits(candidate);
    const auto rhs = parse_version_digits(installed);
    if (lhs.empty() || rhs.empty()) {
        return candidate > installed;
    }

    const auto count = std::max(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < count; ++i) {
        const int left = i < lhs.size() ? lhs[i] : 0;
        const int right = i < rhs.size() ? rhs[i] : 0;
        if (left > right) {
            return true;
        }
        if (left < right) {
            return false;
        }
    }
    return false;
}

std::vector<StoragePluginManifest> merge_manifests(const std::vector<StoragePluginManifest>& base,
                                                   std::vector<StoragePluginManifest> overlay)
{
    std::unordered_map<std::string, std::size_t> index_by_id;
    std::vector<StoragePluginManifest> merged;
    merged.reserve(base.size() + overlay.size());

    for (const auto& manifest : base) {
        index_by_id[manifest.id] = merged.size();
        merged.push_back(manifest);
    }

    for (auto& manifest : overlay) {
        const auto existing = index_by_id.find(manifest.id);
        if (existing != index_by_id.end()) {
            merged[existing->second] = std::move(manifest);
            continue;
        }
        index_by_id[manifest.id] = merged.size();
        merged.push_back(std::move(manifest));
    }

    return merged;
}

} // namespace

StoragePluginManager::StoragePluginManager(std::string config_dir,
                                           StoragePluginPackageFetcher::DownloadFunction download_fn)
    : config_dir_(std::move(config_dir)),
      loader_(manifest_directory_for_config_dir(config_dir_)),
      package_fetcher_(download_directory_for_config_dir(config_dir_), std::move(download_fn)),
      remote_catalog_url_(remote_catalog_url())
{
    load_cached_remote_catalog();
    reload();
}

std::filesystem::path StoragePluginManager::manifest_directory_for_config_dir(const std::string& config_dir)
{
    return std::filesystem::path(config_dir) / "plugins" / "storage" / "manifests";
}

std::filesystem::path StoragePluginManager::catalog_directory_for_config_dir(const std::string& config_dir)
{
    return std::filesystem::path(config_dir) / "plugins" / "storage" / "catalog";
}

std::filesystem::path StoragePluginManager::package_directory_for_config_dir(const std::string& config_dir)
{
    return std::filesystem::path(config_dir) / "plugins" / "storage" / "packages";
}

std::filesystem::path StoragePluginManager::download_directory_for_config_dir(const std::string& config_dir)
{
    return std::filesystem::path(config_dir) / "plugins" / "storage" / "downloads";
}

std::vector<StoragePluginManifest> StoragePluginManager::available_plugins() const
{
    return merged_available_plugins();
}

std::optional<StoragePluginManifest> StoragePluginManager::find_plugin(const std::string& plugin_id) const
{
    if (is_installed(plugin_id)) {
        const auto installed_manifest = loader_.find_plugin(plugin_id);
        if (installed_manifest.has_value()) {
            return installed_manifest;
        }
    }

    for (const auto& manifest : remote_catalog_manifests_) {
        if (manifest.id == plugin_id) {
            return manifest;
        }
    }

    return loader_.find_plugin(plugin_id);
}

std::optional<StoragePluginManifest> StoragePluginManager::find_plugin_for_provider(
    const std::string& provider_id) const
{
    const auto installed_manifest = loader_.find_plugin_for_provider(provider_id);
    if (installed_manifest.has_value() && is_installed(installed_manifest->id)) {
        return installed_manifest;
    }

    for (const auto& manifest : remote_catalog_manifests_) {
        for (const auto& supported_provider_id : manifest.provider_ids) {
            if (supported_provider_id == provider_id) {
                return manifest;
            }
        }
    }

    return loader_.find_plugin_for_provider(provider_id);
}

bool StoragePluginManager::remote_catalog_configured() const
{
    return !remote_catalog_url_.empty();
}

bool StoragePluginManager::can_check_for_updates() const
{
    if (remote_catalog_configured()) {
        return true;
    }

    const auto manifests = merged_available_plugins();
    return std::any_of(manifests.begin(), manifests.end(), [](const StoragePluginManifest& manifest) {
        return manifest.has_remote_manifest();
    });
}

bool StoragePluginManager::refresh_remote_catalog(std::string* error)
{
    if (!can_check_for_updates()) {
        if (error) {
            *error = "No remote plugin update sources are configured.";
        }
        return false;
    }

    std::vector<StoragePluginManifest> manifests;
    std::string first_error;
    bool fetched_any = false;

    const auto upsert_manifest = [&manifests](StoragePluginManifest manifest) {
        const auto existing = std::find_if(manifests.begin(), manifests.end(),
                                           [&](const StoragePluginManifest& candidate) {
                                               return candidate.id == manifest.id;
                                           });
        if (existing != manifests.end()) {
            *existing = std::move(manifest);
            return;
        }
        manifests.push_back(std::move(manifest));
    };

    if (remote_catalog_configured()) {
        std::string catalog_error;
        auto fetched_manifests = package_fetcher_.fetch_catalog_manifests(remote_catalog_url_, &catalog_error);
        if (!fetched_manifests.empty()) {
            manifests = std::move(fetched_manifests);
            fetched_any = true;
        } else if (!catalog_error.empty()) {
            first_error = std::move(catalog_error);
        }
    }

    for (const auto& plugin_id : installed_plugin_ids()) {
        const auto installed_manifest = loader_.find_plugin(plugin_id);
        if (!installed_manifest.has_value() || !installed_manifest->has_remote_manifest()) {
            continue;
        }

        std::string manifest_error;
        auto remote_manifest = package_fetcher_.fetch_remote_manifest(*installed_manifest, &manifest_error);
        if (!remote_manifest.has_value()) {
            if (first_error.empty() && !manifest_error.empty()) {
                first_error = std::move(manifest_error);
            }
            continue;
        }

        if (remote_manifest->remote_manifest_url.empty()) {
            remote_manifest->remote_manifest_url = installed_manifest->remote_manifest_url;
        }
        upsert_manifest(std::move(*remote_manifest));
        fetched_any = true;
    }

    if (!fetched_any) {
        if (error) {
            *error = first_error.empty()
                ? "No plugin update information could be fetched."
                : first_error;
        }
        return false;
    }

    return persist_remote_catalog(std::move(manifests), error);
}

bool StoragePluginManager::supports_plugin(const std::string& plugin_id) const
{
    const auto manifest = find_plugin(plugin_id);
    if (!manifest.has_value()) {
        return false;
    }
    if (manifest->has_remote_manifest() || manifest->has_remote_package()) {
        return true;
    }
    return loader_.supports_plugin(*manifest);
}

bool StoragePluginManager::is_installed(const std::string& plugin_id) const
{
    return installed_plugins_.contains(plugin_id);
}

bool StoragePluginManager::can_update(const std::string& plugin_id) const
{
    const auto installed = installed_plugins_.find(plugin_id);
    if (installed == installed_plugins_.end()) {
        return false;
    }

    const auto manifests = merged_available_plugins();
    const auto plugin = std::find_if(manifests.begin(), manifests.end(), [&](const StoragePluginManifest& manifest) {
        return manifest.id == plugin_id;
    });
    if (plugin == manifests.end()) {
        return false;
    }

    return version_is_newer(plugin->version, installed->second.version);
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
    const auto manifests = merged_available_plugins();
    const auto manifest = std::find_if(manifests.begin(), manifests.end(), [&](const StoragePluginManifest& entry) {
        return entry.id == plugin_id;
    });
    if (manifest == manifests.end()) {
        if (error) {
            *error = "Unknown plugin id.";
        }
        return false;
    }
    const auto resolved_manifest = resolve_install_manifest(*manifest, error);
    if (!resolved_manifest.has_value()) {
        return false;
    }

    if (resolved_manifest->has_remote_package()) {
        return install_from_remote_package(*resolved_manifest, error);
    }
    return install_manifest(*resolved_manifest, error);
}

bool StoragePluginManager::update(const std::string& plugin_id, std::string* error)
{
    if (!is_installed(plugin_id)) {
        if (error) {
            *error = "Plugin is not installed.";
        }
        return false;
    }

    const auto manifests = merged_available_plugins();
    const auto manifest = std::find_if(manifests.begin(), manifests.end(), [&](const StoragePluginManifest& entry) {
        return entry.id == plugin_id;
    });
    if (manifest == manifests.end()) {
        if (error) {
            *error = "Unknown plugin id.";
        }
        return false;
    }

    const auto resolved_manifest = resolve_install_manifest(*manifest, error);
    if (!resolved_manifest.has_value()) {
        return false;
    }

    const auto installed = installed_plugins_.find(plugin_id);
    if (installed != installed_plugins_.end() &&
        !resolved_manifest->has_remote_manifest() &&
        !resolved_manifest->has_remote_package() &&
        !version_is_newer(resolved_manifest->version, installed->second.version)) {
        if (error) {
            *error = "Plugin is already up to date.";
        }
        return false;
    }

    if (resolved_manifest->has_remote_package()) {
        return install_from_remote_package(*resolved_manifest, error);
    }
    return install_manifest(*resolved_manifest, error);
}

bool StoragePluginManager::install_from_archive(const std::filesystem::path& archive_path,
                                                std::string* installed_plugin_id,
                                                std::string* error)
{
    return install_from_archive_internal(archive_path, nullptr, installed_plugin_id, error);
}

bool StoragePluginManager::install_from_archive_internal(const std::filesystem::path& archive_path,
                                                         const StoragePluginManifest* expected_manifest,
                                                         std::string* installed_plugin_id,
                                                         std::string* error)
{
    if (!StoragePluginArchiveExtractor::supports_archive(archive_path)) {
        if (error) {
            *error = "Only .aifsplugin and .zip plugin packages are supported.";
        }
        return false;
    }

    const auto staging_root =
        std::filesystem::path(config_dir_) / "plugins" / "storage" / "staging";
    QDir staging_dir(QString::fromStdString(staging_root.string()));
    if (!staging_dir.exists() && !staging_dir.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = "Failed to create plugin staging directory.";
        }
        return false;
    }

    QTemporaryDir temp_dir(QString::fromStdString((staging_root / "archive-XXXXXX").string()));
    if (!temp_dir.isValid()) {
        if (error) {
            *error = "Failed to create a temporary plugin extraction directory.";
        }
        return false;
    }

    auto extraction =
        StoragePluginArchiveExtractor::extract_archive(archive_path,
                                                       std::filesystem::path(temp_dir.path().toStdString()));
    if (!extraction.ok()) {
        if (error) {
            *error = extraction.message;
        }
        return false;
    }

    std::string manifest_error;
    auto manifest = load_storage_plugin_manifest_from_file(extraction.manifest_path, &manifest_error);
    if (!manifest.has_value()) {
        if (error) {
            *error = manifest_error.empty() ? "Failed to load plugin manifest from archive." : manifest_error;
        }
        return false;
    }

    if (expected_manifest) {
        if (manifest->id != expected_manifest->id) {
            if (error) {
                *error = "The downloaded plugin archive does not match the requested plugin id.";
            }
            return false;
        }
        if (!expected_manifest->version.empty() && manifest->version != expected_manifest->version) {
            if (error) {
                *error = "The downloaded plugin archive version does not match the requested plugin version.";
            }
            return false;
        }
        if (manifest->remote_manifest_url.empty()) {
            manifest->remote_manifest_url = expected_manifest->remote_manifest_url;
        }
        if (manifest->package_download_url.empty()) {
            manifest->package_download_url = expected_manifest->package_download_url;
        }
        if (manifest->package_sha256.empty()) {
            manifest->package_sha256 = expected_manifest->package_sha256;
        }
        if (manifest->platforms.empty()) {
            manifest->platforms = expected_manifest->platforms;
        }
        if (manifest->architectures.empty()) {
            manifest->architectures = expected_manifest->architectures;
        }
    }

    if (manifest->entry_point_kind != "external_process") {
        if (error) {
            *error = "Plugin archives currently support only external_process entry points.";
        }
        return false;
    }

    if (!is_relative_path_spec(manifest->entry_point)) {
        if (error) {
            *error = "Plugin archive entry_point must be a relative path inside the archive.";
        }
        return false;
    }

    for (const auto& package_path : manifest->package_paths) {
        if (!is_relative_path_spec(package_path)) {
            if (error) {
                *error = "Plugin archive package_paths must be relative paths inside the archive.";
            }
            return false;
        }
    }

    if (manifest->entry_point_kind == "external_process") {
        const auto extracted_entry_point = resolve_plugin_source_path(*manifest, manifest->entry_point);
        std::error_code ec;
        if (!std::filesystem::exists(extracted_entry_point, ec) || ec) {
            if (error) {
                *error = "The plugin archive entry point is missing: " + extracted_entry_point.string();
            }
            return false;
        }

#ifndef _WIN32
        std::filesystem::permissions(
            extracted_entry_point,
            std::filesystem::perms::owner_exec |
                std::filesystem::perms::group_exec |
                std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add,
            ec);
        if (ec) {
            if (error) {
                *error = "Failed to mark the extracted plugin entry point executable: " + ec.message();
            }
            return false;
        }
#endif
    }

    if (!install_manifest(*manifest, error)) {
        return false;
    }

    if (installed_plugin_id) {
        *installed_plugin_id = manifest->id;
    }
    return true;
}

bool StoragePluginManager::uninstall(const std::string& plugin_id, std::string* error)
{
    installed_plugins_.erase(plugin_id);
    if (!remove_plugin_artifacts(plugin_id, error)) {
        return false;
    }
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

        std::string manifest_error;
        if (!persist_manifest(*manifest, &manifest_error) && error && error->empty()) {
            *error = manifest_error;
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

std::filesystem::path StoragePluginManager::manifest_path_for_plugin(const std::string& plugin_id) const
{
    return manifest_directory_for_config_dir(config_dir_) / (plugin_id + ".json");
}

std::filesystem::path StoragePluginManager::package_directory_for_plugin(
    const StoragePluginManifest& manifest) const
{
    return package_directory_for_config_dir(config_dir_) / manifest.id / manifest.version;
}

std::vector<StoragePluginManifest> StoragePluginManager::merged_available_plugins() const
{
    return merge_manifests(loader_.available_plugins(), remote_catalog_manifests_);
}

void StoragePluginManager::load_cached_remote_catalog()
{
    std::string error;
    remote_catalog_manifests_ = load_storage_plugin_manifests_from_directory(
        catalog_directory_for_config_dir(config_dir_),
        &error);
    (void)error;
}

bool StoragePluginManager::persist_remote_catalog(std::vector<StoragePluginManifest> manifests,
                                                  std::string* error)
{
    const auto catalog_dir = catalog_directory_for_config_dir(config_dir_);
    std::error_code ec;
    std::filesystem::create_directories(catalog_dir, ec);
    if (ec) {
        if (error) {
            *error = "Failed to create plugin catalog directory: " + ec.message();
        }
        return false;
    }

    std::unordered_set<std::string> retained_ids;
    for (const auto& manifest : manifests) {
        retained_ids.insert(manifest.id);
        if (!save_storage_plugin_manifest_to_file(
                manifest,
                catalog_dir / (manifest.id + ".json"),
                error)) {
            return false;
        }
    }

    for (std::filesystem::directory_iterator it(catalog_dir, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file(ec) || ec) {
            continue;
        }
        if (it->path().extension() != ".json") {
            continue;
        }
        const auto plugin_id = it->path().stem().string();
        if (retained_ids.contains(plugin_id)) {
            continue;
        }
        std::filesystem::remove(it->path(), ec);
        if (ec) {
            if (error) {
                *error = "Failed to update cached plugin catalog: " + ec.message();
            }
            return false;
        }
    }
    if (ec) {
        if (error) {
            *error = "Failed to update cached plugin catalog: " + ec.message();
        }
        return false;
    }

    remote_catalog_manifests_ = std::move(manifests);
    return true;
}

std::optional<StoragePluginManifest> StoragePluginManager::resolve_install_manifest(
    const StoragePluginManifest& manifest,
    std::string* error) const
{
    if (manifest.has_remote_manifest()) {
        auto remote_manifest = package_fetcher_.fetch_remote_manifest(manifest, error);
        if (!remote_manifest.has_value()) {
            return std::nullopt;
        }
        if (remote_manifest->id != manifest.id) {
            if (error) {
                *error = "Remote plugin metadata does not match the selected plugin.";
            }
            return std::nullopt;
        }
        if (remote_manifest->remote_manifest_url.empty()) {
            remote_manifest->remote_manifest_url = manifest.remote_manifest_url;
        }
        return remote_manifest;
    }
    return manifest;
}

bool StoragePluginManager::install_from_remote_package(const StoragePluginManifest& manifest,
                                                       std::string* error)
{
    std::filesystem::path archive_path;
    if (!package_fetcher_.fetch_package_archive(manifest, &archive_path, error)) {
        return false;
    }
    std::string installed_plugin_id;
    return install_from_archive_internal(archive_path, &manifest, &installed_plugin_id, error);
}

bool StoragePluginManager::install_manifest(const StoragePluginManifest& manifest, std::string* error)
{
    std::string compatibility_error;
    if (!storage_plugin_manifest_matches_current_runtime(manifest, &compatibility_error)) {
        if (error) {
            *error = compatibility_error.empty()
                ? "This plugin does not support the current platform."
                : compatibility_error;
        }
        return false;
    }

    std::string support_error;
    if (!loader_.supports_plugin(manifest, &support_error)) {
        if (error) {
            *error = support_error.empty()
                ? "This plugin entry point is not supported by this version of the app."
                : support_error;
        }
        return false;
    }

    StoragePluginManifest installed_manifest = manifest;
    if (!materialize_manifest_for_install(manifest, &installed_manifest, error)) {
        return false;
    }

    if (!persist_manifest(installed_manifest, error)) {
        return false;
    }

    installed_plugins_[manifest.id] = InstalledPluginRecord{
        .id = manifest.id,
        .version = manifest.version
    };
    return save(error);
}

bool StoragePluginManager::persist_manifest(const StoragePluginManifest& manifest, std::string* error) const
{
    return save_storage_plugin_manifest_to_file(manifest, manifest_path_for_plugin(manifest.id), error);
}

bool StoragePluginManager::materialize_manifest_for_install(
    const StoragePluginManifest& manifest,
    StoragePluginManifest* materialized_manifest,
    std::string* error) const
{
    if (!materialized_manifest) {
        if (error) {
            *error = "No manifest output target provided.";
        }
        return false;
    }

    *materialized_manifest = manifest;
    if (manifest.entry_point_kind != "external_process") {
        return true;
    }

    const auto install_dir = package_directory_for_plugin(manifest);
    std::error_code ec;
    std::filesystem::remove_all(install_dir, ec);
    ec.clear();
    if (!std::filesystem::create_directories(install_dir, ec) && ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    std::vector<std::string> package_paths = manifest.package_paths;
    if (package_paths.empty()) {
        package_paths.push_back(manifest.entry_point);
    } else if (std::find(package_paths.begin(), package_paths.end(), manifest.entry_point) ==
               package_paths.end()) {
        package_paths.push_back(manifest.entry_point);
    }

    for (const auto& package_path : package_paths) {
        const auto source_path = resolve_plugin_source_path(manifest, package_path);
        const auto destination_path = install_dir / relative_install_target(manifest, package_path);
        if (!copy_plugin_asset(source_path, destination_path, error)) {
            return false;
        }
    }

    const auto installed_entry_point =
        install_dir / relative_install_target(manifest, manifest.entry_point);
    materialized_manifest->entry_point = installed_entry_point.string();
    materialized_manifest->source_path.clear();

#ifndef _WIN32
    if (manifest.entry_point_kind == "external_process") {
        std::error_code ec;
        std::filesystem::permissions(
            installed_entry_point,
            std::filesystem::perms::owner_exec |
                std::filesystem::perms::group_exec |
                std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add,
            ec);
        if (ec) {
            if (error) {
                *error = "Failed to mark plugin entry point executable: " + ec.message();
            }
            return false;
        }
    }
#endif
    return true;
}

bool StoragePluginManager::remove_plugin_artifacts(const std::string& plugin_id, std::string* error) const
{
    std::error_code ec;
    std::filesystem::remove(manifest_path_for_plugin(plugin_id), ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    std::filesystem::remove_all(package_directory_for_config_dir(config_dir_) / plugin_id, ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    return true;
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
