#include "FolderStructurePluginManager.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <algorithm>
#include <app_version.hpp>
#include <filesystem>
#include <iterator>
#include <optional>
#include <system_error>
#include <utility>

#include "PluginArchiveExtractor.hpp"
#include "PluginEntitlementService.hpp"
#include "PluginLicenseActivator.hpp"
#include "Utils.hpp"

namespace {

constexpr char kDisabledPluginIdsKey[] = "disabled_plugin_ids";

bool copy_package_tree(const std::filesystem::path& source, const std::filesystem::path& destination,
                       std::string* error) {
    std::error_code ec;
    std::filesystem::remove_all(destination, ec);
    ec.clear();
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        if (error) {
            *error = "Failed to create folder-structure plugin package directory: " + ec.message();
        }
        return false;
    }

    std::filesystem::copy(source, destination,
                          std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                          ec);
    if (ec) {
        if (error) {
            *error = "Failed to install folder-structure plugin package: " + ec.message();
        }
        return false;
    }
    return true;
}

bool fail_install(FolderStructurePluginInstallError* error, std::string message) {
    if (error) {
        *error = FolderStructurePluginInstallError{};
        error->message = std::move(message);
    }
    return false;
}

std::vector<std::filesystem::path> candidate_manifest_paths(const std::filesystem::path& package_root) {
    std::vector<std::filesystem::path> paths;
    std::error_code ec;
    if (!std::filesystem::is_directory(package_root, ec) || ec) {
        return paths;
    }

    for (std::filesystem::directory_iterator plugin_it(package_root, ec), plugin_end; plugin_it != plugin_end && !ec;
         plugin_it.increment(ec)) {
        if (!plugin_it->is_directory(ec) || ec) {
            continue;
        }
        for (std::filesystem::directory_iterator version_it(plugin_it->path(), ec), version_end;
             version_it != version_end && !ec; version_it.increment(ec)) {
            if (!version_it->is_directory(ec) || ec) {
                continue;
            }
            const auto manifest = version_it->path() / "manifest.json";
            if (std::filesystem::is_regular_file(manifest, ec) && !ec) {
                paths.push_back(manifest);
            }
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

}  // namespace

FolderStructurePluginManager::FolderStructurePluginManager(
    std::string config_dir, std::vector<FolderStructurePluginPublicKey> trusted_keys,
    std::shared_ptr<const PluginEntitlementService> entitlement_service)
    : config_dir_(std::move(config_dir)),
      trusted_keys_(std::move(trusted_keys)),
      entitlement_service_(std::move(entitlement_service)) {
    if (!entitlement_service_) {
        entitlement_service_ = std::make_shared<PluginEntitlementService>(std::filesystem::path(config_dir_));
    }
}

std::filesystem::path FolderStructurePluginManager::package_directory_for_config_dir(const std::string& config_dir) {
    return std::filesystem::path(config_dir) / "plugins" / "folder-structures" / "packages";
}

std::filesystem::path FolderStructurePluginManager::staging_directory_for_config_dir(const std::string& config_dir) {
    return std::filesystem::path(config_dir) / "plugins" / "folder-structures" / "staging";
}

std::vector<FolderStructurePluginManifest> FolderStructurePluginManager::installed_plugins() const {
    std::vector<FolderStructurePluginManifest> manifests;
    for (const auto& manifest_path : candidate_manifest_paths(package_root())) {
        std::string error;
        auto manifest = load_verified_manifest(manifest_path.parent_path(), &error);
        if (manifest) {
            manifests.push_back(std::move(*manifest));
        }
    }
    return manifests;
}

std::vector<FolderStructurePluginProfile> FolderStructurePluginManager::installed_profiles() const {
    std::vector<FolderStructurePluginProfile> profiles;
    const auto disabled_ids = disabled_plugin_ids();
    for (const auto& manifest : installed_plugins()) {
        if (std::find(disabled_ids.begin(), disabled_ids.end(), manifest.id) != disabled_ids.end()) {
            continue;
        }
        std::string error;
        auto package_profiles = load_folder_structure_plugin_profiles(manifest, &error);
        profiles.insert(profiles.end(), std::make_move_iterator(package_profiles.begin()),
                        std::make_move_iterator(package_profiles.end()));
    }
    return profiles;
}

bool FolderStructurePluginManager::is_installed(const std::string& plugin_id) const {
    const auto manifests = installed_plugins();
    return std::any_of(manifests.begin(), manifests.end(),
                       [&](const FolderStructurePluginManifest& manifest) { return manifest.id == plugin_id; });
}

bool FolderStructurePluginManager::is_enabled(const std::string& plugin_id) const {
    if (plugin_id.empty()) {
        return false;
    }
    const auto disabled_ids = disabled_plugin_ids();
    return std::find(disabled_ids.begin(), disabled_ids.end(), plugin_id) == disabled_ids.end();
}

bool FolderStructurePluginManager::set_enabled(const std::string& plugin_id, bool enabled, std::string* error) const {
    if (plugin_id.empty()) {
        if (error) {
            *error = "No folder-structure plugin id was provided.";
        }
        return false;
    }

    auto disabled_ids = disabled_plugin_ids();
    const auto it = std::find(disabled_ids.begin(), disabled_ids.end(), plugin_id);
    if (enabled) {
        if (it != disabled_ids.end()) {
            disabled_ids.erase(it);
        }
    } else if (it == disabled_ids.end()) {
        disabled_ids.push_back(plugin_id);
    }
    return save_disabled_plugin_ids(std::move(disabled_ids), error);
}

bool FolderStructurePluginManager::activate_license(const std::string& product_id, const std::string& license_key,
                                                    std::string* error) const {
    PluginLicenseActivationRequest request;
    request.activation_url = PluginLicenseActivator::default_activation_url();
    request.license_key = license_key;
    request.product_id = product_id;
    request.app_version = APP_VERSION.to_numeric_string();
    request.platform = PluginLicenseActivator::default_platform();

    const PluginLicenseActivationResult result =
        PluginLicenseActivator::activate(std::filesystem::path(config_dir_), request);
    if (result.activated) {
        return true;
    }

    if (error) {
        *error = result.message.empty() ? "Failed to activate plugin license." : result.message;
    }
    return false;
}

bool FolderStructurePluginManager::install_from_archive(const std::filesystem::path& archive_path,
                                                        std::string* installed_plugin_id, std::string* error) const {
    FolderStructurePluginInstallError detailed_error;
    const bool installed = install_from_archive(archive_path, installed_plugin_id, &detailed_error);
    if (!installed && error) {
        *error = detailed_error.message;
    }
    return installed;
}

bool FolderStructurePluginManager::install_from_archive(const std::filesystem::path& archive_path,
                                                        std::string* installed_plugin_id,
                                                        FolderStructurePluginInstallError* error) const {
    if (!PluginArchiveExtractor::supports_archive(archive_path)) {
        return fail_install(error, "Only .aifsplugin and .zip plugin packages are supported.");
    }

    QDir staging_dir(QString::fromStdString(Utils::path_to_utf8(staging_root())));
    if (!staging_dir.exists() && !staging_dir.mkpath(QStringLiteral("."))) {
        return fail_install(error, "Failed to create folder-structure plugin staging directory.");
    }

    QTemporaryDir temp_dir(QString::fromStdString(Utils::path_to_utf8(staging_root() / "archive-XXXXXX")));
    if (!temp_dir.isValid()) {
        return fail_install(error, "Failed to create a temporary folder-structure plugin extraction directory.");
    }

    auto extraction =
        PluginArchiveExtractor::extract_archive(archive_path, Utils::utf8_to_path(temp_dir.path().toStdString()));
    if (!extraction.ok()) {
        return fail_install(error, extraction.message);
    }

    std::string signer_key_id;
    const std::filesystem::path extracted_package_root = extraction.manifest_path.parent_path();
    std::string failure_message;
    if (!verify_folder_structure_plugin_package(extracted_package_root, trusted_keys(), &signer_key_id,
                                                &failure_message)) {
        return fail_install(error, failure_message);
    }

    failure_message.clear();
    auto manifest = load_folder_structure_plugin_manifest_from_file(extraction.manifest_path, &failure_message);
    if (!manifest) {
        return fail_install(error, failure_message);
    }
    manifest->verified_signer_key_id = signer_key_id;
    if (!has_required_entitlement(*manifest, error)) {
        return false;
    }

    failure_message.clear();
    const auto profiles = load_folder_structure_plugin_profiles(*manifest, &failure_message);
    if (profiles.empty()) {
        return fail_install(error, failure_message.empty()
                                       ? "Folder-structure plugin package contains no usable profiles."
                                       : failure_message);
    }

    const auto install_dir = package_root() / manifest->id / manifest->version;
    std::error_code ec;
    std::filesystem::remove_all(package_root() / manifest->id, ec);
    ec.clear();
    failure_message.clear();
    if (!copy_package_tree(extracted_package_root, install_dir, &failure_message)) {
        return fail_install(error, failure_message);
    }

    if (installed_plugin_id) {
        *installed_plugin_id = manifest->id;
    }
    return true;
}

bool FolderStructurePluginManager::uninstall(const std::string& plugin_id, std::string* error) const {
    if (plugin_id.empty()) {
        if (error) {
            *error = "No folder-structure plugin id was provided.";
        }
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(package_root() / plugin_id, ec);
    if (ec) {
        if (error) {
            *error = "Failed to remove folder-structure plugin package: " + ec.message();
        }
        return false;
    }
    set_enabled(plugin_id, true);
    return true;
}

std::filesystem::path FolderStructurePluginManager::package_root() const {
    return package_directory_for_config_dir(config_dir_);
}

std::filesystem::path FolderStructurePluginManager::staging_root() const {
    return staging_directory_for_config_dir(config_dir_);
}

std::filesystem::path FolderStructurePluginManager::state_file() const {
    return std::filesystem::path(config_dir_) / "plugins" / "folder-structures" / "state.json";
}

std::vector<FolderStructurePluginPublicKey> FolderStructurePluginManager::trusted_keys() const {
    if (!trusted_keys_.empty()) {
        return trusted_keys_;
    }
    return default_folder_structure_plugin_public_keys();
}

std::vector<std::string> FolderStructurePluginManager::disabled_plugin_ids() const {
    const auto path = state_file();
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    std::vector<std::string> ids;
    const QJsonArray values = document.object().value(QString::fromLatin1(kDisabledPluginIdsKey)).toArray();
    ids.reserve(static_cast<std::size_t>(values.size()));
    for (const auto& value : values) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty()) {
            ids.push_back(id.toStdString());
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

bool FolderStructurePluginManager::save_disabled_plugin_ids(std::vector<std::string> plugin_ids,
                                                            std::string* error) const {
    std::sort(plugin_ids.begin(), plugin_ids.end());
    plugin_ids.erase(std::unique(plugin_ids.begin(), plugin_ids.end()), plugin_ids.end());

    std::error_code ec;
    std::filesystem::create_directories(state_file().parent_path(), ec);
    if (ec) {
        if (error) {
            *error = "Failed to create folder-structure plugin state directory: " + ec.message();
        }
        return false;
    }

    QJsonArray values;
    for (const auto& plugin_id : plugin_ids) {
        values.append(QString::fromStdString(plugin_id));
    }
    QJsonObject object;
    object.insert(QString::fromLatin1(kDisabledPluginIdsKey), values);

    QFile file(QString::fromStdString(Utils::path_to_utf8(state_file())));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = "Failed to save folder-structure plugin state.";
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

bool FolderStructurePluginManager::has_required_entitlement(const FolderStructurePluginManifest& manifest,
                                                            FolderStructurePluginInstallError* error) const {
    if (!manifest.license_required) {
        return true;
    }

    const std::string product_id = manifest.product_id.empty() ? manifest.id : manifest.product_id;
    if (entitlement_service_ && entitlement_service_->has_entitlement(product_id, manifest.id)) {
        return true;
    }

    if (error) {
        *error = FolderStructurePluginInstallError{};
        error->message =
            "This folder-structure plugin requires an active entitlement for product '" + product_id + "'.";
        error->missing_entitlement = true;
        error->product_id = product_id;
        error->plugin_id = manifest.id;
        error->plugin_name = manifest.name.empty() ? manifest.id : manifest.name;
        error->purchase_url = manifest.purchase_url;
    }
    return false;
}

bool FolderStructurePluginManager::has_required_entitlement(const FolderStructurePluginManifest& manifest,
                                                            std::string* error) const {
    FolderStructurePluginInstallError detailed_error;
    const bool has_entitlement = has_required_entitlement(manifest, &detailed_error);
    if (!has_entitlement && error) {
        *error = detailed_error.message;
    }
    return has_entitlement;
}

std::optional<FolderStructurePluginManifest> FolderStructurePluginManager::load_verified_manifest(
    const std::filesystem::path& package_dir, std::string* error) const {
    std::string signer_key_id;
    if (!verify_folder_structure_plugin_package(package_dir, trusted_keys(), &signer_key_id, error)) {
        return std::nullopt;
    }

    auto manifest = load_folder_structure_plugin_manifest_from_file(package_dir / "manifest.json", error);
    if (!manifest) {
        return std::nullopt;
    }
    manifest->verified_signer_key_id = signer_key_id;
    if (!has_required_entitlement(*manifest, error)) {
        return std::nullopt;
    }
    return manifest;
}
