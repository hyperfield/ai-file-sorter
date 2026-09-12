#include "FolderStructurePluginManager.hpp"

#include "PluginArchiveExtractor.hpp"
#include "Utils.hpp"

#include <QDir>
#include <QTemporaryDir>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <system_error>
#include <utility>

namespace {

bool copy_package_tree(const std::filesystem::path& source,
                       const std::filesystem::path& destination,
                       std::string* error)
{
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

    std::filesystem::copy(source,
                          destination,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          ec);
    if (ec) {
        if (error) {
            *error = "Failed to install folder-structure plugin package: " + ec.message();
        }
        return false;
    }
    return true;
}

std::vector<std::filesystem::path> candidate_manifest_paths(const std::filesystem::path& package_root)
{
    std::vector<std::filesystem::path> paths;
    std::error_code ec;
    if (!std::filesystem::is_directory(package_root, ec) || ec) {
        return paths;
    }

    for (std::filesystem::directory_iterator plugin_it(package_root, ec), plugin_end;
         plugin_it != plugin_end && !ec;
         plugin_it.increment(ec)) {
        if (!plugin_it->is_directory(ec) || ec) {
            continue;
        }
        for (std::filesystem::directory_iterator version_it(plugin_it->path(), ec), version_end;
             version_it != version_end && !ec;
             version_it.increment(ec)) {
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

} // namespace

FolderStructurePluginManager::FolderStructurePluginManager(
    std::string config_dir,
    std::vector<FolderStructurePluginPublicKey> trusted_keys)
    : config_dir_(std::move(config_dir)),
      trusted_keys_(std::move(trusted_keys))
{
}

std::filesystem::path FolderStructurePluginManager::package_directory_for_config_dir(
    const std::string& config_dir)
{
    return std::filesystem::path(config_dir) / "plugins" / "folder-structures" / "packages";
}

std::filesystem::path FolderStructurePluginManager::staging_directory_for_config_dir(
    const std::string& config_dir)
{
    return std::filesystem::path(config_dir) / "plugins" / "folder-structures" / "staging";
}

std::vector<FolderStructurePluginManifest> FolderStructurePluginManager::installed_plugins() const
{
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

std::vector<FolderStructurePluginProfile> FolderStructurePluginManager::installed_profiles() const
{
    std::vector<FolderStructurePluginProfile> profiles;
    for (const auto& manifest : installed_plugins()) {
        std::string error;
        auto package_profiles = load_folder_structure_plugin_profiles(manifest, &error);
        profiles.insert(profiles.end(),
                        std::make_move_iterator(package_profiles.begin()),
                        std::make_move_iterator(package_profiles.end()));
    }
    return profiles;
}

bool FolderStructurePluginManager::is_installed(const std::string& plugin_id) const
{
    const auto manifests = installed_plugins();
    return std::any_of(manifests.begin(),
                       manifests.end(),
                       [&](const FolderStructurePluginManifest& manifest) {
                           return manifest.id == plugin_id;
                       });
}

bool FolderStructurePluginManager::install_from_archive(const std::filesystem::path& archive_path,
                                                        std::string* installed_plugin_id,
                                                        std::string* error) const
{
    if (!PluginArchiveExtractor::supports_archive(archive_path)) {
        if (error) {
            *error = "Only .aifsplugin and .zip plugin packages are supported.";
        }
        return false;
    }

    QDir staging_dir(QString::fromStdString(Utils::path_to_utf8(staging_root())));
    if (!staging_dir.exists() && !staging_dir.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = "Failed to create folder-structure plugin staging directory.";
        }
        return false;
    }

    QTemporaryDir temp_dir(QString::fromStdString(
        Utils::path_to_utf8(staging_root() / "archive-XXXXXX")));
    if (!temp_dir.isValid()) {
        if (error) {
            *error = "Failed to create a temporary folder-structure plugin extraction directory.";
        }
        return false;
    }

    auto extraction =
        PluginArchiveExtractor::extract_archive(archive_path,
                                                Utils::utf8_to_path(temp_dir.path().toStdString()));
    if (!extraction.ok()) {
        if (error) {
            *error = extraction.message;
        }
        return false;
    }

    std::string signer_key_id;
    const std::filesystem::path extracted_package_root = extraction.manifest_path.parent_path();
    if (!verify_folder_structure_plugin_package(extracted_package_root,
                                                trusted_keys(),
                                                &signer_key_id,
                                                error)) {
        return false;
    }

    auto manifest = load_folder_structure_plugin_manifest_from_file(extraction.manifest_path, error);
    if (!manifest) {
        return false;
    }
    manifest->verified_signer_key_id = signer_key_id;
    const auto profiles = load_folder_structure_plugin_profiles(*manifest, error);
    if (profiles.empty()) {
        if (error && error->empty()) {
            *error = "Folder-structure plugin package contains no usable profiles.";
        }
        return false;
    }

    const auto install_dir = package_root() / manifest->id / manifest->version;
    std::error_code ec;
    std::filesystem::remove_all(package_root() / manifest->id, ec);
    ec.clear();
    if (!copy_package_tree(extracted_package_root, install_dir, error)) {
        return false;
    }

    if (installed_plugin_id) {
        *installed_plugin_id = manifest->id;
    }
    return true;
}

bool FolderStructurePluginManager::uninstall(const std::string& plugin_id, std::string* error) const
{
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
    return true;
}

std::filesystem::path FolderStructurePluginManager::package_root() const
{
    return package_directory_for_config_dir(config_dir_);
}

std::filesystem::path FolderStructurePluginManager::staging_root() const
{
    return staging_directory_for_config_dir(config_dir_);
}

std::vector<FolderStructurePluginPublicKey> FolderStructurePluginManager::trusted_keys() const
{
    if (!trusted_keys_.empty()) {
        return trusted_keys_;
    }
    return default_folder_structure_plugin_public_keys();
}

std::optional<FolderStructurePluginManifest> FolderStructurePluginManager::load_verified_manifest(
    const std::filesystem::path& package_dir,
    std::string* error) const
{
    std::string signer_key_id;
    if (!verify_folder_structure_plugin_package(package_dir, trusted_keys(), &signer_key_id, error)) {
        return std::nullopt;
    }

    auto manifest = load_folder_structure_plugin_manifest_from_file(package_dir / "manifest.json", error);
    if (!manifest) {
        return std::nullopt;
    }
    manifest->verified_signer_key_id = signer_key_id;
    return manifest;
}
