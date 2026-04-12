#include "StoragePluginManifest.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

std::string storage_plugin_current_platform();
std::string storage_plugin_current_architecture();

namespace {

#ifndef AIFS_ONEDRIVE_STORAGE_PLUGIN_NAME
#define AIFS_ONEDRIVE_STORAGE_PLUGIN_NAME "aifs_onedrive_storage_plugin"
#endif

#ifndef AIFS_ONEDRIVE_STORAGE_PLUGIN_MANIFEST_URL
#define AIFS_ONEDRIVE_STORAGE_PLUGIN_MANIFEST_URL ""
#endif

std::string env_or_default(const char* env_name, const char* fallback)
{
    const QByteArray env_value = qgetenv(env_name);
    if (!env_value.isEmpty()) {
        return env_value.toStdString();
    }
    return fallback ? std::string(fallback) : std::string();
}

std::string ascii_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string normalize_platform_name(std::string value)
{
    value = ascii_lower_copy(std::move(value));
    if (value == "win" || value == "win32") {
        return "windows";
    }
    if (value == "mac" || value == "osx" || value == "darwin") {
        return "macos";
    }
    if (value == "any" || value == "all" || value == "*") {
        return "any";
    }
    return value;
}

std::string normalize_architecture_name(std::string value)
{
    value = ascii_lower_copy(std::move(value));
    if (value == "amd64" || value == "x64") {
        return "x86_64";
    }
    if (value == "aarch64") {
        return "arm64";
    }
    if (value == "i386" || value == "i686") {
        return "x86";
    }
    if (value == "any" || value == "all" || value == "*") {
        return "any";
    }
    return value;
}

std::vector<std::string> parse_string_list_field(const QJsonObject& object,
                                                 const char* plural_key,
                                                 std::initializer_list<const char*> singular_keys,
                                                 std::string (*normalize)(std::string))
{
    std::vector<std::string> values;
    std::unordered_set<std::string> seen;

    const auto append_value = [&](const QString& text) {
        auto value = normalize(text.toStdString());
        if (value.empty()) {
            return;
        }
        if (seen.insert(value).second) {
            values.push_back(std::move(value));
        }
    };

    const auto plural_value = object.value(QString::fromLatin1(plural_key));
    if (plural_value.isArray()) {
        const auto array = plural_value.toArray();
        values.reserve(static_cast<std::size_t>(array.size()));
        for (const auto& entry : array) {
            if (entry.isString()) {
                append_value(entry.toString());
            }
        }
        return values;
    }
    if (plural_value.isString()) {
        append_value(plural_value.toString());
        return values;
    }

    for (const auto* singular_key : singular_keys) {
        const auto value = object.value(QString::fromLatin1(singular_key));
        if (value.isString()) {
            append_value(value.toString());
            break;
        }
    }
    return values;
}

bool list_matches_value(const std::vector<std::string>& values, const std::string& expected)
{
    if (values.empty()) {
        return true;
    }
    return std::find(values.begin(), values.end(), std::string("any")) != values.end() ||
           std::find(values.begin(), values.end(), expected) != values.end();
}

int manifest_specificity(const StoragePluginManifest& manifest)
{
    int score = 0;
    if (!manifest.platforms.empty() &&
        std::find(manifest.platforms.begin(), manifest.platforms.end(), std::string("any")) ==
            manifest.platforms.end()) {
        score += 2;
    }
    if (!manifest.architectures.empty() &&
        std::find(manifest.architectures.begin(), manifest.architectures.end(), std::string("any")) ==
            manifest.architectures.end()) {
        score += 1;
    }
    return score;
}

bool manifest_matches_runtime(const StoragePluginManifest& manifest,
                              const std::string& platform,
                              const std::string& architecture,
                              std::string* error)
{
    if (!list_matches_value(manifest.platforms, platform)) {
        if (error) {
            *error = "Plugin targets a different platform.";
        }
        return false;
    }
    if (!list_matches_value(manifest.architectures, architecture)) {
        if (error) {
            *error = "Plugin targets a different CPU architecture.";
        }
        return false;
    }
    return true;
}

std::vector<StoragePluginManifest> select_best_runtime_manifests(
    std::vector<StoragePluginManifest> manifests)
{
    const auto current_platform = storage_plugin_current_platform();
    const auto current_architecture = storage_plugin_current_architecture();

    std::unordered_map<std::string, std::size_t> index_by_id;
    std::vector<StoragePluginManifest> filtered;
    filtered.reserve(manifests.size());

    for (auto& manifest : manifests) {
        if (!manifest_matches_runtime(manifest, current_platform, current_architecture, nullptr)) {
            continue;
        }

        const auto existing = index_by_id.find(manifest.id);
        if (existing == index_by_id.end()) {
            index_by_id.emplace(manifest.id, filtered.size());
            filtered.push_back(std::move(manifest));
            continue;
        }

        auto& current = filtered[existing->second];
        if (manifest_specificity(manifest) >= manifest_specificity(current)) {
            current = std::move(manifest);
        }
    }

    return filtered;
}

std::vector<StoragePluginManifest> filter_catalog_manifests_for_runtime(
    std::vector<StoragePluginManifest> manifests,
    std::string* error)
{
    const bool had_entries = !manifests.empty();
    auto filtered = select_best_runtime_manifests(std::move(manifests));
    if (filtered.empty() && had_entries && error && error->empty()) {
        *error = "Plugin catalog does not contain any entries for this runtime (" +
                 storage_plugin_current_platform() + "/" +
                 storage_plugin_current_architecture() + ").";
    }
    return filtered;
}

const std::vector<StoragePluginManifest>& manifest_catalog()
{
    static const std::vector<StoragePluginManifest> catalog = {
        StoragePluginManifest{
            .id = "onedrive_storage_support",
            .name = "OneDrive Storage Support",
            .description =
                "Adds a dedicated OneDrive connector process with stronger sync-state detection, move preflight checks, and richer undo metadata for synced folders.",
            .version = "1.1.0",
            .provider_ids = {"onedrive"},
            .remote_manifest_url = env_or_default("AI_FILE_SORTER_ONEDRIVE_PLUGIN_MANIFEST_URL",
                                                  AIFS_ONEDRIVE_STORAGE_PLUGIN_MANIFEST_URL),
            .entry_point_kind = "external_process",
            .entry_point = AIFS_ONEDRIVE_STORAGE_PLUGIN_NAME,
            .package_paths = {AIFS_ONEDRIVE_STORAGE_PLUGIN_NAME}
        },
        StoragePluginManifest{
            .id = "cloud_storage_compat",
            .name = "Cloud Storage Compatibility",
            .description =
                "Adds compatibility providers for Dropbox and pCloud. "
                "Installed providers use safer recursive scans and relaxed undo timestamp validation for synced folders.",
            .version = "1.0.0",
            .provider_ids = {"dropbox", "pcloud"},
            .entry_point_kind = "builtin_bundle",
            .entry_point = "cloud_storage_compat_bundle"
        }
    };
    return catalog;
}

QJsonObject to_json_object(const StoragePluginManifest& manifest)
{
    QJsonArray provider_ids;
    for (const auto& provider_id : manifest.provider_ids) {
        provider_ids.append(QString::fromStdString(provider_id));
    }

    QJsonArray platforms;
    for (const auto& platform : manifest.platforms) {
        platforms.append(QString::fromStdString(platform));
    }

    QJsonArray architectures;
    for (const auto& architecture : manifest.architectures) {
        architectures.append(QString::fromStdString(architecture));
    }

    QJsonArray package_paths;
    for (const auto& package_path : manifest.package_paths) {
        package_paths.append(QString::fromStdString(package_path));
    }

    QJsonObject object;
    object["id"] = QString::fromStdString(manifest.id);
    object["name"] = QString::fromStdString(manifest.name);
    object["description"] = QString::fromStdString(manifest.description);
    object["version"] = QString::fromStdString(manifest.version);
    object["remote_manifest_url"] = QString::fromStdString(manifest.remote_manifest_url);
    object["package_download_url"] = QString::fromStdString(manifest.package_download_url);
    object["package_sha256"] = QString::fromStdString(manifest.package_sha256);
    object["provider_ids"] = provider_ids;
    object["platforms"] = platforms;
    object["architectures"] = architectures;
    object["entry_point_kind"] = QString::fromStdString(manifest.entry_point_kind);
    object["entry_point"] = QString::fromStdString(manifest.entry_point);
    object["package_paths"] = package_paths;
    return object;
}

std::optional<StoragePluginManifest> from_json_object(const QJsonObject& object, std::string* error)
{
    StoragePluginManifest manifest;
    manifest.id = object.value("id").toString().toStdString();
    manifest.name = object.value("name").toString().toStdString();
    manifest.description = object.value("description").toString().toStdString();
    manifest.version = object.value("version").toString().toStdString();
    manifest.remote_manifest_url = object.value("remote_manifest_url").toString().toStdString();
    manifest.package_download_url = object.value("package_download_url").toString().toStdString();
    manifest.package_sha256 = object.value("package_sha256").toString().toStdString();
    manifest.entry_point_kind = object.value("entry_point_kind").toString().toStdString();
    manifest.entry_point = object.value("entry_point").toString().toStdString();
    manifest.platforms = parse_string_list_field(
        object,
        "platforms",
        {"platform"},
        normalize_platform_name);
    manifest.architectures = parse_string_list_field(
        object,
        "architectures",
        {"architecture", "arch"},
        normalize_architecture_name);

    const QJsonArray provider_ids = object.value("provider_ids").toArray();
    manifest.provider_ids.reserve(static_cast<std::size_t>(provider_ids.size()));
    for (const auto& value : provider_ids) {
        const auto provider_id = value.toString().toStdString();
        if (!provider_id.empty()) {
            manifest.provider_ids.push_back(provider_id);
        }
    }

    const QJsonArray package_paths = object.value("package_paths").toArray();
    manifest.package_paths.reserve(static_cast<std::size_t>(package_paths.size()));
    for (const auto& value : package_paths) {
        const auto package_path = value.toString().toStdString();
        if (!package_path.empty()) {
            manifest.package_paths.push_back(package_path);
        }
    }

    if ((manifest.entry_point_kind.empty() || manifest.entry_point.empty()) && !manifest.id.empty()) {
        for (const auto& builtin_manifest : manifest_catalog()) {
            if (builtin_manifest.id == manifest.id) {
                manifest.entry_point_kind = builtin_manifest.entry_point_kind;
                manifest.entry_point = builtin_manifest.entry_point;
                break;
            }
        }
    }

    if (manifest.id.empty() || manifest.name.empty() || manifest.version.empty() ||
        manifest.entry_point_kind.empty() || manifest.entry_point.empty()) {
        if (error) {
            *error = "Manifest is missing required fields.";
        }
        return std::nullopt;
    }

    return manifest;
}

std::vector<StoragePluginManifest> manifests_from_json_document(const QJsonDocument& doc, std::string* error)
{
    std::vector<StoragePluginManifest> manifests;

    const auto append_manifest = [&](const QJsonObject& object) {
        std::string manifest_error;
        auto manifest = from_json_object(object, &manifest_error);
        if (!manifest.has_value()) {
            if (error && error->empty()) {
                *error = manifest_error.empty() ? "Invalid plugin manifest in catalog." : manifest_error;
            }
            return;
        }
        manifests.push_back(std::move(*manifest));
    };

    if (doc.isArray()) {
        const QJsonArray array = doc.array();
        manifests.reserve(static_cast<std::size_t>(array.size()));
        for (const auto& value : array) {
            if (!value.isObject()) {
                if (error && error->empty()) {
                    *error = "Plugin catalog contains a non-object entry.";
                }
                continue;
            }
            append_manifest(value.toObject());
        }
        return filter_catalog_manifests_for_runtime(std::move(manifests), error);
    }

    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        if (object.contains("plugins") && object.value("plugins").isArray()) {
            const QJsonArray plugins = object.value("plugins").toArray();
            manifests.reserve(static_cast<std::size_t>(plugins.size()));
            for (const auto& value : plugins) {
                if (!value.isObject()) {
                    if (error && error->empty()) {
                        *error = "Plugin catalog contains a non-object entry.";
                    }
                    continue;
                }
                append_manifest(value.toObject());
            }
            return filter_catalog_manifests_for_runtime(std::move(manifests), error);
        }

        append_manifest(object);
        return filter_catalog_manifests_for_runtime(std::move(manifests), error);
    }

    if (error) {
        *error = "Invalid plugin manifest JSON.";
    }
    return manifests;
}

} // namespace

const std::vector<StoragePluginManifest>& builtin_storage_plugin_manifests()
{
    return manifest_catalog();
}

std::optional<StoragePluginManifest> find_storage_plugin_manifest(const std::string& plugin_id)
{
    for (const auto& manifest : manifest_catalog()) {
        if (manifest.id == plugin_id) {
            return manifest;
        }
    }
    return std::nullopt;
}

std::string storage_plugin_current_platform()
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string storage_plugin_current_architecture()
{
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

bool storage_plugin_manifest_matches_current_runtime(const StoragePluginManifest& manifest,
                                                     std::string* error)
{
    return manifest_matches_runtime(
        manifest,
        storage_plugin_current_platform(),
        storage_plugin_current_architecture(),
        error);
}

std::optional<StoragePluginManifest> load_storage_plugin_manifest_from_file(
    const std::filesystem::path& manifest_path,
    std::string* error)
{
    QFile file(QString::fromStdString(manifest_path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error) {
            *error = "Invalid plugin manifest JSON.";
        }
        return std::nullopt;
    }

    auto manifest = from_json_object(doc.object(), error);
    if (manifest.has_value()) {
        manifest->source_path = manifest_path;
    }
    return manifest;
}

std::optional<StoragePluginManifest> load_storage_plugin_manifest_from_json(
    const std::string& json,
    std::string* error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isObject()) {
        if (error) {
            *error = "Invalid plugin manifest JSON.";
        }
        return std::nullopt;
    }
    return from_json_object(doc.object(), error);
}

std::vector<StoragePluginManifest> load_storage_plugin_manifests_from_directory(
    const std::filesystem::path& manifest_directory,
    std::string* error)
{
    std::vector<StoragePluginManifest> manifests;
    if (manifest_directory.empty()) {
        return manifests;
    }

    QDir dir(QString::fromStdString(manifest_directory.string()));
    if (!dir.exists()) {
        return manifests;
    }

    const QFileInfoList entries = dir.entryInfoList(
        QStringList() << QStringLiteral("*.json"),
        QDir::Files | QDir::Readable,
        QDir::Name);

    for (const QFileInfo& entry : entries) {
        std::string manifest_error;
        auto manifest = load_storage_plugin_manifest_from_file(
            std::filesystem::path(entry.absoluteFilePath().toStdString()),
            &manifest_error);
        if (!manifest.has_value()) {
            if (error && error->empty()) {
                *error = manifest_error;
            }
            continue;
        }
        manifests.push_back(std::move(*manifest));
    }

    return select_best_runtime_manifests(std::move(manifests));
}

std::vector<StoragePluginManifest> load_storage_plugin_manifests_from_json(
    const std::string& json,
    std::string* error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    return manifests_from_json_document(doc, error);
}

bool save_storage_plugin_manifest_to_file(
    const StoragePluginManifest& manifest,
    const std::filesystem::path& manifest_path,
    std::string* error)
{
    const auto parent_directory = manifest_path.parent_path();
    if (!parent_directory.empty()) {
        QDir dir(QString::fromStdString(parent_directory.string()));
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            if (error) {
                *error = "Failed to create plugin manifest directory.";
            }
            return false;
        }
    }

    QFile file(QString::fromStdString(manifest_path.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }

    const QJsonDocument doc(to_json_object(manifest));
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }

    return true;
}

std::optional<StoragePluginManifest> find_storage_plugin_manifest_for_provider(const std::string& provider_id)
{
    for (const auto& manifest : manifest_catalog()) {
        for (const auto& supported_provider_id : manifest.provider_ids) {
            if (supported_provider_id == provider_id) {
                return manifest;
            }
        }
    }
    return std::nullopt;
}
