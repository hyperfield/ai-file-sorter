#include "SupportCodeManager.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QString>
#include <QSysInfo>

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

constexpr std::uint32_t kBlobMagic = 0xA15F50C2u;
constexpr std::uint32_t kBlobVersion = 2u;
constexpr std::size_t kSaltSize = 16u;
constexpr std::size_t kHashSize = 32u;
constexpr std::size_t kBlobSize = 120u;
constexpr std::size_t kSignatureSize = 64u;
constexpr char kCodePrefix[] = "AIFS1";
constexpr char kPayloadPrefix[] = "aifs-support:v1:";

// This must match the private key held by the website-side signer.
constexpr std::array<unsigned char, 32> kVerificationPublicKey{
    0xd4, 0x56, 0x9d, 0x00, 0x24, 0x50, 0xf5, 0xa2,
    0x90, 0x94, 0xff, 0x0f, 0xf8, 0xee, 0xcc, 0x7b,
    0x4f, 0xfa, 0x05, 0x2a, 0xf8, 0x35, 0x37, 0xba,
    0x4e, 0xde, 0x3c, 0xc5, 0x16, 0xf5, 0x66, 0xe1,
};

constexpr std::array<unsigned char, 32> kBlobPepper{
    0x6f, 0x24, 0x94, 0x11, 0x5d, 0xca, 0x37, 0xa8,
    0x7b, 0xe0, 0x12, 0x49, 0xf6, 0x9d, 0x58, 0x03,
    0xc4, 0x71, 0x2e, 0xb9, 0x8a, 0x16, 0xdd, 0x60,
    0x34, 0xf3, 0x8c, 0x27, 0x90, 0x4a, 0xbe, 0x15,
};

#ifdef AI_FILE_SORTER_TEST_BUILD
constexpr char kTestPayload[] = "aifs-support:v1:test-build";
#endif

std::string trim_ascii(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

QByteArray appendable_base64url(std::string_view segment) {
    if (segment.empty()) {
        return {};
    }

    QByteArray encoded(segment.data(), static_cast<qsizetype>(segment.size()));
    const int remainder = encoded.size() % 4;
    if (remainder != 0) {
        encoded.append(QByteArray(4 - remainder, '='));
    }
    return encoded;
}

QByteArray decode_base64url(std::string_view segment) {
    if (segment.empty()) {
        return {};
    }

    return QByteArray::fromBase64(
        appendable_base64url(segment),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
}

bool is_supported_payload(const QByteArray& payload) {
    if (!payload.startsWith(kPayloadPrefix)) {
        return false;
    }

    if (payload.size() <= static_cast<qsizetype>(std::char_traits<char>::length(kPayloadPrefix)) ||
        payload.size() > 160) {
        return false;
    }

    for (int i = static_cast<int>(std::char_traits<char>::length(kPayloadPrefix)); i < payload.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(payload.at(i));
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' ||
            ch == '_' ||
            ch == ':' ||
            ch == '.') {
            continue;
        }
        return false;
    }

    return true;
}

bool verify_signature(const QByteArray& payload, const QByteArray& signature) {
    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519,
        nullptr,
        kVerificationPublicKey.data(),
        kVerificationPublicKey.size());
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

QByteArray to_byte_array(std::uint32_t value) {
    QByteArray bytes(4, Qt::Uninitialized);
    bytes[0] = static_cast<char>(value & 0xffu);
    bytes[1] = static_cast<char>((value >> 8) & 0xffu);
    bytes[2] = static_cast<char>((value >> 16) & 0xffu);
    bytes[3] = static_cast<char>((value >> 24) & 0xffu);
    return bytes;
}

std::uint32_t from_little_endian_u32(const unsigned char* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::uint32_t leading_u32(const QByteArray& value) {
    if (value.size() < 4) {
        return 0;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(value.constData());
    return from_little_endian_u32(data);
}

QByteArray sha256_labeled(std::string_view label, std::initializer_list<QByteArray> segments) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(label.data(), static_cast<qsizetype>(label.size()));
    hash.addData(reinterpret_cast<const char*>(kBlobPepper.data()),
                 static_cast<qsizetype>(kBlobPepper.size()));
    for (const QByteArray& segment : segments) {
        hash.addData(segment);
    }
    return hash.result();
}

std::array<unsigned char, kSaltSize> random_salt() {
    std::array<unsigned char, kSaltSize> salt{};
    for (std::size_t i = 0; i < salt.size(); i += sizeof(quint32)) {
        const quint32 word = QRandomGenerator::global()->generate();
        const std::size_t remaining = std::min<std::size_t>(sizeof(word), salt.size() - i);
        for (std::size_t j = 0; j < remaining; ++j) {
            salt[i + j] = static_cast<unsigned char>((word >> (j * 8)) & 0xffu);
        }
    }
    return salt;
}

QByteArray to_byte_array(const std::array<unsigned char, kSaltSize>& value) {
    return QByteArray(reinterpret_cast<const char*>(value.data()),
                      static_cast<qsizetype>(value.size()));
}

std::filesystem::path blob_filename() {
    return "support_prompt_state.bin";
}

} // namespace

SupportCodeManager::SupportCodeManager(std::filesystem::path config_dir)
    : config_dir_(std::move(config_dir)) {}

bool SupportCodeManager::is_valid_code(const std::string& code) {
    return decode_payload(code).has_value();
}

bool SupportCodeManager::redeem_code(const std::string& code) const {
    const auto payload = decode_payload(code);
    if (!payload.has_value()) {
        return false;
    }

    return write_state(*payload);
}

bool SupportCodeManager::is_prompt_permanently_disabled() const {
    std::ifstream input(storage_path(), std::ios::binary);
    if (!input) {
        return false;
    }

    std::array<unsigned char, kBlobSize> blob{};
    input.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(blob.size())) {
        return false;
    }

    const std::uint32_t stored_magic = from_little_endian_u32(blob.data());
    const std::uint32_t stored_version = from_little_endian_u32(blob.data() + 4);
    const QByteArray salt(reinterpret_cast<const char*>(blob.data() + 8),
                          static_cast<qsizetype>(kSaltSize));
    const QByteArray payload_hash(reinterpret_cast<const char*>(blob.data() + 24),
                                  static_cast<qsizetype>(kHashSize));
    const QByteArray machine_hash(reinterpret_cast<const char*>(blob.data() + 56),
                                  static_cast<qsizetype>(kHashSize));
    const QByteArray checksum(reinterpret_cast<const char*>(blob.data() + 88),
                              static_cast<qsizetype>(kHashSize));

    const QByteArray expected_machine_hash = sha256_labeled(
        "aifs/support/machine/v2",
        {salt, QByteArray::fromStdString(machine_binding_key())});
    if (machine_hash != expected_machine_hash) {
        return false;
    }

    if (stored_magic != (kBlobMagic ^ leading_u32(payload_hash)) ||
        stored_version != (kBlobVersion ^ leading_u32(machine_hash))) {
        return false;
    }

    const QByteArray expected_checksum = sha256_labeled(
        "aifs/support/blob/v2",
        {to_byte_array(stored_magic), to_byte_array(stored_version), salt, payload_hash, machine_hash});

    return checksum == expected_checksum;
}

#ifdef AI_FILE_SORTER_TEST_BUILD
bool SupportCodeManager::force_disable_prompt_for_testing() const {
    return write_state(kTestPayload);
}
#endif

std::optional<std::string> SupportCodeManager::decode_payload(const std::string& code) {
    const std::string trimmed = trim_ascii(code);
    if (trimmed.empty() || trimmed.size() > 512) {
        return std::nullopt;
    }

    const std::size_t first_dot = trimmed.find('.');
    if (first_dot == std::string::npos || trimmed.substr(0, first_dot) != kCodePrefix) {
        return std::nullopt;
    }

    const std::size_t second_dot = trimmed.find('.', first_dot + 1);
    if (second_dot == std::string::npos || trimmed.find('.', second_dot + 1) != std::string::npos) {
        return std::nullopt;
    }

    const std::string_view payload_segment(trimmed.data() + first_dot + 1,
                                           second_dot - first_dot - 1);
    const std::string_view signature_segment(trimmed.data() + second_dot + 1,
                                             trimmed.size() - second_dot - 1);
    if (payload_segment.empty() || signature_segment.empty()) {
        return std::nullopt;
    }

    const QByteArray payload = decode_base64url(payload_segment);
    const QByteArray signature = decode_base64url(signature_segment);
    if (payload.isEmpty() ||
        signature.size() != static_cast<qsizetype>(kSignatureSize) ||
        !is_supported_payload(payload) ||
        !verify_signature(payload, signature)) {
        return std::nullopt;
    }

    return std::string(payload.constData(), static_cast<std::size_t>(payload.size()));
}

std::filesystem::path SupportCodeManager::storage_path() const {
    return config_dir_ / blob_filename();
}

std::string SupportCodeManager::machine_binding_key() const {
    QByteArray material;

    auto append = [&material](const QByteArray& value) {
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
    append(QSysInfo::prettyProductName().toUtf8());

    if (material.isEmpty()) {
        material = QByteArray::fromStdString(config_dir_.string());
    }

    return std::string(material.constData(), static_cast<std::size_t>(material.size()));
}

bool SupportCodeManager::write_state(const std::string& payload) const {
    const auto salt = random_salt();
    const QByteArray salt_bytes = to_byte_array(salt);
    const QByteArray payload_hash = sha256_labeled(
        "aifs/support/payload/v2",
        {QByteArray::fromStdString(payload)});
    const QByteArray machine_hash = sha256_labeled(
        "aifs/support/machine/v2",
        {salt_bytes, QByteArray::fromStdString(machine_binding_key())});

    const std::uint32_t stored_magic = kBlobMagic ^ leading_u32(payload_hash);
    const std::uint32_t stored_version = kBlobVersion ^ leading_u32(machine_hash);
    const QByteArray checksum = sha256_labeled(
        "aifs/support/blob/v2",
        {to_byte_array(stored_magic), to_byte_array(stored_version), salt_bytes, payload_hash, machine_hash});

    QByteArray blob;
    blob.reserve(static_cast<qsizetype>(kBlobSize));
    blob.append(to_byte_array(stored_magic));
    blob.append(to_byte_array(stored_version));
    blob.append(salt_bytes);
    blob.append(payload_hash);
    blob.append(machine_hash);
    blob.append(checksum);

    std::error_code ec;
    std::filesystem::create_directories(config_dir_, ec);
    if (ec) {
        return false;
    }

    std::ofstream output(storage_path(), std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output.write(blob.constData(), static_cast<std::streamsize>(blob.size()));
    output.close();
    if (!output) {
        return false;
    }

    std::filesystem::permissions(
        storage_path(),
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace,
        ec);

    return true;
}
