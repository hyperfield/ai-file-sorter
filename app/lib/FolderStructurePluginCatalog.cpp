#include "FolderStructurePluginCatalog.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

#ifndef AIFS_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL
#ifdef AI_FILE_SORTER_TEST_BUILD
#define AIFS_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL ""
#else
#define AIFS_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL "https://filesorter.app/api/plugins/folder-structures/catalog/"
#endif
#endif

constexpr char kCatalogVersion[] = "aifs.folder_structure_plugin_catalog.v1";

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string string_field(const QJsonObject& object, const char* key) {
    return object.value(QString::fromLatin1(key)).toString().trimmed().toStdString();
}

std::optional<FolderStructurePluginCatalogEntry> catalog_entry_from_object(const QJsonObject& object,
                                                                           std::string* error) {
    FolderStructurePluginCatalogEntry entry;
    entry.id = string_field(object, "id");
    entry.name = string_field(object, "name");
    entry.description = string_field(object, "description");
    entry.version = string_field(object, "version");
    entry.license_required = object.value("license_required").toBool(false);
    entry.product_id = string_field(object, "product_id");
    entry.purchase_url = string_field(object, "purchase_url");
    entry.package_download_url = string_field(object, "package_download_url");
    entry.package_sha256 = string_field(object, "package_sha256");

    if (entry.id.empty() || entry.name.empty() || entry.version.empty()) {
        if (error) {
            *error = "Folder-structure plugin catalog entry is missing required fields.";
        }
        return std::nullopt;
    }
    if (entry.license_required && entry.product_id.empty()) {
        entry.product_id = entry.id;
    }

    const bool has_package_url = !entry.package_download_url.empty();
    const bool has_package_sha = !entry.package_sha256.empty();
    if (has_package_url != has_package_sha) {
        if (error) {
            *error = "Folder-structure plugin catalog entry has incomplete package metadata.";
        }
        return std::nullopt;
    }

    return entry;
}

void append_catalog_entry(const QJsonObject& object, std::vector<FolderStructurePluginCatalogEntry>* entries,
                          std::string* error) {
    std::string entry_error;
    auto entry = catalog_entry_from_object(object, &entry_error);
    if (!entry.has_value()) {
        if (error && error->empty()) {
            *error = entry_error.empty() ? "Invalid folder-structure plugin catalog entry." : entry_error;
        }
        return;
    }
    entries->push_back(std::move(*entry));
}

std::vector<FolderStructurePluginCatalogEntry> entries_from_document(const QJsonDocument& document,
                                                                     std::string* error) {
    std::vector<FolderStructurePluginCatalogEntry> entries;

    if (document.isArray()) {
        const QJsonArray array = document.array();
        entries.reserve(static_cast<std::size_t>(array.size()));
        for (const auto& value : array) {
            if (!value.isObject()) {
                if (error && error->empty()) {
                    *error = "Folder-structure plugin catalog contains a non-object entry.";
                }
                continue;
            }
            append_catalog_entry(value.toObject(), &entries, error);
        }
        return entries;
    }

    if (!document.isObject()) {
        if (error) {
            *error = "Invalid folder-structure plugin catalog JSON.";
        }
        return entries;
    }

    const QJsonObject object = document.object();
    if (object.contains("plugins")) {
        const std::string version = string_field(object, "version");
        if (!version.empty() && version != kCatalogVersion) {
            if (error) {
                *error = "Unsupported folder-structure plugin catalog version.";
            }
            return entries;
        }
        if (!object.value("plugins").isArray()) {
            if (error) {
                *error = "Folder-structure plugin catalog plugins field must be an array.";
            }
            return entries;
        }

        const QJsonArray plugins = object.value("plugins").toArray();
        entries.reserve(static_cast<std::size_t>(plugins.size()));
        for (const auto& value : plugins) {
            if (!value.isObject()) {
                if (error && error->empty()) {
                    *error = "Folder-structure plugin catalog contains a non-object entry.";
                }
                continue;
            }
            append_catalog_entry(value.toObject(), &entries, error);
        }
        return entries;
    }

    append_catalog_entry(object, &entries, error);
    return entries;
}

}  // namespace

std::string default_folder_structure_plugin_catalog_url() {
    const QByteArray env_value = qgetenv("AI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL");
    if (!env_value.isEmpty()) {
        return trim_copy(env_value.toStdString());
    }
    return AIFS_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL;
}

std::optional<FolderStructurePluginCatalogEntry> load_folder_structure_plugin_catalog_entry_from_json(
    const std::string& json, std::string* error) {
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!document.isObject()) {
        if (error) {
            *error = "Invalid folder-structure plugin catalog entry JSON.";
        }
        return std::nullopt;
    }
    return catalog_entry_from_object(document.object(), error);
}

std::vector<FolderStructurePluginCatalogEntry> load_folder_structure_plugin_catalog_entries_from_json(
    const std::string& json, std::string* error) {
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    return entries_from_document(document, error);
}

FolderStructurePluginCatalogFetcher::FolderStructurePluginCatalogFetcher(std::filesystem::path cache_directory,
                                                                         DownloadFunction download_fn)
    : cache_directory_(std::move(cache_directory)), fetcher_(cache_directory_, std::move(download_fn)) {}

std::vector<FolderStructurePluginCatalogEntry> FolderStructurePluginCatalogFetcher::fetch_catalog_entries(
    const std::string& catalog_url, std::string* error) const {
    if (catalog_url.empty()) {
        if (error) {
            *error = "No folder-structure plugin catalog URL is configured.";
        }
        return {};
    }

    auto catalog_json = fetcher_.fetch_text(catalog_url, catalog_cache_path(cache_directory_), error);
    if (!catalog_json.has_value()) {
        return {};
    }
    return load_folder_structure_plugin_catalog_entries_from_json(*catalog_json, error);
}

bool FolderStructurePluginCatalogFetcher::fetch_package_archive(const FolderStructurePluginCatalogEntry& entry,
                                                                std::filesystem::path* archive_path,
                                                                std::string* error) const {
    const PluginPackageSource source{.plugin_id = entry.id,
                                     .version = entry.version,
                                     .package_download_url = entry.package_download_url,
                                     .package_sha256 = entry.package_sha256};
    return fetcher_.fetch_package_archive(source, archive_path, error);
}

std::filesystem::path FolderStructurePluginCatalogFetcher::catalog_cache_path(
    const std::filesystem::path& cache_directory) {
    return cache_directory / "catalog" / "folder-structure-plugin-catalog.json";
}
