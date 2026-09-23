#include "PluginEntitlementService.hpp"

#include <openssl/evp.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>
#include <QSysInfo>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "Utils.hpp"

namespace {

#ifndef AIFS_PLUGIN_ENTITLEMENT_PUBLIC_KEYS
#define AIFS_PLUGIN_ENTITLEMENT_PUBLIC_KEYS ""
#endif

constexpr char kReceiptVersion[] = "aifs.entitlement.v1";
constexpr char kReceiptAlgorithm[] = "ed25519";

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

QByteArray base64url_encode(const QByteArray& bytes) {
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString latin1_from_bytes(const QByteArray& bytes) {
    return QString::fromLatin1(bytes.constData(), static_cast<qsizetype>(bytes.size()));
}

QByteArray with_base64_padding(QByteArray value) {
    const qsizetype remainder = value.size() % 4;
    if (remainder > 0) {
        value.append(QByteArray(4 - remainder, '='));
    }
    return value;
}

QByteArray base64url_decode(const QJsonValue& value) {
    if (!value.isString()) {
        return {};
    }
    return QByteArray::fromBase64(with_base64_padding(value.toString().trimmed().toUtf8()),
                                  QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
}

bool verify_ed25519_signature(const QByteArray& payload, const QByteArray& signature,
                              const PluginEntitlementPublicKey& trusted_key) {
    if (signature.size() != 64) {
        return false;
    }

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

std::optional<PluginEntitlementPublicKey> trusted_key_for_id(
    const std::vector<PluginEntitlementPublicKey>& trusted_keys, const std::string& key_id) {
    const auto match = std::find_if(trusted_keys.begin(), trusted_keys.end(),
                                    [&](const PluginEntitlementPublicKey& key) { return key.key_id == key_id; });
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

std::vector<PluginEntitlementPublicKey> parse_public_key_list(std::string value) {
    std::vector<PluginEntitlementPublicKey> keys;
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
            keys.push_back(PluginEntitlementPublicKey{key_id, *key});
        }
    }
    return keys;
}

QDateTime parse_utc_datetime(const QString& value) {
    QDateTime parsed = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(value, Qt::ISODate);
    }
    return parsed.isValid() ? parsed.toUTC() : QDateTime();
}

bool receipt_window_is_active(const QJsonObject& payload) {
    const QDateTime offline_until = parse_utc_datetime(payload.value("offline_until").toString().trimmed());
    return offline_until.isValid() && QDateTime::currentDateTimeUtc() <= offline_until;
}

bool receipt_matches_device(const QJsonObject& payload, const std::string& expected_device_id) {
    const std::string receipt_device_id = payload.value("device_id").toString().trimmed().toStdString();
    return !receipt_device_id.empty() && receipt_device_id == expected_device_id;
}

bool receipt_has_active_entitlement(const QJsonObject& payload) {
    const QJsonArray entitlements = payload.value("entitlements").toArray();
    for (const auto& value : entitlements) {
        const QJsonObject entitlement = value.toObject();
        const std::string status = ascii_lower_copy(entitlement.value("status").toString().trimmed().toStdString());
        const std::string product_id = entitlement.value("product_id").toString().trimmed().toStdString();
        if (status == "active" && !product_id.empty()) {
            return true;
        }
    }
    return false;
}

bool receipt_grants(const QJsonObject& payload, std::string_view product_id, std::string_view plugin_id) {
    const QJsonArray entitlements = payload.value("entitlements").toArray();
    for (const auto& value : entitlements) {
        const QJsonObject entitlement = value.toObject();
        const std::string status = ascii_lower_copy(entitlement.value("status").toString().trimmed().toStdString());
        const std::string entitlement_product_id = entitlement.value("product_id").toString().trimmed().toStdString();
        const std::string entitlement_plugin_id = entitlement.value("plugin_id").toString().trimmed().toStdString();
        if (status != "active" || std::string_view(entitlement_product_id) != product_id) {
            continue;
        }
        if (!plugin_id.empty() && std::string_view(entitlement_plugin_id) != plugin_id) {
            continue;
        }
        return true;
    }
    return false;
}

bool development_bypass_grants(std::string_view product_id, std::string_view plugin_id) {
#ifdef AIFS_ENABLE_PLUGIN_ENTITLEMENT_DEV_BYPASS
    const char* raw_value = std::getenv("AI_FILE_SORTER_PLUGIN_ENTITLEMENT_DEV_BYPASS");
    if (!raw_value || raw_value[0] == '\0') {
        return false;
    }

    std::string value(raw_value);
    std::replace(value.begin(), value.end(), ';', ',');

    std::istringstream stream(value);
    std::string entry;
    while (std::getline(stream, entry, ',')) {
        entry = trim_copy(std::move(entry));
        const std::string lower_entry = ascii_lower_copy(entry);
        if (entry == "*" || lower_entry == "1" || lower_entry == "true" || lower_entry == "all") {
            return true;
        }
        if (std::string_view(entry) == product_id) {
            return true;
        }
        if (!plugin_id.empty() && std::string_view(entry) == plugin_id) {
            return true;
        }
    }
#else
    (void) product_id;
    (void) plugin_id;
#endif
    return false;
}

std::optional<QJsonObject> parse_verified_payload(const QByteArray& payload, const QByteArray& signature,
                                                  const std::string& key_id,
                                                  const std::vector<PluginEntitlementPublicKey>& trusted_keys,
                                                  const std::string& expected_device_id, std::string* error) {
    const auto trusted_key = trusted_key_for_id(trusted_keys, key_id);
    if (!trusted_key) {
        if (error) {
            *error = "Entitlement receipt was signed by an untrusted key.";
        }
        return std::nullopt;
    }
    if (!verify_ed25519_signature(payload, signature, *trusted_key)) {
        if (error) {
            *error = "Entitlement receipt signature verification failed.";
        }
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        if (error) {
            *error = "Entitlement receipt payload is invalid JSON.";
        }
        return std::nullopt;
    }

    const QJsonObject object = doc.object();
    if (object.value("version").toString().trimmed() != QString::fromLatin1(kReceiptVersion)) {
        if (error) {
            *error = "Entitlement receipt uses an unsupported version.";
        }
        return std::nullopt;
    }
    if (!receipt_matches_device(object, expected_device_id)) {
        if (error) {
            *error = "Entitlement receipt does not match this device.";
        }
        return std::nullopt;
    }
    if (!receipt_window_is_active(object)) {
        if (error) {
            *error = "Entitlement receipt has expired or is missing an offline window.";
        }
        return std::nullopt;
    }
    if (!receipt_has_active_entitlement(object)) {
        if (error) {
            *error = "Entitlement receipt does not contain an active entitlement.";
        }
        return std::nullopt;
    }
    return object;
}

std::optional<QJsonObject> parse_receipt_wrapper(const QByteArray& receipt_bytes,
                                                 const std::vector<PluginEntitlementPublicKey>& trusted_keys,
                                                 const std::string& expected_device_id) {
    const QJsonDocument doc = QJsonDocument::fromJson(receipt_bytes);
    if (!doc.isObject()) {
        return std::nullopt;
    }

    const QJsonObject wrapper = doc.object();
    const QJsonObject signature_object = wrapper.value("signature").toObject();
    const std::string algorithm =
        ascii_lower_copy(signature_object.value("algorithm").toString().trimmed().toStdString());
    const std::string key_id = signature_object.value("key_id").toString().trimmed().toStdString();
    if (algorithm != kReceiptAlgorithm || key_id.empty()) {
        return std::nullopt;
    }

    const QByteArray payload = base64url_decode(wrapper.value("payload"));
    const QByteArray signature = base64url_decode(signature_object.value("value"));
    if (payload.isEmpty() || signature.size() != 64) {
        return std::nullopt;
    }
    return parse_verified_payload(payload, signature, key_id, trusted_keys, expected_device_id, nullptr);
}

std::string receipt_filename(const QByteArray& payload, const QByteArray& signature, const std::string& key_id) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(payload);
    hash.addData(signature);
    hash.addData(key_id.data(), static_cast<qsizetype>(key_id.size()));
    return hash.result().toHex().left(32).toStdString() + ".json";
}

std::string path_to_stable_utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

QByteArray device_fingerprint_material(const std::filesystem::path& config_dir) {
    QByteArray material;
    const auto append = [&material](const QByteArray& value) {
        if (value.isEmpty()) {
            return;
        }
        if (!material.isEmpty()) {
            material.push_back('\0');
        }
        material.append(value);
    };

    append(QSysInfo::machineUniqueId());
    append(QSysInfo::machineHostName().toUtf8());
    append(QSysInfo::currentCpuArchitecture().toUtf8());
    append(QSysInfo::buildAbi().toUtf8());
    append(QSysInfo::kernelType().toUtf8());
    append(QSysInfo::kernelVersion().toUtf8());

    if (material.isEmpty()) {
        material = QByteArray::fromStdString(path_to_stable_utf8(config_dir));
    }
    return material;
}

}  // namespace

PluginEntitlementService::PluginEntitlementService(std::filesystem::path config_dir,
                                                   std::vector<PluginEntitlementPublicKey> trusted_keys)
    : config_dir_(std::move(config_dir)), trusted_keys_(std::move(trusted_keys)) {}

std::filesystem::path PluginEntitlementService::receipt_directory_for_config_dir(
    const std::filesystem::path& config_dir) {
    return config_dir / "plugins" / "entitlements" / "receipts";
}

std::vector<PluginEntitlementPublicKey> PluginEntitlementService::default_public_keys() {
    return parse_public_key_list(AIFS_PLUGIN_ENTITLEMENT_PUBLIC_KEYS);
}

std::string PluginEntitlementService::device_id() const {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    static constexpr char kLabel[] = "aifs-entitlement-device:v1";
    hash.addData(kLabel, static_cast<qsizetype>(std::char_traits<char>::length(kLabel)));
    hash.addData(device_fingerprint_material(config_dir_));
    return "dev_" + hash.result().toHex().left(32).toStdString();
}

bool PluginEntitlementService::store_receipt(const QByteArray& payload, const QByteArray& signature,
                                             const std::string& key_id, std::string* error) const {
    const auto keys = trusted_keys();
    if (keys.empty()) {
        if (error) {
            *error = "No trusted entitlement receipt keys are configured.";
        }
        return false;
    }
    if (payload.isEmpty() || signature.size() != 64 || key_id.empty()) {
        if (error) {
            *error = "Entitlement receipt is incomplete.";
        }
        return false;
    }

    if (!parse_verified_payload(payload, signature, key_id, keys, device_id(), error)) {
        return false;
    }

    QJsonObject signature_object;
    signature_object.insert("algorithm", QString::fromLatin1(kReceiptAlgorithm));
    signature_object.insert("key_id", QString::fromStdString(key_id));
    signature_object.insert("value", latin1_from_bytes(base64url_encode(signature)));

    QJsonObject wrapper;
    wrapper.insert("schema_version", 1);
    wrapper.insert("payload", latin1_from_bytes(base64url_encode(payload)));
    wrapper.insert("signature", signature_object);

    std::error_code ec;
    std::filesystem::create_directories(receipt_directory(), ec);
    if (ec) {
        if (error) {
            *error = "Failed to create entitlement receipt directory: " + ec.message();
        }
        return false;
    }

    const std::filesystem::path destination =
        receipt_directory() / Utils::utf8_to_path(receipt_filename(payload, signature, key_id));
    QSaveFile file(QString::fromStdString(Utils::path_to_utf8(destination)));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    file.write(QJsonDocument(wrapper).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    return true;
}

bool PluginEntitlementService::has_entitlement(std::string_view product_id, std::string_view plugin_id) const {
    if (product_id.empty()) {
        return false;
    }

    if (development_bypass_grants(product_id, plugin_id)) {
        return true;
    }

    const auto keys = trusted_keys();
    if (keys.empty()) {
        return false;
    }

    const std::filesystem::path receipts = receipt_directory();
    std::error_code ec;
    if (!std::filesystem::is_directory(receipts, ec) || ec) {
        return false;
    }

    const std::string expected_device_id = device_id();
    for (std::filesystem::directory_iterator it(receipts, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec) || ec || it->path().extension() != ".json") {
            continue;
        }

        QFile file(QString::fromStdString(Utils::path_to_utf8(it->path())));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const auto payload = parse_receipt_wrapper(file.readAll(), keys, expected_device_id);
        if (payload && receipt_grants(*payload, product_id, plugin_id)) {
            return true;
        }
    }
    return false;
}

std::vector<PluginEntitlementPublicKey> PluginEntitlementService::trusted_keys() const {
    if (!trusted_keys_.empty()) {
        return trusted_keys_;
    }
    return default_public_keys();
}

std::filesystem::path PluginEntitlementService::receipt_directory() const {
    return receipt_directory_for_config_dir(config_dir_);
}
