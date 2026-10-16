#include <openssl/evp.h>
#include <zip.h>

#include <QAbstractItemView>
#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <algorithm>
#include <array>
#include <chrono>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "FolderStructurePattern.hpp"
#include "FolderStructurePluginCatalog.hpp"
#include "FolderStructurePluginDialog.hpp"
#include "FolderStructurePluginManager.hpp"
#include "FolderStructurePluginProfile.hpp"
#include "FolderStructureTemplates.hpp"
#include "FolderTreeCatalog.hpp"
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "PluginArchiveExtractor.hpp"
#include "PluginBuildConfig.hpp"
#include "PluginEntitlementService.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"

namespace {

constexpr std::array<unsigned char, 32> kTestPrivateKey{
    0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60, 0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
    0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19, 0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60};

constexpr std::array<unsigned char, 32> kTestPublicKey{0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7, 0xd5, 0x4b, 0xfe,
                                                       0xd3, 0xc9, 0x64, 0x07, 0x3a, 0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6,
                                                       0x23, 0x25, 0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a};

constexpr char kTestKeyId[] = "test-folder-structure-key";
constexpr int kDialogRefreshWaitAttempts = 50;
constexpr int kDialogRefreshProcessEventsMs = 25;
constexpr int kDialogRefreshSleepMs = 10;

struct ArchiveEntry {
    std::string name;
    QByteArray payload;
};

FolderStructurePluginPublicKey test_public_key() {
    return FolderStructurePluginPublicKey{kTestKeyId, kTestPublicKey};
}

PluginEntitlementPublicKey test_entitlement_public_key() {
    return PluginEntitlementPublicKey{kTestKeyId, kTestPublicKey};
}

std::string sha256_hex(const QByteArray& payload) {
    return QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex().toStdString();
}

std::string read_binary_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

QByteArray sign_ed25519(const QByteArray& payload) {
    EVP_PKEY* key =
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, kTestPrivateKey.data(), kTestPrivateKey.size());
    REQUIRE(key != nullptr);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    REQUIRE(ctx != nullptr);

    QByteArray signature;
    signature.resize(64);
    std::size_t signature_size = static_cast<std::size_t>(signature.size());
    REQUIRE(EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1);
    REQUIRE(EVP_DigestSign(ctx, reinterpret_cast<unsigned char*>(signature.data()), &signature_size,
                           reinterpret_cast<const unsigned char*>(payload.constData()),
                           static_cast<std::size_t>(payload.size())) == 1);
    signature.resize(static_cast<qsizetype>(signature_size));

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return signature;
}

void create_zip_archive(const std::filesystem::path& archive_path, const std::vector<ArchiveEntry>& entries) {
    int error_code = 0;
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error_code);
    REQUIRE(archive != nullptr);

    for (const auto& entry : entries) {
        zip_source_t* source =
            zip_source_buffer(archive, entry.payload.constData(), static_cast<zip_uint64_t>(entry.payload.size()), 0);
        REQUIRE(source != nullptr);
        const zip_int64_t index =
            zip_file_add(archive, entry.name.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        REQUIRE(index >= 0);
    }

    REQUIRE(zip_close(archive) == 0);
}

std::vector<ArchiveEntry> signed_plugin_entries(const std::string& manifest, const std::string& profile,
                                                const std::vector<ArchiveEntry>& extra_entries = {}) {
    std::vector<std::pair<std::string, QByteArray>> signed_files{
        {"manifest.json", QByteArray::fromStdString(manifest)},
        {"profiles/johnny.json", QByteArray::fromStdString(profile)}};
    for (const auto& extra : extra_entries) {
        signed_files.push_back({extra.name, extra.payload});
    }

    std::string signature_manifest = std::string("{\n") + "  \"schema_version\": 1,\n" +
                                     "  \"algorithm\": \"ed25519\",\n" + "  \"key_id\": \"" + kTestKeyId + "\",\n" +
                                     "  \"files\": [\n";
    for (std::size_t index = 0; index < signed_files.size(); ++index) {
        const auto& [path, payload] = signed_files[index];
        signature_manifest += "    {\"path\":\"" + path + "\",\"sha256\":\"" + sha256_hex(payload) + "\"}";
        signature_manifest += index + 1 == signed_files.size() ? "\n" : ",\n";
    }
    signature_manifest += "  ]\n}\n";

    const QByteArray signature_payload = QByteArray::fromStdString(signature_manifest);
    std::vector<ArchiveEntry> entries{{"johnny/manifest.json", QByteArray::fromStdString(manifest)},
                                      {"johnny/profiles/johnny.json", QByteArray::fromStdString(profile)},
                                      {"johnny/plugin-signature.json", signature_payload},
                                      {"johnny/plugin-signature.sig", sign_ed25519(signature_payload)}};
    for (const auto& extra : extra_entries) {
        entries.push_back({"johnny/" + extra.name, extra.payload});
    }
    return entries;
}

std::string plugin_manifest() {
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

std::string licensed_plugin_manifest() {
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
  "license_required": true,
  "product_id": "johnny_decimal_support",
  "profile_paths": ["profiles/johnny.json"]
})json";
}

std::string plugin_profile() {
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

QByteArray entitlement_payload(const std::string& device_id, const QString& offline_until,
                               const QString& status = QStringLiteral("active"),
                               const QString& product_id = QStringLiteral("johnny_decimal_support"),
                               const QString& plugin_id = QStringLiteral("johnny_decimal_support")) {
    QJsonObject entitlement;
    entitlement.insert("plugin_id", plugin_id);
    entitlement.insert("product_id", product_id);
    entitlement.insert("status", status);

    QJsonArray entitlements;
    entitlements.append(entitlement);

    QJsonObject payload;
    payload.insert("version", "aifs.entitlement.v1");
    payload.insert("license_id", "lic_test");
    payload.insert("device_id", QString::fromStdString(device_id));
    payload.insert("issued_at", "2026-09-13T00:00:00Z");
    payload.insert("offline_until", offline_until);
    payload.insert("entitlements", entitlements);
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

bool wait_for_button_enabled(QPushButton* button) {
    for (int attempt = 0; attempt < kDialogRefreshWaitAttempts; ++attempt) {
        if (button && button->isEnabled()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, kDialogRefreshProcessEventsMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(kDialogRefreshSleepMs));
    }
    QCoreApplication::processEvents();
    return button && button->isEnabled();
}

}  // namespace

TEST_CASE("FolderStructurePluginCatalog parses Johnny Decimal catalog entries") {
    const std::string catalog_json = R"json({
  "version": "aifs.folder_structure_plugin_catalog.v1",
  "plugins": [
    {
      "id": "johnny_decimal_support",
      "name": "Johnny.Decimal Support",
      "description": "Adds Johnny.Decimal starter folders, archive recognition, validation, index views, next-folder creation, and routing guidance.",
      "version": "1.0.0",
      "license_required": true,
      "product_id": "johnny_decimal_support",
      "purchase_url": "https://filesorter.app/plugins/johnny-decimal-support/",
      "package_download_url": "",
      "package_sha256": ""
    }
  ]
})json";

    std::string error;
    const auto entries = load_folder_structure_plugin_catalog_entries_from_json(catalog_json, &error);

    REQUIRE(error.empty());
    REQUIRE(entries.size() == 1);
    const auto& entry = entries.front();
    CHECK(entry.id == "johnny_decimal_support");
    CHECK(entry.name == "Johnny.Decimal Support");
    CHECK(entry.version == "1.0.0");
    CHECK(entry.license_required);
    CHECK(entry.product_id == "johnny_decimal_support");
    CHECK(entry.purchase_url == "https://filesorter.app/plugins/johnny-decimal-support/");
    CHECK_FALSE(entry.has_remote_package());
}

TEST_CASE("FolderStructurePluginCatalogFetcher downloads catalog entries and packages") {
    TempDir cache_dir;
    const std::string catalog_url = "https://plugins.example.invalid/folder-structures/catalog/";
    const std::string package_url =
        "https://plugins.example.invalid/folder-structures/johnny-decimal-support-1.0.0.aifsplugin";
    const std::string package_payload = "signed johnny decimal plugin package";
    const std::string package_sha = sha256_hex(QByteArray::fromStdString(package_payload));
    const std::string catalog_json = std::string(R"json({
  "version": "aifs.folder_structure_plugin_catalog.v1",
  "plugins": [
    {
      "id": "johnny_decimal_support",
      "name": "Johnny.Decimal Support",
      "description": "Adds Johnny.Decimal starter folders and routing guidance.",
      "version": "1.0.0",
      "license_required": true,
      "product_id": "johnny_decimal_support",
      "purchase_url": "https://filesorter.app/plugins/johnny-decimal-support/",
      "package_download_url": ")json") +
                                     package_url +
                                     R"json(",
      "package_sha256": ")json" + package_sha +
                                     R"json("
    }
  ]
})json";

    auto download_fn = [catalog_url, package_url, catalog_json, package_payload](
                           const std::string& url, const std::filesystem::path& destination,
                           FolderStructurePluginCatalogFetcher::ProgressCallback,
                           FolderStructurePluginCatalogFetcher::CancelCheck) {
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (url == catalog_url) {
            out << catalog_json;
            return;
        }
        if (url == package_url) {
            out.write(package_payload.data(), static_cast<std::streamsize>(package_payload.size()));
            return;
        }
        throw std::runtime_error("Unexpected folder-structure plugin URL");
    };

    FolderStructurePluginCatalogFetcher fetcher(cache_dir.path(), download_fn);
    std::string error;
    const auto entries = fetcher.fetch_catalog_entries(catalog_url, &error);

    REQUIRE(error.empty());
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().has_remote_package());

    std::filesystem::path archive_path;
    REQUIRE(fetcher.fetch_package_archive(entries.front(), &archive_path, &error));
    CHECK(error.empty());
    CHECK(archive_path.filename() == "johnny-decimal-support-1.0.0.aifsplugin");
    CHECK(read_binary_file(archive_path) == package_payload);
}

TEST_CASE("FolderStructurePluginManager refreshes remote catalog entries") {
    if (!PluginBuildConfig::remote_catalogs_enabled()) {
        SUCCEED("Remote plugin catalogs are disabled for this build.");
        return;
    }

    TempDir config_dir;
    const std::string catalog_url = "https://plugins.example.invalid/folder-structures/catalog/";
    EnvVarGuard catalog_guard("AI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL", catalog_url);
    const std::string catalog_json = R"json({
  "version": "aifs.folder_structure_plugin_catalog.v1",
  "plugins": [
    {
      "id": "johnny_decimal_support",
      "name": "Johnny.Decimal Support",
      "description": "Adds Johnny.Decimal starter folders and routing guidance.",
      "version": "1.0.0",
      "license_required": true,
      "product_id": "johnny_decimal_support",
      "purchase_url": "https://filesorter.app/plugins/johnny-decimal-support/",
      "package_download_url": "",
      "package_sha256": ""
    }
  ]
})json";

    auto download_fn = [catalog_url, catalog_json](const std::string& url, const std::filesystem::path& destination,
                                                   FolderStructurePluginCatalogFetcher::ProgressCallback,
                                                   FolderStructurePluginCatalogFetcher::CancelCheck) {
        if (url != catalog_url) {
            throw std::runtime_error("Unexpected folder-structure catalog URL");
        }
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        out << catalog_json;
    };

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()}, nullptr, download_fn);
    REQUIRE(manager.remote_catalog_configured());

    std::string error;
    REQUIRE(manager.refresh_remote_catalog(&error));
    CHECK(error.empty());

    const auto plugin = manager.find_plugin("johnny_decimal_support");
    REQUIRE(plugin.has_value());
    CHECK(plugin->name == "Johnny.Decimal Support");
    CHECK(plugin->license_required);
    CHECK_FALSE(plugin->has_remote_package());

    FolderStructurePluginManager reloaded(config_dir.path().string(), {test_public_key()}, nullptr, download_fn);
    const auto cached_plugin = reloaded.find_plugin("johnny_decimal_support");
    REQUIRE(cached_plugin.has_value());
    CHECK(cached_plugin->purchase_url == "https://filesorter.app/plugins/johnny-decimal-support/");
}

TEST_CASE("FolderStructurePluginManager installs catalog plugins on demand") {
    if (!PluginBuildConfig::remote_catalogs_enabled()) {
        SUCCEED("Remote plugin catalogs are disabled for this build.");
        return;
    }

    TempDir config_dir;
    TempDir archive_dir;
    const std::string catalog_url = "https://plugins.example.invalid/folder-structures/catalog/";
    const std::string package_url =
        "https://plugins.example.invalid/folder-structures/johnny-decimal-support-1.0.0.aifsplugin";
    EnvVarGuard catalog_guard("AI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL", catalog_url);

    const auto archive_path = archive_dir.path() / "johnny-decimal-support-1.0.0.aifsplugin";
    create_zip_archive(archive_path, signed_plugin_entries(plugin_manifest(), plugin_profile()));
    const std::string package_payload = read_binary_file(archive_path);
    const std::string package_sha = sha256_hex(QByteArray::fromStdString(package_payload));
    const std::string catalog_json = std::string(R"json({
  "version": "aifs.folder_structure_plugin_catalog.v1",
  "plugins": [
    {
      "id": "johnny_decimal_support",
      "name": "Johnny.Decimal Support",
      "description": "Adds Johnny.Decimal starter folders and routing guidance.",
      "version": "1.0.0",
      "license_required": false,
      "product_id": "johnny_decimal_support",
      "purchase_url": "https://filesorter.app/plugins/johnny-decimal-support/",
      "package_download_url": ")json") +
                                     package_url +
                                     R"json(",
      "package_sha256": ")json" + package_sha +
                                     R"json("
    }
  ]
})json";

    auto download_fn = [catalog_url, package_url, catalog_json, package_payload](
                           const std::string& url, const std::filesystem::path& destination,
                           FolderStructurePluginCatalogFetcher::ProgressCallback,
                           FolderStructurePluginCatalogFetcher::CancelCheck) {
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (url == catalog_url) {
            out << catalog_json;
            return;
        }
        if (url == package_url) {
            out.write(package_payload.data(), static_cast<std::streamsize>(package_payload.size()));
            return;
        }
        throw std::runtime_error("Unexpected folder-structure plugin URL");
    };

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()}, nullptr, download_fn);
    std::string error;
    REQUIRE(manager.refresh_remote_catalog(&error));
    REQUIRE(manager.install("johnny_decimal_support", &error));
    CHECK(error.empty());
    CHECK(manager.is_installed("johnny_decimal_support"));
    REQUIRE(manager.installed_profiles().size() == 1);
}

TEST_CASE("PluginEntitlementService verifies signed device receipts") {
    TempDir config_dir;
    PluginEntitlementService service(config_dir.path(), {test_entitlement_public_key()});
    const QByteArray payload = entitlement_payload(service.device_id(), QStringLiteral("2999-01-01T00:00:00Z"));

    std::string error;
    REQUIRE(service.store_receipt(payload, sign_ed25519(payload), kTestKeyId, &error));
    CHECK(error.empty());
    CHECK(service.has_entitlement("johnny_decimal_support", "johnny_decimal_support"));
    CHECK_FALSE(service.has_entitlement("other_product", "johnny_decimal_support"));
}

TEST_CASE("PluginEntitlementService rejects invalid receipts") {
    TempDir config_dir;
    PluginEntitlementService service(config_dir.path(), {test_entitlement_public_key()});

    const QByteArray valid_payload = entitlement_payload(service.device_id(), QStringLiteral("2999-01-01T00:00:00Z"));
    QByteArray tampered_payload = valid_payload;
    tampered_payload.append(' ');

    std::string error;
    CHECK_FALSE(service.store_receipt(tampered_payload, sign_ed25519(valid_payload), kTestKeyId, &error));
    CHECK(error.find("signature") != std::string::npos);

    error.clear();
    const QByteArray expired_payload = entitlement_payload(service.device_id(), QStringLiteral("2000-01-01T00:00:00Z"));
    CHECK_FALSE(service.store_receipt(expired_payload, sign_ed25519(expired_payload), kTestKeyId, &error));
    CHECK(error.find("expired") != std::string::npos);

    error.clear();
    const QByteArray other_device_payload = entitlement_payload("dev_other", QStringLiteral("2999-01-01T00:00:00Z"));
    CHECK_FALSE(service.store_receipt(other_device_payload, sign_ed25519(other_device_payload), kTestKeyId, &error));
    CHECK(error.find("device") != std::string::npos);
    CHECK_FALSE(service.has_entitlement("johnny_decimal_support", "johnny_decimal_support"));
}

TEST_CASE("FolderStructurePluginManager installs signed declarative plugins") {
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "johnny-decimal.aifsplugin";
    auto entries = signed_plugin_entries(plugin_manifest(), plugin_profile());
    SECTION("Forward-slash archive paths") {}
    SECTION("Windows archive paths with explicit directories") {
        for (auto& entry : entries) {
            std::replace(entry.name.begin(), entry.name.end(), '/', '\\');
        }
        entries.insert(entries.begin(), {"johnny\\profiles\\", QByteArray{}});
    }
    create_zip_archive(archive_path, entries);

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
    const auto plugin_descriptor = std::find_if(descriptors.begin(), descriptors.end(),
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

TEST_CASE("FolderStructurePluginManager persists disabled plugin profiles") {
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "johnny-decimal.aifsplugin";
    create_zip_archive(archive_path, signed_plugin_entries(plugin_manifest(), plugin_profile()));

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()});

    std::string error;
    REQUIRE(manager.install_from_archive(archive_path, nullptr, &error));
    REQUIRE(manager.is_enabled("johnny_decimal_support"));
    REQUIRE(manager.installed_profiles().size() == 1);

    REQUIRE(manager.set_enabled("johnny_decimal_support", false, &error));
    CHECK_FALSE(manager.is_enabled("johnny_decimal_support"));
    CHECK(manager.installed_plugins().size() == 1);
    CHECK(manager.installed_profiles().empty());

    FolderStructurePluginManager reloaded(config_dir.path().string(), {test_public_key()});
    CHECK_FALSE(reloaded.is_enabled("johnny_decimal_support"));
    CHECK(reloaded.installed_profiles().empty());

    REQUIRE(reloaded.set_enabled("johnny_decimal_support", true, &error));
    CHECK(reloaded.is_enabled("johnny_decimal_support"));
    CHECK(reloaded.installed_profiles().size() == 1);
}

TEST_CASE("Johnny.Decimal offer still triggers when installed plugin is disabled") {
    EnvVarGuard catalog_guard("AI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL", " ");
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;
    TempDir config_dir;
    EnvVarGuard home_guard("HOME", config_dir.path().string());
    EnvVarGuard app_config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    const std::filesystem::path archive_root = config_dir.path() / "ExistingArchive";
    std::filesystem::create_directories(archive_root / "00-09 System" / "01 Inbox");
    std::filesystem::create_directories(archive_root / "00-09 System" / "02 Settings");
    std::filesystem::create_directories(archive_root / "10-19 Admin" / "11 Finance");
    std::filesystem::create_directories(archive_root / "10-19 Admin" / "12 Legal");

    const auto package_path = config_dir.path() / "johnny-decimal.aifsplugin";
    create_zip_archive(package_path, signed_plugin_entries(plugin_manifest(), plugin_profile()));

    auto manager = std::make_shared<FolderStructurePluginManager>(
        config_dir.path().string(), std::vector<FolderStructurePluginPublicKey>{test_public_key()});
    std::string error;
    REQUIRE(manager->install_from_archive(package_path, nullptr, &error));
    REQUIRE(manager->is_installed("johnny_decimal_support"));
    REQUIRE(manager->is_enabled("johnny_decimal_support"));

    Settings settings;
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false, /*test_mode=*/true, config_dir.path().string());
    MainAppTestAccess::set_folder_structure_plugin_manager(window, manager);

    const QString root = QString::fromStdString(archive_root.string());
    CHECK_FALSE(MainAppTestAccess::should_offer_johnny_decimal_plugin_for_root(window, root));

    REQUIRE(manager->set_enabled("johnny_decimal_support", false, &error));
    CHECK(MainAppTestAccess::should_offer_johnny_decimal_plugin_for_root(window, root));

    settings.set_hide_johnny_decimal_plugin_suggestion(true);
    CHECK_FALSE(MainAppTestAccess::should_offer_johnny_decimal_plugin_for_root(window, root));
}

TEST_CASE("FolderStructurePluginManager requires entitlement for licensed plugins") {
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "johnny-decimal-paid.aifsplugin";
    create_zip_archive(archive_path, signed_plugin_entries(licensed_plugin_manifest(), plugin_profile()));

    auto entitlement_service = std::make_shared<PluginEntitlementService>(
        config_dir.path(), std::vector<PluginEntitlementPublicKey>{test_entitlement_public_key()});
    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()}, entitlement_service);

    FolderStructurePluginInstallError error;
    CHECK_FALSE(manager.install_from_archive(archive_path, nullptr, &error));
    CHECK_FALSE(error.message.empty());
    CHECK(error.missing_entitlement);
    CHECK(error.product_id == "johnny_decimal_support");
    CHECK(error.plugin_id == "johnny_decimal_support");
    CHECK(error.plugin_name == "Johnny.Decimal Support");
    CHECK(manager.installed_plugins().empty());
}

TEST_CASE("FolderStructurePluginManager installs licensed plugins with entitlement receipts") {
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "johnny-decimal-paid.aifsplugin";
    create_zip_archive(archive_path, signed_plugin_entries(licensed_plugin_manifest(), plugin_profile()));

    auto entitlement_service = std::make_shared<PluginEntitlementService>(
        config_dir.path(), std::vector<PluginEntitlementPublicKey>{test_entitlement_public_key()});
    const QByteArray payload =
        entitlement_payload(entitlement_service->device_id(), QStringLiteral("2999-01-01T00:00:00Z"));
    std::string error;
    REQUIRE(entitlement_service->store_receipt(payload, sign_ed25519(payload), kTestKeyId, &error));

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()}, entitlement_service);

    std::string installed_plugin_id;
    REQUIRE(manager.install_from_archive(archive_path, &installed_plugin_id, &error));
    CHECK(installed_plugin_id == "johnny_decimal_support");
    CHECK(manager.installed_plugins().size() == 1);
    CHECK(manager.installed_profiles().size() == 1);
}

TEST_CASE("FolderStructurePluginDialog controls plugin enablement") {
    EnvVarGuard catalog_guard("AI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL", " ");
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "johnny-decimal.aifsplugin";
    create_zip_archive(archive_path, signed_plugin_entries(plugin_manifest(), plugin_profile()));

    auto manager = std::make_shared<FolderStructurePluginManager>(
        config_dir.path().string(), std::vector<FolderStructurePluginPublicKey>{test_public_key()});
    std::string error;
    REQUIRE(manager->install_from_archive(archive_path, nullptr, &error));

    FolderStructurePluginDialog dialog(manager);

    auto* plugin_list = dialog.findChild<QTreeWidget*>();
    REQUIRE(plugin_list != nullptr);
    CHECK(plugin_list->selectionBehavior() == QAbstractItemView::SelectRows);
    REQUIRE(plugin_list->topLevelItemCount() == 1);
    auto* item = plugin_list->topLevelItem(0);
    REQUIRE(item != nullptr);
    CHECK(item->checkState(0) == Qt::Checked);

    QString combined_label_text;
    for (const auto* label : dialog.findChildren<QLabel*>()) {
        combined_label_text += label->text();
        combined_label_text += QLatin1Char('\n');
    }
    CHECK(combined_label_text.contains(QStringLiteral("Status: Enabled")));

    item->setCheckState(0, Qt::Unchecked);
    QCoreApplication::processEvents();
    CHECK_FALSE(manager->is_enabled("johnny_decimal_support"));
    CHECK(manager->installed_profiles().empty());
}

TEST_CASE("FolderStructurePluginDialog lists remote catalog plugins") {
    if (!PluginBuildConfig::remote_catalogs_enabled()) {
        SUCCEED("Remote plugin catalogs are disabled for this build.");
        return;
    }

    const std::string catalog_url = "https://plugins.example.invalid/folder-structures/catalog/";
    EnvVarGuard catalog_guard("AI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL", catalog_url);
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;
    TempDir config_dir;
    const std::string catalog_json = R"json({
  "version": "aifs.folder_structure_plugin_catalog.v1",
  "plugins": [
    {
      "id": "johnny_decimal_support",
      "name": "Johnny.Decimal Support",
      "description": "Adds Johnny.Decimal starter folders and routing guidance.",
      "version": "1.0.0",
      "license_required": true,
      "product_id": "johnny_decimal_support",
      "purchase_url": "https://filesorter.app/plugins/johnny-decimal-support/",
      "package_download_url": "",
      "package_sha256": ""
    }
  ]
})json";

    auto download_fn = [catalog_url, catalog_json](const std::string& url, const std::filesystem::path& destination,
                                                   FolderStructurePluginCatalogFetcher::ProgressCallback,
                                                   FolderStructurePluginCatalogFetcher::CancelCheck) {
        if (url != catalog_url) {
            throw std::runtime_error("Unexpected folder-structure catalog URL");
        }
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        out << catalog_json;
    };

    auto entitlement_service = std::make_shared<PluginEntitlementService>(
        config_dir.path(), std::vector<PluginEntitlementPublicKey>{test_entitlement_public_key()});
    bool license_active = false;
    SECTION("License activation needed") {}
    SECTION("License active before installation") {
        const QByteArray payload =
            entitlement_payload(entitlement_service->device_id(), QStringLiteral("2999-01-01T00:00:00Z"));
        std::string error;
        REQUIRE(entitlement_service->store_receipt(payload, sign_ed25519(payload), kTestKeyId, &error));
        license_active = true;
    }

    auto manager = std::make_shared<FolderStructurePluginManager>(
        config_dir.path().string(), std::vector<FolderStructurePluginPublicKey>{test_public_key()},
        entitlement_service, download_fn);
    std::string error;
    REQUIRE(manager->refresh_remote_catalog(&error));
    CHECK_FALSE(manager->is_installed("johnny_decimal_support"));

    FolderStructurePluginDialog dialog(manager);
    auto* plugin_list = dialog.findChild<QTreeWidget*>();
    REQUIRE(plugin_list != nullptr);
    REQUIRE(plugin_list->topLevelItemCount() == 1);
    auto* item = plugin_list->topLevelItem(0);
    REQUIRE(item != nullptr);
    CHECK(item->text(0) == QStringLiteral("Available"));
    CHECK(item->text(1) == QStringLiteral("Johnny.Decimal Support"));
    CHECK(item->text(2) == QStringLiteral("1.0.0"));

    QPushButton* activate_button = nullptr;
    QPushButton* obtain_button = nullptr;
    for (auto* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Activate License...")) {
            activate_button = button;
        } else if (button->text() == (license_active ? QStringLiteral("Read more") : QStringLiteral("Get It"))) {
            obtain_button = button;
        }
    }
    REQUIRE(activate_button != nullptr);
    CHECK(activate_button->isHidden() == license_active);
    REQUIRE(obtain_button != nullptr);
    CHECK_FALSE(obtain_button->isHidden());
    CHECK(wait_for_button_enabled(obtain_button));
    CHECK(obtain_button->icon().isNull() == license_active);
}

TEST_CASE("FolderStructurePluginDialog shows read-more action for installed activated plugins") {
    if (!PluginBuildConfig::commercial_plugin_actions_enabled()) {
        SUCCEED("Commercial plugin actions are disabled for this build.");
        return;
    }

    EnvVarGuard catalog_guard("AI_FILE_SORTER_FOLDER_STRUCTURE_PLUGIN_CATALOG_URL", " ");
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "johnny-decimal-paid.aifsplugin";
    create_zip_archive(archive_path, signed_plugin_entries(licensed_plugin_manifest(), plugin_profile()));

    auto entitlement_service = std::make_shared<PluginEntitlementService>(
        config_dir.path(), std::vector<PluginEntitlementPublicKey>{test_entitlement_public_key()});
    const QByteArray payload =
        entitlement_payload(entitlement_service->device_id(), QStringLiteral("2999-01-01T00:00:00Z"));
    std::string error;
    REQUIRE(entitlement_service->store_receipt(payload, sign_ed25519(payload), kTestKeyId, &error));

    auto manager = std::make_shared<FolderStructurePluginManager>(
        config_dir.path().string(), std::vector<FolderStructurePluginPublicKey>{test_public_key()},
        entitlement_service);
    REQUIRE(manager->install_from_archive(archive_path, nullptr, &error));

    FolderStructurePluginDialog dialog(manager);

    auto* plugin_list = dialog.findChild<QTreeWidget*>();
    REQUIRE(plugin_list != nullptr);
    REQUIRE(plugin_list->topLevelItemCount() == 1);
    auto* item = plugin_list->topLevelItem(0);
    REQUIRE(item != nullptr);
    CHECK(item->checkState(0) == Qt::Checked);

    QPushButton* read_more_button = nullptr;
    QPushButton* obtain_button = nullptr;
    for (auto* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Read more")) {
            read_more_button = button;
        } else if (button->text() == QStringLiteral("Get It")) {
            obtain_button = button;
        }
    }

    REQUIRE(read_more_button != nullptr);
    CHECK_FALSE(read_more_button->isHidden());
    CHECK(wait_for_button_enabled(read_more_button));
    CHECK(obtain_button == nullptr);
}

TEST_CASE("PluginArchiveExtractor rejects unsafe paths with either separator") {
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "unsafe.aifsplugin";
    const auto destination = archive_dir.path() / "extracted";
    std::string unsafe_path;
    SECTION("Forward-slash traversal") { unsafe_path = "../escaped.json"; }
    SECTION("Backslash traversal") { unsafe_path = "..\\escaped.json"; }
    SECTION("Mixed-separator traversal") { unsafe_path = "profiles/..\\..\\escaped.json"; }
    SECTION("Backslash absolute path") { unsafe_path = "\\escaped.json"; }
    SECTION("Windows network path") { unsafe_path = "\\\\server\\share\\escaped.json"; }
    create_zip_archive(archive_path, {{"manifest.json", QByteArray::fromStdString(plugin_manifest())},
                                      {unsafe_path, QByteArray("unsafe payload")}});

    const auto result = PluginArchiveExtractor::extract_archive(archive_path, destination);
    CHECK_FALSE(result.ok());
    CHECK(result.message.find("unsafe path") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(archive_dir.path() / "escaped.json"));
}

TEST_CASE("FolderStructurePluginManager rejects unsigned payload files") {
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "unsigned-payload.aifsplugin";
    auto entries = signed_plugin_entries(plugin_manifest(), plugin_profile());
    entries.push_back({"johnny\\profiles\\unsigned.json", QByteArray("unsigned payload")});
    create_zip_archive(archive_path, entries);

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()});
    std::string error;
    CHECK_FALSE(manager.install_from_archive(archive_path, nullptr, &error));
    CHECK(error.find("unsigned payload") != std::string::npos);
    CHECK(manager.installed_plugins().empty());
}

TEST_CASE("FolderStructurePluginManager rejects tampered plugin payloads") {
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

TEST_CASE("FolderStructurePluginManager rejects executable payload files") {
    TempDir config_dir;
    TempDir archive_dir;
    const auto archive_path = archive_dir.path() / "executable-payload.aifsplugin";
    const std::vector<ArchiveEntry> extra_entries{{"bin/helper.exe", QByteArray("not a real executable")}};
    create_zip_archive(archive_path, signed_plugin_entries(plugin_manifest(), plugin_profile(), extra_entries));

    FolderStructurePluginManager manager(config_dir.path().string(), {test_public_key()});

    std::string error;
    CHECK_FALSE(manager.install_from_archive(archive_path, nullptr, &error));
    CHECK(error.find("executable") != std::string::npos);
    CHECK(manager.installed_plugins().empty());
}
