#include "StoragePluginPackageSignature.hpp"

#include <openssl/evp.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "Utils.hpp"

namespace {

#ifndef AIFS_STORAGE_PLUGIN_PUBLIC_KEYS
#define AIFS_STORAGE_PLUGIN_PUBLIC_KEYS ""
#endif

constexpr char kSignatureManifestName[] = "plugin-signature.json";
constexpr char kSignatureFileName[] = "plugin-signature.sig";

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string ascii_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::optional<std::filesystem::path> safe_relative_package_path(std::string value) {
    if (value.find('\\') != std::string::npos) {
        return std::nullopt;
    }

    const std::filesystem::path normalized = std::filesystem::path(value).lexically_normal();
    if (normalized.empty() || normalized.has_root_name() || normalized.has_root_directory()) {
        return std::nullopt;
    }

    std::filesystem::path sanitized;
    for (const auto& component : normalized) {
        if (component.empty() || component == "." || component == "..") {
            return std::nullopt;
        }
        sanitized /= component;
    }
    if (sanitized.empty()) {
        return std::nullopt;
    }
    return sanitized;
}

std::string generic_relative_path_text(const std::filesystem::path& path) {
    const auto generic = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(generic.data()), generic.size());
}

std::string relative_path_text(const std::filesystem::path& root, const std::filesystem::path& path) {
    std::error_code ec;
    const auto relative = std::filesystem::relative(path, root, ec);
    if (ec || relative.empty() || relative == ".") {
        return {};
    }
    return generic_relative_path_text(relative);
}

QByteArray read_file_bytes(const std::filesystem::path& path, std::string* error) {
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return {};
    }
    return file.readAll();
}

std::string normalize_sha256(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }),
                value.end());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool is_sha256_hex(const std::string& value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isxdigit(ch) != 0; });
}

std::string compute_sha256(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open signed storage plugin file for SHA-256 verification.");
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1 << 20);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            throw std::runtime_error("Failed while reading signed storage plugin file for SHA-256 verification.");
        }
        hash.addData(chunk);
    }
    return hash.result().toHex().toStdString();
}

QByteArray decode_base64_or_raw_signature(QByteArray signature) {
    if (signature.size() == 64) {
        return signature;
    }

    signature = signature.trimmed();
    if (signature.size() == 64) {
        return signature;
    }

    QByteArray decoded =
        QByteArray::fromBase64(signature, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.size() == 64) {
        return decoded;
    }
    decoded =
        QByteArray::fromBase64(signature, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    return decoded.size() == 64 ? decoded : QByteArray();
}

bool verify_ed25519_signature(const QByteArray& payload, const QByteArray& signature,
                              const StoragePluginPackagePublicKey& trusted_key) {
    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, trusted_key.ed25519_public_key.data(),
                                                trusted_key.ed25519_public_key.size());
    if (!key) {
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(key);
        return false;
    }

    const bool verified = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1 &&
                          EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char*>(signature.constData()),
                                           static_cast<std::size_t>(signature.size()),
                                           reinterpret_cast<const unsigned char*>(payload.constData()),
                                           static_cast<std::size_t>(payload.size())) == 1;

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return verified;
}

std::optional<StoragePluginPackagePublicKey> trusted_key_for_id(
    const std::vector<StoragePluginPackagePublicKey>& trusted_keys, const std::string& key_id) {
    const auto match =
        std::find_if(trusted_keys.begin(), trusted_keys.end(), [&](const auto& key) { return key.key_id == key_id; });
    if (match == trusted_keys.end()) {
        return std::nullopt;
    }
    return *match;
}

std::optional<std::array<unsigned char, 32>> decode_public_key_base64(const std::string& encoded) {
    QByteArray decoded = QByteArray::fromBase64(QByteArray::fromStdString(encoded),
                                                QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.size() != 32) {
        decoded = QByteArray::fromBase64(QByteArray::fromStdString(encoded),
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

std::vector<StoragePluginPackagePublicKey> parse_public_key_list(std::string value) {
    std::vector<StoragePluginPackagePublicKey> keys;
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
            keys.push_back(StoragePluginPackagePublicKey{key_id, *key});
        }
    }
    return keys;
}

bool append_actual_payload_paths(const std::filesystem::path& package_root, std::set<std::string>* actual_payload_paths,
                                 std::string* error) {
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(package_root, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec) || ec) {
            continue;
        }
        const std::string relative = relative_path_text(package_root, it->path());
        if (relative.empty() || relative == kSignatureManifestName || relative == kSignatureFileName) {
            continue;
        }
        if (!safe_relative_package_path(relative)) {
            if (error) {
                *error = "Storage plugin package contains an unsafe payload path.";
            }
            return false;
        }
        actual_payload_paths->insert(relative);
    }
    if (ec) {
        if (error) {
            *error = "Failed to inspect storage plugin package: " + ec.message();
        }
        return false;
    }
    return true;
}

}  // namespace

std::vector<StoragePluginPackagePublicKey> default_storage_plugin_package_public_keys() {
    return parse_public_key_list(AIFS_STORAGE_PLUGIN_PUBLIC_KEYS);
}

bool verify_storage_plugin_package(const std::filesystem::path& package_root,
                                   const std::vector<StoragePluginPackagePublicKey>& trusted_keys,
                                   StoragePluginPackageVerification* verification, std::string* error) {
    if (trusted_keys.empty()) {
        if (error) {
            *error = "No trusted storage plugin signing keys are configured.";
        }
        return false;
    }

    const std::filesystem::path signature_manifest_path = package_root / kSignatureManifestName;
    const std::filesystem::path signature_path = package_root / kSignatureFileName;

    std::string read_error;
    const QByteArray signed_payload = read_file_bytes(signature_manifest_path, &read_error);
    if (signed_payload.isEmpty() && !read_error.empty()) {
        if (error) {
            *error = "Storage plugin package is missing signature metadata.";
        }
        return false;
    }
    const QByteArray signature = decode_base64_or_raw_signature(read_file_bytes(signature_path, &read_error));
    if (signature.size() != 64) {
        if (error) {
            *error = "Storage plugin package signature is missing or invalid.";
        }
        return false;
    }

    const QJsonDocument signature_doc = QJsonDocument::fromJson(signed_payload);
    if (!signature_doc.isObject()) {
        if (error) {
            *error = "Storage plugin signature manifest is invalid JSON.";
        }
        return false;
    }

    const QJsonObject signature_root = signature_doc.object();
    const std::string algorithm =
        ascii_lower_copy(signature_root.value("algorithm").toString().trimmed().toStdString());
    const std::string key_id = signature_root.value("key_id").toString().trimmed().toStdString();
    if (algorithm != "ed25519" || key_id.empty()) {
        if (error) {
            *error = "Storage plugin signature manifest has unsupported metadata.";
        }
        return false;
    }

    const auto trusted_key = trusted_key_for_id(trusted_keys, key_id);
    if (!trusted_key) {
        if (error) {
            *error = "Storage plugin package was signed by an untrusted key.";
        }
        return false;
    }
    if (!verify_ed25519_signature(signed_payload, signature, *trusted_key)) {
        if (error) {
            *error = "Storage plugin package signature verification failed.";
        }
        return false;
    }

    const QJsonArray files = signature_root.value("files").toArray();
    if (files.isEmpty()) {
        if (error) {
            *error = "Storage plugin signature manifest contains no file hashes.";
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
                *error = "Storage plugin signature manifest contains an invalid file hash entry.";
            }
            return false;
        }

        const std::string normalized_path = generic_relative_path_text(*relative_path);
        if (normalized_path == kSignatureManifestName || normalized_path == kSignatureFileName) {
            if (error) {
                *error = "Storage plugin signature cannot list signature files as payload.";
            }
            return false;
        }
        if (!expected_hashes.emplace(normalized_path, hash).second) {
            if (error) {
                *error = "Storage plugin signature manifest contains duplicate file entries.";
            }
            return false;
        }
    }

    if (!expected_hashes.contains("manifest.json")) {
        if (error) {
            *error = "Storage plugin signature must cover manifest.json.";
        }
        return false;
    }

    std::set<std::string> actual_payload_paths;
    if (!append_actual_payload_paths(package_root, &actual_payload_paths, error)) {
        return false;
    }

    for (const auto& actual : actual_payload_paths) {
        if (!expected_hashes.contains(actual)) {
            if (error) {
                *error = "Storage plugin package contains an unsigned payload file.";
            }
            return false;
        }
    }

    for (const auto& [relative, expected_hash] : expected_hashes) {
        if (!actual_payload_paths.contains(relative)) {
            if (error) {
                *error = "Storage plugin signature references a missing payload file.";
            }
            return false;
        }

        try {
            if (compute_sha256(package_root / Utils::utf8_to_path(relative)) != expected_hash) {
                if (error) {
                    *error = "Storage plugin payload hash verification failed.";
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

    if (verification) {
        verification->signer_key_id = key_id;
        verification->signed_payload_paths.clear();
        for (const auto& [relative, hash] : expected_hashes) {
            (void) hash;
            verification->signed_payload_paths.insert(relative);
        }
    }
    return true;
}
