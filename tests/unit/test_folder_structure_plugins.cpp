#include <catch2/catch_test_macros.hpp>

#include "FolderStructurePluginManager.hpp"
#include "FolderStructurePluginProfile.hpp"
#include "FolderStructureTemplates.hpp"
#include "FolderStructurePattern.hpp"
#include "FolderTreeCatalog.hpp"
#include "TestHelpers.hpp"

#include <QByteArray>
#include <QCryptographicHash>

#include <openssl/evp.h>

#include <zip.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::array<unsigned char, 32> kTestPrivateKey{
    0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
    0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
    0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
    0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60};

constexpr std::array<unsigned char, 32> kTestPublicKey{
    0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
    0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
    0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
    0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a};

constexpr char kTestKeyId[] = "test-folder-structure-key";

struct ArchiveEntry {
    std::string name;
    QByteArray payload;
};

FolderStructurePluginPublicKey test_public_key()
{
    return FolderStructurePluginPublicKey{kTestKeyId, kTestPublicKey};
}

std::string sha256_hex(const QByteArray& payload)
{
    return QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex().toStdString();
}

QByteArray sign_ed25519(const QByteArray& payload)
{
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519,
        nullptr,
        kTestPrivateKey.data(),
        kTestPrivateKey.size());
    REQUIRE(key != nullptr);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    REQUIRE(ctx != nullptr);

    QByteArray signature;
    signature.resize(64);
    std::size_t signature_size = static_cast<std::size_t>(signature.size());
    REQUIRE(EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1);
    REQUIRE(EVP_DigestSign(ctx,
                           reinterpret_cast<unsigned char*>(signature.data()),
                           &signature_size,
                           reinterpret_cast<const unsigned char*>(payload.constData()),
                           static_cast<std::size_t>(payload.size())) == 1);
    signature.resize(static_cast<qsizetype>(signature_size));

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return signature;
}

void create_zip_archive(const std::filesystem::path& archive_path,
                        const std::vector<ArchiveEntry>& entries)
{
    int error_code = 0;
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error_code);
    REQUIRE(archive != nullptr);

    for (const auto& entry : entries) {
        zip_source_t* source = zip_source_buffer(archive,
                                                 entry.payload.constData(),
                                                 static_cast<zip_uint64_t>(entry.payload.size()),
                                                 0);
        REQUIRE(source != nullptr);
        const zip_int64_t index = zip_file_add(archive,
                                               entry.name.c_str(),
                                               source,
                                               ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        REQUIRE(index >= 0);
    }

    REQUIRE(zip_close(archive) == 0);
}

std::vector<ArchiveEntry> signed_plugin_entries(
    const std::string& manifest,
    const std::string& profile,
    const std::vector<ArchiveEntry>& extra_entries = {})
{
    std::vector<std::pair<std::string, QByteArray>> signed_files{
        {"manifest.json", QByteArray::fromStdString(manifest)},
        {"profiles/johnny.json", QByteArray::fromStdString(profile)}};
    for (const auto& extra : extra_entries) {
        signed_files.push_back({extra.name, extra.payload});
    }

    std::string signature_manifest =
        std::string("{\n") +
        "  \"schema_version\": 1,\n" +
        "  \"algorithm\": \"ed25519\",\n" +
        "  \"key_id\": \"" + kTestKeyId + "\",\n" +
        "  \"files\": [\n";
    for (std::size_t index = 0; index < signed_files.size(); ++index) {
        const auto& [path, payload] = signed_files[index];
        signature_manifest +=
            "    {\"path\":\"" + path + "\",\"sha256\":\"" + sha256_hex(payload) + "\"}";
        signature_manifest += index + 1 == signed_files.size() ? "\n" : ",\n";
    }
    signature_manifest += "  ]\n}\n";

    const QByteArray signature_payload = QByteArray::fromStdString(signature_manifest);
    std::vector<ArchiveEntry> entries{
        {"johnny/manifest.json", QByteArray::fromStdString(manifest)},
        {"johnny/profiles/johnny.json", QByteArray::fromStdString(profile)},
        {"johnny/plugin-signature.json", signature_payload},
        {"johnny/plugin-signature.sig", sign_ed25519(signature_payload)}};
    for (const auto& extra : extra_entries) {
        entries.push_back({"johnny/" + extra.name, extra.payload});
    }
    return entries;
}

std::string plugin_manifest()
{
    return std::string(R"json({
  "id": "johnny_decimal_support",
  "name": "Johnny.Decimal Support",
  "description": "Adds Johnny.Decimal structure guidance.",
  "version": "1.0.0",
  "entry_point_kind": "folder_structure_profile",
  "platforms": [")json") +
           folder_structure_plugin_current_platform() +
           R"json("],
  "architectures": [")json" +
           folder_structure_plugin_current_architecture() +
           R"json("],
  "profile_paths": ["profiles/johnny.json"]
})json";
}

std::string plugin_profile()
{
    return R"json({
  "id": "johnny_decimal_profile",
  "name": "Johnny.Decimal Complete",
  "description": "Templates and routing rules for a Johnny.Decimal archive.",
  "structure_kind": "johnny_decimal",
  "detectors": ["johnny_decimal_like"],
  "initial_directories": [
    "00-09 System",
    "00-09 System/01 Index",
    "10-19 Admin",
    "10-19 Admin/11 Finance"
  ],
  "prompt_guidance": [
    "Keep Johnny.Decimal IDs attached to folder names when routing into an existing archive."
  ],
  "new_folder_guidance": [
    "When creating a child folder, use the next free number inside the parent range."
  ]
})json";
}

} // namespace

TEST_CASE("FolderStructurePluginManager installs signed declarative plugins")
{
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "johnny-decimal.aifsplugin";
    create_zip_archive(archive_path, signed_plugin_entries(plugin_manifest(), plugin_profile()));

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()});

    std::string installed_plugin_id;
    std::string error;
    REQUIRE(manager.install_from_archive(archive_path, &installed_plugin_id, &error));
    CHECK(installed_plugin_id == "johnny_decimal_support");

    const auto plugins = manager.installed_plugins();
    REQUIRE(plugins.size() == 1);
    CHECK(plugins.front().verified_signer_key_id == kTestKeyId);

    const auto profiles = manager.installed_profiles();
    REQUIRE(profiles.size() == 1);
    CHECK(profiles.front().id == "johnny_decimal_profile");
    CHECK(profiles.front().structure_kind == "johnny_decimal");
    CHECK(profiles.front().prompt_guidance.find("Johnny.Decimal IDs") != std::string::npos);

    const auto descriptors = FolderStructureTemplates::all_with_plugins(profiles);
    const auto plugin_descriptor = std::find_if(
        descriptors.begin(),
        descriptors.end(),
        [](const FolderStructureTemplates::Descriptor& descriptor) {
            return descriptor.plugin_profile_id == "johnny_decimal_profile";
        });
    REQUIRE(plugin_descriptor != descriptors.end());

    TempDir output_dir;
    const auto creation = FolderStructureTemplates::create(output_dir.path(), *plugin_descriptor);
    REQUIRE(creation.success);
    CHECK(std::filesystem::is_directory(output_dir.path() / "00-09 System" / "01 Index"));

    FolderTreeCatalog::Catalog catalog({
        {"10-19 Admin", 1},
        {"10-19 Admin/11 Finance", 2},
        {"20-29 Work", 1},
        {"20-29 Work/21 Projects", 2},
    });
    const auto inferred = FolderStructurePattern::infer_profile(catalog, profiles);
    CHECK(inferred.has_recognized_conventions);
    CHECK(inferred.prompt_guidance.find("Installed folder-structure plugin guidance") != std::string::npos);
    CHECK(inferred.prompt_guidance.find("Matched profile: Johnny.Decimal Complete") != std::string::npos);
}

TEST_CASE("FolderStructurePluginManager rejects tampered plugin payloads")
{
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "tampered.aifsplugin";
    auto entries = signed_plugin_entries(plugin_manifest(), plugin_profile());
    for (auto& entry : entries) {
        if (entry.name == "johnny/profiles/johnny.json") {
            entry.payload = QByteArray::fromStdString(plugin_profile() + "\n ");
        }
    }
    create_zip_archive(archive_path, entries);

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()});

    std::string error;
    CHECK_FALSE(manager.install_from_archive(archive_path, nullptr, &error));
    CHECK(error.find("hash") != std::string::npos);
    CHECK(manager.installed_plugins().empty());
}

TEST_CASE("FolderStructurePluginManager rejects executable payload files")
{
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "executable-payload.aifsplugin";
    const std::vector<ArchiveEntry> extra_entries{
        {"bin/helper.exe", QByteArray("not a real executable")}};
    create_zip_archive(archive_path,
                       signed_plugin_entries(plugin_manifest(), plugin_profile(), extra_entries));

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()});

    std::string error;
    CHECK_FALSE(manager.install_from_archive(archive_path, nullptr, &error));
    CHECK(error.find("executable") != std::string::npos);
    CHECK(manager.installed_plugins().empty());
}
