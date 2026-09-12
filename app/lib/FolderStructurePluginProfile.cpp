#include "FolderStructurePluginProfile.hpp"

#include "FolderTreeCatalog.hpp"
#include "Utils.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/evp.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace {

#ifndef AIFS_FOLDER_STRUCTURE_PLUGIN_PUBLIC_KEYS
#define AIFS_FOLDER_STRUCTURE_PLUGIN_PUBLIC_KEYS ""
#endif

constexpr char kFolderStructureEntryPointKind[] = "folder_structure_profile";
constexpr char kSignatureManifestName[] = "plugin-signature.json";
constexpr char kSignatureFileName[] = "plugin-signature.sig";

std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
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

bool list_matches_value(const std::vector<std::string>& values, const std::string& expected)
{
    return values.empty() ||
           std::find(values.begin(), values.end(), std::string("any")) != values.end() ||
           std::find(values.begin(), values.end(), expected) != values.end();
}

std::vector<std::string> parse_string_list_field(const QJsonObject& object,
                                                 const char* plural_key,
                                                 std::initializer_list<const char*> singular_keys,
                                                 std::string (*normalize)(std::string) = nullptr)
{
    std::vector<std::string> values;
    std::unordered_set<std::string> seen;

    const auto append_value = [&](QString text) {
        std::string value = text.trimmed().toStdString();
        if (normalize) {
            value = normalize(std::move(value));
        }
        if (!value.empty() && seen.insert(value).second) {
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

std::optional<std::filesystem::path> safe_relative_package_path(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    const std::filesystem::path normalized = std::filesystem::path(value).lexically_normal();
    if (normalized.empty() || normalized.has_root_name() || normalized.has_root_directory()) {
        return std::nullopt;
    }

    std::filesystem::path sanitized;
    for (const auto& component : normalized) {
        if (component == "." || component.empty()) {
            continue;
        }
        if (component == "..") {
            return std::nullopt;
        }
        sanitized /= component;
    }
    if (sanitized.empty()) {
        return std::nullopt;
    }
    return sanitized;
}

std::string generic_relative_path_text(const std::filesystem::path& path)
{
    const auto generic = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(generic.data()), generic.size());
}

std::string relative_path_text(const std::filesystem::path& root,
                               const std::filesystem::path& path)
{
    std::error_code ec;
    const auto relative = std::filesystem::relative(path, root, ec);
    if (ec || relative.empty() || relative == ".") {
        return {};
    }
    return generic_relative_path_text(relative);
}

bool is_executable_like_package_path(const std::string& relative_path)
{
    const std::filesystem::path path(relative_path);
    const std::string ext = ascii_lower_copy(path.extension().string());
    static const std::unordered_set<std::string> blocked = {
        ".bat", ".cmd", ".com", ".dll", ".dylib", ".exe", ".js",
        ".msi", ".ps1", ".py", ".sh", ".so", ".vbs"};
    return blocked.contains(ext);
}

QByteArray read_file_bytes(const std::filesystem::path& path, std::string* error)
{
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return {};
    }
    return file.readAll();
}

std::optional<QJsonObject> read_json_object(const std::filesystem::path& path, std::string* error)
{
    std::string read_error;
    const QByteArray bytes = read_file_bytes(path, &read_error);
    if (bytes.isEmpty() && !read_error.empty()) {
        if (error) {
            *error = read_error;
        }
        return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) {
        if (error) {
            *error = "Invalid JSON object.";
        }
        return std::nullopt;
    }
    return doc.object();
}

std::string normalize_sha256(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool is_sha256_hex(const std::string& value)
{
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isxdigit(ch) != 0;
           });
}

std::string compute_sha256(const std::filesystem::path& path)
{
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open signed plugin file for SHA-256 verification.");
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1 << 20);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            throw std::runtime_error("Failed while reading signed plugin file for SHA-256 verification.");
        }
        hash.addData(chunk);
    }
    return hash.result().toHex().toStdString();
}

QByteArray decode_base64_or_raw_signature(QByteArray signature)
{
    if (signature.size() == 64) {
        return signature;
    }

    signature = signature.trimmed();
    if (signature.size() == 64) {
        return signature;
    }

    QByteArray decoded = QByteArray::fromBase64(
        signature,
        QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.size() == 64) {
        return decoded;
    }
    decoded = QByteArray::fromBase64(
        signature,
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    return decoded.size() == 64 ? decoded : QByteArray();
}

bool verify_ed25519_signature(const QByteArray& payload,
                              const QByteArray& signature,
                              const FolderStructurePluginPublicKey& trusted_key)
{
    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519,
        nullptr,
        trusted_key.ed25519_public_key.data(),
        trusted_key.ed25519_public_key.size());
    if (!key) {
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(key);
        return false;
    }

    const bool verified =
        EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1 &&
        EVP_DigestVerify(
            ctx,
            reinterpret_cast<const unsigned char*>(signature.constData()),
            static_cast<std::size_t>(signature.size()),
            reinterpret_cast<const unsigned char*>(payload.constData()),
            static_cast<std::size_t>(payload.size())) == 1;

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return verified;
}

std::optional<FolderStructurePluginPublicKey> trusted_key_for_id(
    const std::vector<FolderStructurePluginPublicKey>& trusted_keys,
    const std::string& key_id)
{
    const auto match = std::find_if(trusted_keys.begin(),
                                    trusted_keys.end(),
                                    [&](const FolderStructurePluginPublicKey& key) {
                                        return key.key_id == key_id;
                                    });
    if (match == trusted_keys.end()) {
        return std::nullopt;
    }
    return *match;
}

std::optional<std::array<unsigned char, 32>> decode_public_key_base64(const std::string& encoded)
{
    QByteArray decoded = QByteArray::fromBase64(
        QByteArray::fromStdString(encoded),
        QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.size() != 32) {
        decoded = QByteArray::fromBase64(
            QByteArray::fromStdString(encoded),
            QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    }
    if (decoded.size() != 32) {
        return std::nullopt;
    }

    std::array<unsigned char, 32> key{};
    for (int i = 0; i < decoded.size(); ++i) {
        key[static_cast<std::size_t>(i)] = static_cast<unsigned char>(decoded.at(i));
    }
    return key;
}

std::vector<FolderStructurePluginPublicKey> parse_public_key_list(std::string value)
{
    std::vector<FolderStructurePluginPublicKey> keys;
    std::replace(value.begin(), value.end(), ';', ',');

    std::istringstream stream(value);
    std::string entry;
    while (std::getline(stream, entry, ',')) {
        entry = trim_copy(std::move(entry));
        if (entry.empty()) {
            continue;
        }

        const auto separator = entry.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key_id = trim_copy(entry.substr(0, separator));
        const std::string encoded = trim_copy(entry.substr(separator + 1));
        if (key_id.empty() || encoded.empty()) {
            continue;
        }
        if (auto key = decode_public_key_base64(encoded)) {
            keys.push_back(FolderStructurePluginPublicKey{key_id, *key});
        }
    }
    return keys;
}

std::vector<std::string> guidance_lines_from_value(const QJsonValue& value)
{
    std::vector<std::string> lines;
    if (value.isString()) {
        const std::string line = value.toString().trimmed().toStdString();
        if (!line.empty()) {
            lines.push_back(line);
        }
        return lines;
    }
    if (!value.isArray()) {
        return lines;
    }

    const QJsonArray array = value.toArray();
    lines.reserve(static_cast<std::size_t>(array.size()));
    for (const auto& entry : array) {
        if (!entry.isString()) {
            continue;
        }
        const std::string line = entry.toString().trimmed().toStdString();
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::string join_guidance(const std::vector<std::string>& lines)
{
    std::ostringstream out;
    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        out << "- " << line << "\n";
    }
    return out.str();
}

std::vector<std::string> parse_safe_relative_folder_paths(const QJsonObject& object,
                                                          std::initializer_list<const char*> keys,
                                                          std::string* error)
{
    std::vector<std::string> paths;
    for (const auto* key : keys) {
        paths = parse_string_list_field(object, key, {});
        if (!paths.empty()) {
            break;
        }
    }

    std::vector<std::string> valid_paths;
    valid_paths.reserve(paths.size());
    for (const auto& path : paths) {
        const auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
        if (!validation.valid) {
            if (error) {
                *error = "Folder-structure plugin profile contains an invalid template directory.";
            }
            return {};
        }
        valid_paths.push_back(validation.normalized_path);
    }
    return valid_paths;
}

} // namespace

std::vector<FolderStructurePluginPublicKey> default_folder_structure_plugin_public_keys()
{
    return parse_public_key_list(AIFS_FOLDER_STRUCTURE_PLUGIN_PUBLIC_KEYS);
}

std::string folder_structure_plugin_current_platform()
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

std::string folder_structure_plugin_current_architecture()
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

bool folder_structure_plugin_manifest_matches_current_runtime(
    const FolderStructurePluginManifest& manifest,
    std::string* error)
{
    if (!list_matches_value(manifest.platforms, folder_structure_plugin_current_platform())) {
        if (error) {
            *error = "Plugin targets a different platform.";
        }
        return false;
    }
    if (!list_matches_value(manifest.architectures, folder_structure_plugin_current_architecture())) {
        if (error) {
            *error = "Plugin targets a different CPU architecture.";
        }
        return false;
    }
    return true;
}

std::optional<FolderStructurePluginManifest> load_folder_structure_plugin_manifest_from_file(
    const std::filesystem::path& manifest_path,
    std::string* error)
{
    auto object = read_json_object(manifest_path, error);
    if (!object) {
        return std::nullopt;
    }

    FolderStructurePluginManifest manifest;
    manifest.id = object->value("id").toString().trimmed().toStdString();
    manifest.name = object->value("name").toString().trimmed().toStdString();
    manifest.description = object->value("description").toString().trimmed().toStdString();
    manifest.version = object->value("version").toString().trimmed().toStdString();
    manifest.entry_point_kind =
        object->value("entry_point_kind").toString().trimmed().toStdString();
    manifest.profile_paths =
        parse_string_list_field(*object, "profile_paths", {"profile_path"});
    manifest.platforms =
        parse_string_list_field(*object, "platforms", {"platform"}, normalize_platform_name);
    manifest.architectures =
        parse_string_list_field(*object, "architectures", {"architecture", "arch"},
                                normalize_architecture_name);
    manifest.source_path = manifest_path;

    if (manifest.id.empty() || manifest.name.empty() || manifest.version.empty() ||
        manifest.entry_point_kind.empty() || manifest.profile_paths.empty()) {
        if (error) {
            *error = "Folder-structure plugin manifest is missing required fields.";
        }
        return std::nullopt;
    }
    if (manifest.entry_point_kind != kFolderStructureEntryPointKind) {
        if (error) {
            *error = "Folder-structure plugin manifest must use folder_structure_profile entry point kind.";
        }
        return std::nullopt;
    }
    for (const auto& profile_path : manifest.profile_paths) {
        if (!safe_relative_package_path(profile_path)) {
            if (error) {
                *error = "Folder-structure plugin manifest contains an invalid profile path.";
            }
            return std::nullopt;
        }
    }
    if (!folder_structure_plugin_manifest_matches_current_runtime(manifest, error)) {
        return std::nullopt;
    }
    return manifest;
}

std::optional<FolderStructurePluginProfile> load_folder_structure_plugin_profile_from_file(
    const std::filesystem::path& profile_path,
    const FolderStructurePluginManifest& fallback_manifest,
    std::string* error)
{
    auto object = read_json_object(profile_path, error);
    if (!object) {
        return std::nullopt;
    }

    const QJsonObject recognition = object->value("recognition").toObject();

    FolderStructurePluginProfile profile;
    profile.id = object->value("id").toString().trimmed().toStdString();
    profile.name = object->value("name").toString().trimmed().toStdString();
    profile.description = object->value("description").toString().trimmed().toStdString();
    profile.structure_kind = object->value("structure_kind").toString().trimmed().toStdString();
    if (profile.structure_kind.empty()) {
        profile.structure_kind = recognition.value("structure_kind").toString().trimmed().toStdString();
    }
    if (profile.structure_kind.empty()) {
        profile.structure_kind = recognition.value("kind").toString().trimmed().toStdString();
    }
    profile.available = !object->contains("available") ||
                        object->value("available").toBool(true);
    profile.detectors = parse_string_list_field(*object, "detectors", {"detector"});
    const auto recognition_detectors =
        parse_string_list_field(recognition, "detectors", {"detector"});
    profile.detectors.insert(profile.detectors.end(),
                             recognition_detectors.begin(),
                             recognition_detectors.end());
    std::sort(profile.detectors.begin(), profile.detectors.end());
    profile.detectors.erase(std::unique(profile.detectors.begin(), profile.detectors.end()),
                            profile.detectors.end());
    profile.initial_directories =
        parse_safe_relative_folder_paths(*object,
                                         {"initial_directories",
                                          "template_directories",
                                          "relative_directories"},
                                         error);
    if (error && !error->empty() && profile.initial_directories.empty()) {
        return std::nullopt;
    }

    profile.prompt_guidance = join_guidance(guidance_lines_from_value(object->value("prompt_guidance")));
    profile.new_folder_guidance =
        join_guidance(guidance_lines_from_value(object->value("new_folder_guidance")));
    if (profile.prompt_guidance.empty()) {
        profile.prompt_guidance =
            join_guidance(guidance_lines_from_value(recognition.value("prompt_guidance")));
    }
    if (profile.new_folder_guidance.empty()) {
        profile.new_folder_guidance =
            join_guidance(guidance_lines_from_value(recognition.value("new_folder_guidance")));
    }
    profile.source_path = profile_path;

    if (profile.id.empty()) {
        profile.id = fallback_manifest.id;
    }
    if (profile.name.empty()) {
        profile.name = fallback_manifest.name;
    }
    if (profile.description.empty()) {
        profile.description = fallback_manifest.description;
    }
    if (profile.structure_kind.empty()) {
        if (error) {
            *error = "Folder-structure plugin profile is missing structure_kind.";
        }
        return std::nullopt;
    }
    if (profile.name.empty() || profile.description.empty()) {
        if (error) {
            *error = "Folder-structure plugin profile is missing user-facing metadata.";
        }
        return std::nullopt;
    }
    if (profile.available && profile.initial_directories.empty()) {
        if (error) {
            *error = "Available folder-structure plugin profiles must include template directories.";
        }
        return std::nullopt;
    }
    return profile;
}

std::vector<FolderStructurePluginProfile> load_folder_structure_plugin_profiles(
    const FolderStructurePluginManifest& manifest,
    std::string* error)
{
    std::vector<FolderStructurePluginProfile> profiles;
    const std::filesystem::path package_root = manifest.source_path.parent_path();
    if (package_root.empty()) {
        if (error) {
            *error = "Folder-structure plugin manifest has no package root.";
        }
        return profiles;
    }

    profiles.reserve(manifest.profile_paths.size());
    for (const auto& profile_path_text : manifest.profile_paths) {
        const auto relative = safe_relative_package_path(profile_path_text);
        if (!relative) {
            if (error) {
                *error = "Folder-structure plugin manifest contains an invalid profile path.";
            }
            return {};
        }

        std::string profile_error;
        auto profile =
            load_folder_structure_plugin_profile_from_file(package_root / *relative,
                                                           manifest,
                                                           &profile_error);
        if (!profile) {
            if (error) {
                *error = profile_error.empty()
                    ? "Failed to load folder-structure plugin profile."
                    : profile_error;
            }
            return {};
        }
        profiles.push_back(std::move(*profile));
    }
    return profiles;
}

bool verify_folder_structure_plugin_package(
    const std::filesystem::path& package_root,
    const std::vector<FolderStructurePluginPublicKey>& trusted_keys,
    std::string* signer_key_id,
    std::string* error)
{
    if (trusted_keys.empty()) {
        if (error) {
            *error = "No trusted folder-structure plugin signing keys are configured.";
        }
        return false;
    }

    const std::filesystem::path signature_manifest_path = package_root / kSignatureManifestName;
    const std::filesystem::path signature_path = package_root / kSignatureFileName;
    std::string read_error;
    const QByteArray signed_payload = read_file_bytes(signature_manifest_path, &read_error);
    if (signed_payload.isEmpty() && !read_error.empty()) {
        if (error) {
            *error = "Failed to read folder-structure plugin signature manifest.";
        }
        return false;
    }
    const QByteArray signature =
        decode_base64_or_raw_signature(read_file_bytes(signature_path, &read_error));
    if (signature.size() != 64) {
        if (error) {
            *error = "Folder-structure plugin signature is missing or invalid.";
        }
        return false;
    }

    const QJsonDocument signature_doc = QJsonDocument::fromJson(signed_payload);
    if (!signature_doc.isObject()) {
        if (error) {
            *error = "Folder-structure plugin signature manifest is invalid JSON.";
        }
        return false;
    }
    const QJsonObject signature_root = signature_doc.object();
    const std::string algorithm =
        ascii_lower_copy(signature_root.value("algorithm").toString().trimmed().toStdString());
    const std::string key_id = signature_root.value("key_id").toString().trimmed().toStdString();
    if (algorithm != "ed25519" || key_id.empty()) {
        if (error) {
            *error = "Folder-structure plugin signature manifest has unsupported metadata.";
        }
        return false;
    }

    const auto trusted_key = trusted_key_for_id(trusted_keys, key_id);
    if (!trusted_key) {
        if (error) {
            *error = "Folder-structure plugin was signed by an untrusted key.";
        }
        return false;
    }
    if (!verify_ed25519_signature(signed_payload, signature, *trusted_key)) {
        if (error) {
            *error = "Folder-structure plugin signature verification failed.";
        }
        return false;
    }

    const QJsonArray files = signature_root.value("files").toArray();
    if (files.isEmpty()) {
        if (error) {
            *error = "Folder-structure plugin signature manifest contains no file hashes.";
        }
        return false;
    }

    std::map<std::string, std::string> expected_hashes;
    for (const auto& value : files) {
        const QJsonObject file = value.toObject();
        const std::string path_text = file.value("path").toString().trimmed().toStdString();
        const auto relative_path = safe_relative_package_path(path_text);
        const std::string hash = normalize_sha256(file.value("sha256").toString().toStdString());
        if (!relative_path || !is_sha256_hex(hash)) {
            if (error) {
                *error = "Folder-structure plugin signature manifest contains an invalid file hash entry.";
            }
            return false;
        }
        const std::string normalized_path = generic_relative_path_text(*relative_path);
        if (normalized_path == kSignatureManifestName || normalized_path == kSignatureFileName) {
            if (error) {
                *error = "Folder-structure plugin signature cannot list signature files as payload.";
            }
            return false;
        }
        if (is_executable_like_package_path(normalized_path)) {
            if (error) {
                *error = "Folder-structure plugin packages cannot contain executable payload files.";
            }
            return false;
        }
        expected_hashes[normalized_path] = hash;
    }

    if (!expected_hashes.contains("manifest.json")) {
        if (error) {
            *error = "Folder-structure plugin signature must cover manifest.json.";
        }
        return false;
    }

    std::set<std::string> actual_payload_paths;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(package_root, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file(ec) || ec) {
            continue;
        }
        const std::string relative = relative_path_text(package_root, it->path());
        if (relative.empty() || relative == kSignatureManifestName || relative == kSignatureFileName) {
            continue;
        }
        if (is_executable_like_package_path(relative)) {
            if (error) {
                *error = "Folder-structure plugin packages cannot contain executable payload files.";
            }
            return false;
        }
        actual_payload_paths.insert(relative);
    }
    if (ec) {
        if (error) {
            *error = "Failed to inspect folder-structure plugin package: " + ec.message();
        }
        return false;
    }

    for (const auto& actual : actual_payload_paths) {
        if (!expected_hashes.contains(actual)) {
            if (error) {
                *error = "Folder-structure plugin contains an unsigned payload file.";
            }
            return false;
        }
    }

    for (const auto& [relative, expected_hash] : expected_hashes) {
        if (!actual_payload_paths.contains(relative)) {
            if (error) {
                *error = "Folder-structure plugin signature references a missing payload file.";
            }
            return false;
        }

        try {
            if (compute_sha256(package_root / Utils::utf8_to_path(relative)) != expected_hash) {
                if (error) {
                    *error = "Folder-structure plugin payload hash verification failed.";
                }
                return false;
            }
        } catch (const std::exception& ex) {
            if (error) {
                *error = ex.what();
            }
            return false;
        }
    }

    if (signer_key_id) {
        *signer_key_id = key_id;
    }
    return true;
}
