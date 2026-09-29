#include <openssl/evp.h>
#include <spdlog/fmt/fmt.h>
#include <zip.h>

#include <QCryptographicHash>
#include <algorithm>
#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "CategorizationService.hpp"
#include "DatabaseManager.hpp"
#include "DocumentTextAnalyzer.hpp"
#include "FolderTreeCatalog.hpp"
#include "ILLMClient.hpp"
#include "LocalFsProvider.hpp"
#include "ResultsCoordinator.hpp"
#include "Settings.hpp"
#include "StoragePluginLoader.hpp"
#include "StoragePluginManager.hpp"
#include "StoragePluginPackageSignature.hpp"
#include "StorageProviderRegistry.hpp"
#include "TestHelpers.hpp"
#include "UndoManager.hpp"
#include "Utils.hpp"

namespace {
constexpr std::array<unsigned char, 32> kStoragePackageTestPrivateKey{
    0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60, 0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
    0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19, 0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60};

constexpr std::array<unsigned char, 32> kStoragePackageTestPublicKey{
    0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7, 0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
    0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25, 0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a};

constexpr char kStoragePackageTestKeyId[] = "test-storage-package-key";

void write_file(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "data";
}

StoragePluginPackagePublicKey storage_package_test_key() {
    return StoragePluginPackagePublicKey{kStoragePackageTestKeyId, kStoragePackageTestPublicKey};
}

std::vector<StoragePluginPackagePublicKey> storage_package_test_keys() {
    return {storage_package_test_key()};
}

void copy_file_with_permissions(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::permissions(destination, std::filesystem::status(source).permissions(),
                                 std::filesystem::perm_options::replace);
}

std::string read_binary_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string utf8_string(const char8_t* value) {
    return std::string(reinterpret_cast<const char*>(value));
}

std::string sha256_hex(std::string_view payload) {
    const QByteArray bytes(payload.data(), static_cast<qsizetype>(payload.size()));
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toStdString();
}

void create_zip_archive(const std::filesystem::path& archive_path,
                        const std::vector<std::pair<std::string, std::string>>& entries) {
    int error_code = 0;
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error_code);
    REQUIRE(archive != nullptr);

    for (const auto& [name, payload] : entries) {
        zip_source_t* source = zip_source_buffer(archive, payload.data(), static_cast<zip_uint64_t>(payload.size()), 0);
        REQUIRE(source != nullptr);
        const zip_int64_t index = zip_file_add(archive, name.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        REQUIRE(index >= 0);
    }

    REQUIRE(zip_close(archive) == 0);
}

std::string string_from_bytes(const QByteArray& bytes) {
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QByteArray sign_storage_package_payload(const QByteArray& payload) {
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, kStoragePackageTestPrivateKey.data(),
                                                 kStoragePackageTestPrivateKey.size());
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

std::vector<std::pair<std::string, std::string>> signed_storage_plugin_entries(
    const std::string& manifest, std::vector<std::pair<std::string, std::string>> payload_entries,
    const std::string& key_id = kStoragePackageTestKeyId) {
    std::vector<std::pair<std::string, std::string>> signed_files{{"manifest.json", manifest}};
    signed_files.insert(signed_files.end(), payload_entries.begin(), payload_entries.end());

    std::string signature_manifest = std::string("{\n") + "  \"schema_version\": 1,\n" +
                                     "  \"algorithm\": \"ed25519\",\n" + "  \"key_id\": \"" + key_id + "\",\n" +
                                     "  \"files\": [\n";
    for (std::size_t index = 0; index < signed_files.size(); ++index) {
        const auto& [path, payload] = signed_files[index];
        signature_manifest += "    {\"path\":\"" + path + "\",\"sha256\":\"" + sha256_hex(payload) + "\"}";
        signature_manifest += index + 1 == signed_files.size() ? "\n" : ",\n";
    }
    signature_manifest += "  ]\n}\n";

    const QByteArray signature_payload = QByteArray::fromStdString(signature_manifest);
    std::vector<std::pair<std::string, std::string>> entries = signed_files;
    entries.push_back({"plugin-signature.json", signature_manifest});
    entries.push_back({"plugin-signature.sig", string_from_bytes(sign_storage_package_payload(signature_payload))});
    return entries;
}

void create_signed_storage_plugin_archive(const std::filesystem::path& archive_path, const std::string& manifest,
                                          std::vector<std::pair<std::string, std::string>> payload_entries,
                                          const std::string& key_id = kStoragePackageTestKeyId) {
    create_zip_archive(archive_path, signed_storage_plugin_entries(manifest, std::move(payload_entries), key_id));
}

std::filesystem::path storage_plugin_stub_path() {
#ifdef AIFS_STORAGE_PLUGIN_STUB_NAME
    return std::filesystem::path(QApplication::applicationDirPath().toStdString()) / AIFS_STORAGE_PLUGIN_STUB_NAME;
#else
    return {};
#endif
}

std::string alternate_platform_name() {
    const auto current = storage_plugin_current_platform();
    if (current != "windows") {
        return "windows";
    }
    if (current != "linux") {
        return "linux";
    }
    return "macos";
}

std::string alternate_architecture_name() {
    const auto current = storage_plugin_current_architecture();
    if (current != "arm64") {
        return "arm64";
    }
    if (current != "x86_64") {
        return "x86_64";
    }
    return "x86";
}

std::string packaged_stub_entry_point() {
    return "bin/" + storage_plugin_stub_path().filename().string();
}

std::filesystem::path create_signed_stub_storage_plugin_archive(const std::filesystem::path& archive_dir,
                                                                const std::string& plugin_id,
                                                                const std::string& plugin_name,
                                                                const std::string& provider_id,
                                                                const std::string& version) {
    const auto archive_path = archive_dir / (plugin_id + ".aifsplugin");
    const auto entry_point = packaged_stub_entry_point();
    const auto payload = read_binary_file(storage_plugin_stub_path());
    const std::string manifest = fmt::format(R"json({{
  "id": "{}",
  "name": "{}",
  "description": "Signed test package backed by the external-process storage connector stub.",
  "version": "{}",
  "provider_ids": ["{}"],
  "entry_point_kind": "external_process",
  "entry_point": "{}",
  "package_paths": ["{}"]
}})json",
                                             plugin_id, plugin_name, version, provider_id, entry_point, entry_point);
    create_signed_storage_plugin_archive(archive_path, manifest, {{entry_point, payload}});
    return archive_path;
}

class CountingLLM : public ILLMClient {
   public:
    CountingLLM(std::shared_ptr<int> calls, std::string response)
        : calls_(std::move(calls)), response_(std::move(response)) {}

    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override {
        ++(*calls_);
        return response_;
    }

    std::string complete_prompt(const std::string&, int) override {
        ++(*calls_);
        return response_;
    }

    void set_prompt_logging_enabled(bool) override {}

   private:
    std::shared_ptr<int> calls_;
    std::string response_;
};

class PromptCapturingLLM : public ILLMClient {
   public:
    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override {
        return std::string();
    }

    std::string complete_prompt(const std::string& prompt, int) override {
        last_prompt = prompt;
        return utf8_string(u8R"({"summary":"Quarterly summary","filename":"시장 분석"})");
    }

    void set_prompt_logging_enabled(bool) override {}

    std::string last_prompt;
};
}  // namespace

TEST_CASE("CategorizationService uses cached categorization without calling LLM") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());

    TempDir data_dir;
    const std::string dir_path = data_dir.path().string();
    const std::string file_name = "cached.png";
    const auto resolved = db.resolve_category("Images", "Photos");
    REQUIRE(resolved.taxonomy_id > 0);
    REQUIRE(
        db.insert_or_update_file_with_categorization(file_name, "F", dir_path, resolved, false, std::string(), false));

    CategorizationService service(settings, db, nullptr);
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() { return std::make_unique<CountingLLM>(calls, "Documents : Reports"); };

    const auto full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    const auto categorized = service.categorize_entries(files, true, stop_flag, {}, {}, {}, {}, factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Images");
    CHECK(categorized.front().subcategory == "Photos");
    CHECK(*calls == 0);
}

TEST_CASE("CategorizationService uses cached folder-tree target without calling LLM") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;

    TempDir data_dir;
    const std::string dir_path = data_dir.path().string();
    REQUIRE(std::filesystem::create_directories(data_dir.path() / "Documents" / "Invoices"));
    settings.set_sort_folder(dir_path);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(false);

    DatabaseManager db(settings.get_config_dir());
    const std::string file_name = "invoice_q2_2026.pdf";
    const auto resolved = db.resolve_category("Documents", "Invoices");
    REQUIRE(resolved.taxonomy_id > 0);
    REQUIRE(
        db.insert_or_update_file_with_categorization(file_name, "F", dir_path, resolved, false, std::string(), false));
    const auto catalog = FolderTreeCatalog::Catalog::scan(data_dir.path());
    DatabaseManager::FolderTreeRoutingRecord route;
    route.destination_root = dir_path;
    route.tree_fingerprint = catalog.fingerprint();
    route.allow_new_folders = false;
    route.semantic_category = "Documents";
    route.semantic_subcategory = "Invoices";
    route.semantic_target_folder = "Documents/Invoices";
    route.best_existing_folder = "Documents/Invoices";
    route.best_existing_score = 20;
    route.target_folder_relative_path = "Documents/Invoices";
    route.target_folder_exists = true;
    REQUIRE(db.insert_or_update_folder_tree_routing(file_name, "F", dir_path, route));

    CategorizationService service(settings, db, nullptr);
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<CountingLLM>(calls,
                                             "{\"targetFolder\":\"Other/Unsorted Review\",\"createFolder\":false}");
    };

    const auto full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    const auto categorized = service.categorize_entries(files, true, stop_flag, {}, {}, {}, {}, factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().folder_tree_mode);
    CHECK(categorized.front().target_folder_relative_path == "Documents/Invoices");
    CHECK(categorized.front().target_folder_exists);
    CHECK_FALSE(categorized.front().target_folder_suggested_new);
    CHECK(categorized.front().category == "Documents");
    CHECK(categorized.front().subcategory == "Invoices");
    CHECK(*calls == 0);
}

TEST_CASE("CategorizationService derives new folder route from semantic cache without LLM") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;

    TempDir data_dir;
    const std::string dir_path = data_dir.path().string();
    REQUIRE(std::filesystem::create_directories(data_dir.path() / "Images" / "Screenshots"));
    REQUIRE(std::filesystem::create_directories(data_dir.path() / "Other" / "Unsorted Review"));
    settings.set_sort_folder(dir_path);
    settings.set_sorting_mode(SortingMode::ExistingFolderTree);
    settings.set_suggest_new_folders(true);

    DatabaseManager db(settings.get_config_dir());
    const std::string file_name = "invoice_q3_2026.pdf";
    const auto resolved = db.resolve_category("Documents", "Invoices");
    REQUIRE(resolved.taxonomy_id > 0);
    REQUIRE(
        db.insert_or_update_file_with_categorization(file_name, "F", dir_path, resolved, false, std::string(), false));

    CategorizationService service(settings, db, nullptr);
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() {
        return std::make_unique<CountingLLM>(calls,
                                             "{\"targetFolder\":\"Other/Unsorted Review\",\"createFolder\":false}");
    };

    const auto full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    const auto categorized = service.categorize_entries(files, true, stop_flag, {}, {}, {}, {}, factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().folder_tree_mode);
    CHECK(categorized.front().target_folder_relative_path == "Documents/Invoices");
    CHECK(categorized.front().target_folder_suggested_new);
    CHECK_FALSE(categorized.front().target_folder_exists);
    CHECK(categorized.front().category == "Documents");
    CHECK(categorized.front().subcategory == "Invoices");
    CHECK(*calls == 0);
}

TEST_CASE("CategorizationService falls back to LLM when cache is empty") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());

    TempDir data_dir;
    const std::string dir_path = data_dir.path().string();
    const std::string file_name = "uncached.pdf";
    DatabaseManager::ResolvedCategory empty{0, "", ""};
    REQUIRE(db.insert_or_update_file_with_categorization(file_name, "F", dir_path, empty, false, std::string(), false));

    CategorizationService service(settings, db, nullptr);
    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() { return std::make_unique<CountingLLM>(calls, "Documents : Reports"); };

    const auto full_path = (data_dir.path() / file_name).string();
    const std::vector<FileEntry> files = {FileEntry{full_path, file_name, FileType::File}};

    const auto categorized = service.categorize_entries(files, true, stop_flag, {}, {}, {}, {}, factory);

    REQUIRE(categorized.size() == 1);
    CHECK(categorized.front().category == "Documents");
    CHECK(categorized.front().subcategory == "Reports");
    CHECK(*calls == 1);

    const auto cached = db.get_categorization_from_db(dir_path, file_name, FileType::File);
    REQUIRE(cached.size() == 2);
    CHECK(cached[0] == "Documents");
    CHECK(cached[1] == "Reports");
}

TEST_CASE("CategorizationService invokes completion callback per entry") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const auto first_path = (data_dir.path() / "first.txt").string();
    const auto second_path = (data_dir.path() / "second.txt").string();
    const std::vector<FileEntry> files = {FileEntry{first_path, "first.txt", FileType::File},
                                          FileEntry{second_path, "second.txt", FileType::File}};

    std::atomic<bool> stop_flag{false};
    auto calls = std::make_shared<int>(0);
    auto factory = [calls]() { return std::make_unique<CountingLLM>(calls, "Documents : Reports"); };

    std::size_t queued_count = 0;
    std::size_t completed_count = 0;
    const auto categorized = service.categorize_entries(
        files, true, stop_flag, {}, [&queued_count](const FileEntry&) { ++queued_count; },
        [&completed_count](const FileEntry&) { ++completed_count; }, {}, factory);

    REQUIRE(categorized.size() == files.size());
    CHECK(queued_count == files.size());
    CHECK(completed_count == files.size());
    CHECK(*calls == static_cast<int>(files.size()));
}

TEST_CASE("CategorizationService loads cached entries recursively for analysis") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string root_path = data_dir.path().string();
    const std::string child_path = (data_dir.path() / "child").string();

    const auto resolved = db.resolve_category("Images", "Photos");
    REQUIRE(resolved.taxonomy_id > 0);

    REQUIRE(db.insert_or_update_file_with_categorization("root.png", "F", root_path, resolved, false, std::string(),
                                                         false));
    DatabaseManager::ResolvedCategory empty{0, "", ""};
    REQUIRE(db.insert_or_update_file_with_categorization("suggested.png", "F", child_path, empty, false,
                                                         "rename_me.png", true));

    settings.set_include_subdirectories(false);
    auto cached_root_only = service.load_cached_entries(root_path);
    REQUIRE(cached_root_only.size() == 1);
    CHECK(cached_root_only.front().file_name == "root.png");

    settings.set_include_subdirectories(true);
    auto cached_recursive = service.load_cached_entries(root_path);
    REQUIRE(cached_recursive.size() == 2);
    const auto it = std::find_if(cached_recursive.begin(), cached_recursive.end(),
                                 [](const CategorizedFile& entry) { return entry.file_name == "suggested.png"; });
    REQUIRE(it != cached_recursive.end());
    CHECK(it->suggested_name == "rename_me.png");
    CHECK(it->file_path == child_path);
}

TEST_CASE("Recursive recategorization clears stale subtree cache entries") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    TempDir data_dir;
    const std::string root_path = data_dir.path().string();
    const std::string child_path = (data_dir.path() / "child").string();

    const auto resolved = db.resolve_category("Documents", "Reports");
    REQUIRE(resolved.taxonomy_id > 0);

    // Simulate a partially re-categorized subtree: the root entry already uses the
    // new style while a nested entry is still cached with the old style.
    REQUIRE(
        db.insert_or_update_file_with_categorization("root.txt", "F", root_path, resolved, true, std::string(), false));
    REQUIRE(db.insert_or_update_file_with_categorization("child.txt", "F", child_path, resolved, false, std::string(),
                                                         false));

    CHECK(db.has_categorization_style_conflict(root_path, true, true));

    REQUIRE(db.clear_directory_categorizations(root_path, true));

    settings.set_include_subdirectories(true);
    CHECK(service.load_cached_entries(root_path).empty());
}

TEST_CASE("ResultsCoordinator respects full-path cache keys for recursive scans") {
    TempDir data_dir;
    const auto root_file = data_dir.path() / "sample.txt";
    const auto nested_file = data_dir.path() / "nested" / "sample.txt";
    write_file(root_file);
    write_file(nested_file);

    LocalFsProvider provider;
    ResultsCoordinator coordinator(provider);
    const auto options = FileScanOptions::Files | FileScanOptions::Recursive;

    std::unordered_set<std::string> cached_by_name{"sample.txt"};
    auto uncached_by_name =
        coordinator.find_files_to_categorize(data_dir.path().string(), options, cached_by_name, false);
    CHECK(uncached_by_name.empty());

    std::unordered_set<std::string> cached_by_path{root_file.string()};
    auto uncached_by_path =
        coordinator.find_files_to_categorize(data_dir.path().string(), options, cached_by_path, true);
    REQUIRE(uncached_by_path.size() == 1);
    CHECK(uncached_by_path.front().full_path == nested_file.string());
}

TEST_CASE("ResultsCoordinator preserves UTF-8 full paths during recursive matching") {
    TempDir data_dir;
    const auto unicode_name = utf8_string(u8"旅行.txt");
    const auto root_file = data_dir.path() / Utils::utf8_to_path(unicode_name);
    const auto nested_file = data_dir.path() / "nested" / Utils::utf8_to_path(unicode_name);
    write_file(root_file);
    write_file(nested_file);

    LocalFsProvider provider;
    ResultsCoordinator coordinator(provider);
    const auto options = FileScanOptions::Files | FileScanOptions::Recursive;
    const std::string root_dir = Utils::path_to_utf8(data_dir.path());
    const std::string root_file_utf8 = Utils::path_to_utf8(root_file);
    const std::string nested_file_utf8 = Utils::path_to_utf8(nested_file);

    const std::vector<CategorizedFile> categorized = {CategorizedFile{Utils::path_to_utf8(root_file.parent_path()),
                                                                      Utils::path_to_utf8(root_file.filename()),
                                                                      FileType::File, "Documents", "Travel", 0}};

    const auto cached_paths = coordinator.extract_file_names(categorized, true);
    CHECK(cached_paths.contains(root_file_utf8));

    const auto uncached = coordinator.find_files_to_categorize(root_dir, options, cached_paths, true);
    REQUIRE(uncached.size() == 1);
    CHECK(uncached.front().full_path == nested_file_utf8);

    const auto actual_files = coordinator.list_directory(root_dir, options);
    const auto files_to_sort = coordinator.compute_files_to_sort(root_dir, options, actual_files, categorized, true);
    REQUIRE(files_to_sort.size() == 1);
    CHECK(files_to_sort.front().file_path == Utils::path_to_utf8(root_file.parent_path()));
    CHECK(files_to_sort.front().file_name == Utils::path_to_utf8(root_file.filename()));
}

TEST_CASE("DocumentTextAnalyzer handles UTF-8 filenames") {
    TempDir data_dir;
    const auto unicode_name = utf8_string(u8"東京_보고서.txt");
    const auto document_path = data_dir.path() / Utils::utf8_to_path(unicode_name);
    write_file(document_path);

    PromptCapturingLLM llm;
    DocumentTextAnalyzer analyzer;

    const auto result = analyzer.analyze(document_path, llm);

    CHECK(llm.last_prompt.find(unicode_name) != std::string::npos);
    CHECK(result.suggested_name == utf8_string(u8"시장_분석.txt"));
}

TEST_CASE("StorageProviderRegistry resolves the local filesystem provider by default") {
    StorageProviderRegistry registry;
    auto local_provider = std::make_shared<LocalFsProvider>();
    registry.register_builtin(local_provider);

    const auto detection = registry.detect(std::string());
    REQUIRE(detection.matched);
    CHECK(detection.provider_id == "local_fs");

    const auto resolved = registry.resolve_for(std::string());
    REQUIRE(resolved);
    CHECK(resolved->id() == "local_fs");
}

TEST_CASE("StorageProviderRegistry detects cloud folders while resolving local fallback") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    StorageProviderRegistry registry;
    StoragePluginLoader loader;
    for (auto& provider : loader.create_detection_providers()) {
        registry.register_builtin(std::move(provider));
    }
    auto local_provider = std::make_shared<LocalFsProvider>();
    registry.register_builtin(local_provider);

    const std::string folder_path = "/Users/example/OneDrive - Work/Documents";
    const auto detection = registry.detect(folder_path);
    REQUIRE(detection.matched);
    CHECK(detection.provider_id == "onedrive");
    CHECK(detection.needs_additional_support);

    const auto resolved = registry.resolve_for(folder_path);
    REQUIRE(resolved);
    CHECK(resolved->id() == "local_fs");
}

TEST_CASE("StoragePluginManager persists installed plugins") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = create_signed_stub_storage_plugin_archive(
        archive_dir.path(), "mockcloud_compat", "MockCloud Compatibility", "mockcloud", "0.1.0");

    StoragePluginManager writer(config_dir.path().string(), {}, storage_package_test_keys());
    CHECK_FALSE(writer.is_installed("mockcloud_compat"));
    REQUIRE(writer.install_from_archive(archive_path));
    const auto manifest_path =
        StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string()) / "mockcloud_compat.json";
    CHECK(std::filesystem::exists(manifest_path));

    StoragePluginManager reader(config_dir.path().string());
    CHECK(reader.is_installed("mockcloud_compat"));
    const auto installed_ids = reader.installed_plugin_ids();
    REQUIRE(installed_ids.size() == 1);
    CHECK(installed_ids.front() == "mockcloud_compat");

    const auto plugin = reader.find_plugin_for_provider("mockcloud");
    REQUIRE(plugin.has_value());
    CHECK(plugin->id == "mockcloud_compat");
    CHECK(plugin->version == "0.1.0");
    CHECK(plugin->entry_point_kind == "external_process");
    CHECK(std::filesystem::exists(plugin->entry_point));
    CHECK(plugin->entry_point.find("packages/mockcloud_compat/0.1.0/") != std::string::npos);
}

TEST_CASE("StoragePluginLoader discovers plugin manifests from disk") {
    TempDir plugin_dir;

    const StoragePluginManifest manifest{.id = "network_drive_compat",
                                         .name = "Network Drive Compatibility",
                                         .description = "Adds compatibility helpers for mounted SMB and NFS shares.",
                                         .version = "0.2.0",
                                         .provider_ids = {"smb", "nfs"},
                                         .entry_point_kind = "external_process",
                                         .entry_point = "network_drive_compat"};
    REQUIRE(save_storage_plugin_manifest_to_file(manifest, plugin_dir.path() / "network_drive_compat.json"));

    StoragePluginLoader loader(plugin_dir.path());
    const auto discovered = loader.find_plugin("network_drive_compat");
    REQUIRE(discovered.has_value());
    CHECK(discovered->name == "Network Drive Compatibility");
    CHECK(discovered->version == "0.2.0");
    CHECK_FALSE(loader.supports_plugin(*discovered));

    const auto provider_manifest = loader.find_plugin_for_provider("smb");
    REQUIRE(provider_manifest.has_value());
    CHECK(provider_manifest->id == "network_drive_compat");
}

TEST_CASE("StoragePluginLoader backfills builtin entry points for legacy manifests") {
    TempDir plugin_dir;
    const auto legacy_manifest_path = plugin_dir.path() / "cloud_storage_compat.json";
    std::ofstream out(legacy_manifest_path);
    out << R"json({
  "id": "cloud_storage_compat",
  "name": "Cloud Storage Compatibility",
  "description": "Legacy manifest without entry point metadata.",
  "version": "1.0.0",
  "provider_ids": ["onedrive", "dropbox", "pcloud"]
})json";
    out.close();

    StoragePluginLoader loader(plugin_dir.path());
    const auto manifest = loader.find_plugin("cloud_storage_compat");
    REQUIRE(manifest.has_value());
    CHECK(manifest->entry_point_kind == "builtin_bundle");
    CHECK(manifest->entry_point == "cloud_storage_compat_bundle");
    CHECK(loader.supports_plugin(*manifest));
}

TEST_CASE("StoragePluginManager rejects unsupported directory-backed plugin manifests") {
    TempDir config_dir;
    const auto manifest_dir = StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string());

    const StoragePluginManifest manifest{.id = "network_drive_compat",
                                         .name = "Network Drive Compatibility",
                                         .description = "Adds compatibility helpers for mounted SMB and NFS shares.",
                                         .version = "0.2.0",
                                         .provider_ids = {"smb", "nfs"},
                                         .entry_point_kind = "external_process",
                                         .entry_point = "network_drive_compat"};
    REQUIRE(save_storage_plugin_manifest_to_file(manifest, manifest_dir / "network_drive_compat.json"));

    StoragePluginManager plugin_manager(config_dir.path().string());
    const auto discovered = plugin_manager.find_plugin("network_drive_compat");
    REQUIRE(discovered.has_value());
    CHECK(discovered->version == "0.2.0");

    std::string error;
    CHECK_FALSE(plugin_manager.install("network_drive_compat", &error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(plugin_manager.is_installed("network_drive_compat"));
}

TEST_CASE("StoragePluginManager installs supported external-process plugins") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir source_dir;
    const auto manifest_dir = StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string());
    const auto staged_binary = source_dir.path() / "mockcloud_compat";
    copy_file_with_permissions(storage_plugin_stub_path(), staged_binary);

    const StoragePluginManifest manifest{.id = "mockcloud_compat",
                                         .name = "MockCloud Compatibility",
                                         .description = "Test plugin backed by an external process stub.",
                                         .version = "0.1.0",
                                         .provider_ids = {"mockcloud"},
                                         .entry_point_kind = "external_process",
                                         .entry_point = staged_binary.string()};
    REQUIRE(save_storage_plugin_manifest_to_file(manifest, manifest_dir / "mockcloud_compat.json"));

    StoragePluginManager plugin_manager(config_dir.path().string());
    REQUIRE(plugin_manager.supports_plugin("mockcloud_compat"));
    REQUIRE(plugin_manager.install("mockcloud_compat"));
    CHECK(plugin_manager.is_installed("mockcloud_compat"));

    const auto installed_manifest = plugin_manager.find_plugin("mockcloud_compat");
    REQUIRE(installed_manifest.has_value());
    CHECK(installed_manifest->entry_point != staged_binary.string());
    CHECK(std::filesystem::exists(installed_manifest->entry_point));

    const auto package_dir = StoragePluginManager::package_directory_for_config_dir(config_dir.path().string()) /
                             "mockcloud_compat" / "0.1.0";
    CHECK(std::filesystem::exists(package_dir));
    CHECK(std::filesystem::exists(package_dir / staged_binary.filename()));
}

TEST_CASE("StoragePluginManager installs .aifsplugin archives with manifest and assets") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "mockcloud_compat.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    const std::string manifest = R"json({
  "id": "mockcloud_archive",
  "name": "MockCloud Archive Plugin",
  "description": "Archive-installed plugin backed by the storage plugin stub.",
  "version": "0.2.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/mockcloud_plugin",
  "package_paths": ["bin/mockcloud_plugin"]
})json";
    create_signed_storage_plugin_archive(archive_path, manifest, {{"bin/mockcloud_plugin", stub_payload}});

    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    std::string installed_plugin_id;
    REQUIRE(plugin_manager.install_from_archive(archive_path, &installed_plugin_id));
    CHECK(installed_plugin_id == "mockcloud_archive");
    CHECK(plugin_manager.is_installed("mockcloud_archive"));

    const auto plugin = plugin_manager.find_plugin("mockcloud_archive");
    REQUIRE(plugin.has_value());
    CHECK(std::filesystem::exists(plugin->entry_point));
    CHECK(plugin->entry_point.find("packages/mockcloud_archive/0.2.0/bin/mockcloud_plugin") != std::string::npos);
    CHECK(plugin->verified_signer_key_id == kStoragePackageTestKeyId);

    StoragePluginLoader loader(StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string()));
    StorageProviderRegistry registry;
    registry.register_builtin(std::make_shared<LocalFsProvider>());
    for (auto& provider : loader.create_detection_providers()) {
        registry.register_builtin(std::move(provider));
    }
    for (auto& provider : loader.create_providers_for_installed_plugins(plugin_manager.installed_plugin_ids())) {
        registry.register_builtin(std::move(provider));
    }

    TempDir data_dir;
    const auto cloud_dir = data_dir.path() / "MockCloud Archive";
    write_file(cloud_dir / "sample.txt");
    const auto resolved = registry.resolve_for(cloud_dir.string());
    REQUIRE(resolved);
    CHECK(resolved->id() == "mockcloud");
}

TEST_CASE("StoragePluginManager selects signed runtime-specific storage package paths and entitlement metadata") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "mockcloud_runtime.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    const auto runtime_entry = std::string("bin/") + storage_plugin_current_platform() + "/" +
                               storage_plugin_current_architecture() + "/mockcloud_plugin";
    const std::string manifest = fmt::format(R"json({{
  "id": "mockcloud_runtime",
  "name": "MockCloud Runtime Plugin",
  "description": "Archive-installed plugin with runtime-specific connector paths.",
  "version": "0.3.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "license_required": true,
  "product_id": "mockcloud_storage_support",
  "purchase_url": "https://plugins.example.invalid/mockcloud/buy",
  "runtimes": [
    {{
      "platforms": ["{}"],
      "architectures": ["{}"],
      "entry_point": "{}",
      "package_paths": ["{}"]
    }}
  ]
}})json",
                                             storage_plugin_current_platform(), storage_plugin_current_architecture(),
                                             runtime_entry, runtime_entry);
    create_signed_storage_plugin_archive(archive_path, manifest, {{runtime_entry, stub_payload}});

    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    std::string installed_plugin_id;
    REQUIRE(plugin_manager.install_from_archive(archive_path, &installed_plugin_id));
    CHECK(installed_plugin_id == "mockcloud_runtime");

    const auto plugin = plugin_manager.find_plugin("mockcloud_runtime");
    REQUIRE(plugin.has_value());
    CHECK(plugin->entry_point.find(runtime_entry) != std::string::npos);
    CHECK(plugin->license_required);
    CHECK(plugin->product_id == "mockcloud_storage_support");
    CHECK(plugin->purchase_url == "https://plugins.example.invalid/mockcloud/buy");
    CHECK(plugin->verified_signer_key_id == kStoragePackageTestKeyId);
}

TEST_CASE("StoragePluginManager rejects unsigned and tampered storage plugin archives") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "mockcloud_invalid.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    const std::string manifest = R"json({
  "id": "mockcloud_invalid",
  "name": "MockCloud Invalid Plugin",
  "description": "Invalid archive-installed plugin.",
  "version": "0.2.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/mockcloud_plugin",
  "package_paths": ["bin/mockcloud_plugin"]
})json";

    SECTION("unsigned") {
        create_zip_archive(archive_path, {{"manifest.json", manifest}, {"bin/mockcloud_plugin", stub_payload}});
        StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
        std::string error;
        CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
        CHECK(error.find("signature") != std::string::npos);
    }

    SECTION("tampered manifest") {
        auto entries = signed_storage_plugin_entries(manifest, {{"bin/mockcloud_plugin", stub_payload}});
        for (auto& [name, payload] : entries) {
            if (name == "manifest.json") {
                payload += "\n ";
            }
        }
        create_zip_archive(archive_path, entries);
        StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
        std::string error;
        CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
        CHECK(error.find("hash") != std::string::npos);
    }

    SECTION("tampered payload") {
        auto entries = signed_storage_plugin_entries(manifest, {{"bin/mockcloud_plugin", stub_payload}});
        for (auto& [name, payload] : entries) {
            if (name == "bin/mockcloud_plugin") {
                payload += "tampered";
            }
        }
        create_zip_archive(archive_path, entries);
        StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
        std::string error;
        CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
        CHECK(error.find("hash") != std::string::npos);
    }

    SECTION("unknown key id") {
        create_signed_storage_plugin_archive(archive_path, manifest, {{"bin/mockcloud_plugin", stub_payload}},
                                             "unknown-storage-package-key");
        StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
        std::string error;
        CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
        CHECK(error.find("untrusted") != std::string::npos);
    }
}

TEST_CASE("StoragePluginManager rejects unsafe storage plugin archive paths") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "unsafe_storage_plugin.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    const std::string traversal_manifest = R"json({
  "id": "unsafe_storage_plugin",
  "name": "Unsafe Storage Plugin",
  "description": "Invalid archive path.",
  "version": "0.1.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "../bin/mockcloud_plugin",
  "package_paths": ["bin/mockcloud_plugin"]
})json";
    create_signed_storage_plugin_archive(archive_path, traversal_manifest, {{"bin/mockcloud_plugin", stub_payload}});

    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    std::string error;
    CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
    CHECK(error.find("relative path") != std::string::npos);
}

TEST_CASE("StoragePluginManager rejects signed storage plugin archives with missing entry points") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "missing_entry_point.aifsplugin";
    const std::string manifest = R"json({
  "id": "missing_entry_point",
  "name": "Missing Entry Point",
  "description": "Signed manifest without the referenced connector file.",
  "version": "0.1.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/missing_connector",
  "package_paths": ["bin/missing_connector"]
})json";
    create_signed_storage_plugin_archive(archive_path, manifest, {});

    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    std::string error;
    CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
    CHECK(error.find("entry point") != std::string::npos);
}

TEST_CASE("StoragePluginManager installs plugins from remote manifests and archives") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    const auto manifest_dir = StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string());

    const auto archive_path = config_dir.path() / "remotecloud_support.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    const std::string package_manifest_url = "https://plugins.example.invalid/remotecloud/manifest.json";
    const std::string package_archive_url = "https://plugins.example.invalid/remotecloud/package.aifsplugin";
    const std::string remote_manifest = R"json({
  "id": "remotecloud_support",
  "name": "RemoteCloud Storage Support",
  "description": "Remote-installed plugin backed by an external helper.",
  "version": "1.2.0",
  "provider_ids": ["mockcloud"],
  "package_download_url": "https://plugins.example.invalid/remotecloud/package.aifsplugin",
  "package_sha256": "__PACKAGE_SHA__",
  "entry_point_kind": "external_process",
  "entry_point": "bin/remotecloud_plugin",
  "package_paths": ["bin/remotecloud_plugin"]
})json";
    std::string package_manifest = R"json({
  "id": "remotecloud_support",
  "name": "RemoteCloud Storage Support",
  "description": "Remote-installed plugin backed by an external helper.",
  "version": "1.2.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/remotecloud_plugin",
  "package_paths": ["bin/remotecloud_plugin"]
})json";
    create_signed_storage_plugin_archive(archive_path, package_manifest, {{"bin/remotecloud_plugin", stub_payload}});
    const auto archive_payload = read_binary_file(archive_path);
    const auto package_sha = sha256_hex(archive_payload);
    std::string resolved_remote_manifest = remote_manifest;
    const auto sha_marker = resolved_remote_manifest.find("__PACKAGE_SHA__");
    REQUIRE(sha_marker != std::string::npos);
    resolved_remote_manifest.replace(sha_marker, std::string("__PACKAGE_SHA__").size(), package_sha);

    const StoragePluginManifest seed_manifest{.id = "remotecloud_support",
                                              .name = "RemoteCloud Storage Support",
                                              .description = "Seed manifest that resolves to a remote package source.",
                                              .version = "0.0.0",
                                              .provider_ids = {"mockcloud"},
                                              .remote_manifest_url = package_manifest_url,
                                              .entry_point_kind = "external_process",
                                              .entry_point = "remotecloud_plugin"};
    REQUIRE(save_storage_plugin_manifest_to_file(seed_manifest, manifest_dir / "remotecloud_support.json"));

    auto download_fn = [package_manifest_url, package_archive_url, resolved_remote_manifest, archive_payload](
                           const std::string& url, const std::filesystem::path& destination,
                           StoragePluginPackageFetcher::ProgressCallback, StoragePluginPackageFetcher::CancelCheck) {
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (url == package_manifest_url) {
            out << resolved_remote_manifest;
            return;
        }
        if (url == package_archive_url) {
            out.write(archive_payload.data(), static_cast<std::streamsize>(archive_payload.size()));
            return;
        }
        throw std::runtime_error("Unexpected remote plugin URL");
    };

    StoragePluginManager plugin_manager(config_dir.path().string(), download_fn, storage_package_test_keys());
    REQUIRE(plugin_manager.supports_plugin("remotecloud_support"));
    REQUIRE(plugin_manager.install("remotecloud_support"));
    CHECK(plugin_manager.is_installed("remotecloud_support"));

    const auto plugin = plugin_manager.find_plugin("remotecloud_support");
    REQUIRE(plugin.has_value());
    CHECK(plugin->version == "1.2.0");
    CHECK(std::filesystem::exists(plugin->entry_point));
    CHECK(plugin->entry_point.find("packages/remotecloud_support/1.2.0/bin/remotecloud_plugin") != std::string::npos);
}

TEST_CASE("StoragePluginManager installs builtin plugin ids from local archives") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "onedrive_storage_support.aifsplugin";
    const auto entry_point = packaged_stub_entry_point();
    const auto plugin_payload = read_binary_file(storage_plugin_stub_path());
    const std::string manifest = R"json({
  "id": "onedrive_storage_support",
  "name": "OneDrive Storage Support",
  "description": "Local archive install for the builtin OneDrive plugin id.",
  "version": "9.9.9",
  "provider_ids": ["onedrive"],
  "entry_point_kind": "external_process",
  "entry_point": ")json" + entry_point +
                                 R"json(",
  "package_paths": [")json" + entry_point +
                                 R"json("]
})json";
    create_signed_storage_plugin_archive(archive_path, manifest, {{entry_point, plugin_payload}});

    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    std::string installed_plugin_id;
    REQUIRE(plugin_manager.install_from_archive(archive_path, &installed_plugin_id));
    CHECK(installed_plugin_id == "onedrive_storage_support");

    const auto plugin = plugin_manager.find_plugin("onedrive_storage_support");
    REQUIRE(plugin.has_value());
    CHECK(plugin->version == "9.9.9");
    CHECK(std::filesystem::exists(plugin->entry_point));
}

TEST_CASE("StoragePluginManager rejects archive plugins for another runtime") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "foreign_runtime_plugin.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    const std::string manifest = fmt::format(R"json({{
  "id": "foreign_runtime_plugin",
  "name": "Foreign Runtime Plugin",
  "description": "Archive install that targets a different runtime.",
  "version": "1.0.0",
  "provider_ids": ["mockcloud"],
  "platforms": ["{}"],
  "architectures": ["{}"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/foreign_runtime_plugin",
  "package_paths": ["bin/foreign_runtime_plugin"]
}})json",
                                             alternate_platform_name(), alternate_architecture_name());
    create_signed_storage_plugin_archive(archive_path, manifest, {{"bin/foreign_runtime_plugin", stub_payload}});

    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    std::string error;
    CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
    CHECK((error == "Plugin targets a different platform." || error == "Plugin targets a different CPU architecture."));
}

TEST_CASE("StoragePluginManager refreshes available plugins from a remote catalog") {
    TempDir config_dir;
    const std::string catalog_url = "https://plugins.example.invalid/storage/catalog.json";
    EnvVarGuard catalog_guard("AI_FILE_SORTER_STORAGE_PLUGIN_CATALOG_URL", catalog_url);
    const auto current_platform = storage_plugin_current_platform();
    const auto current_architecture = storage_plugin_current_architecture();
    const std::string catalog_json =
        fmt::format(R"json({{
  "plugins": [
    {{
      "id": "servercloud_support",
      "name": "ServerCloud Storage Support",
      "description": "Generic catalog entry.",
      "version": "1.5.0",
      "provider_ids": ["servercloud"],
      "remote_manifest_url": "https://plugins.example.invalid/storage/servercloud/generic-manifest.json",
      "entry_point_kind": "external_process",
      "entry_point": "servercloud_plugin"
    }},
    {{
      "id": "servercloud_support",
      "name": "ServerCloud Storage Support",
      "description": "Delivered from the remote plugin catalog.",
      "version": "2.0.0",
      "provider_ids": ["servercloud"],
      "platforms": ["{}"],
      "architectures": ["{}"],
      "remote_manifest_url": "https://plugins.example.invalid/storage/servercloud/manifest.json",
      "entry_point_kind": "external_process",
      "entry_point": "servercloud_plugin"
    }},
    {{
      "id": "servercloud_support",
      "name": "ServerCloud Storage Support",
      "description": "Wrong runtime variant.",
      "version": "9.9.9",
      "provider_ids": ["servercloud"],
      "platforms": ["{}"],
      "architectures": ["{}"],
      "remote_manifest_url": "https://plugins.example.invalid/storage/servercloud/foreign-manifest.json",
      "entry_point_kind": "external_process",
      "entry_point": "servercloud_plugin"
    }}
  ]
}})json",
                    current_platform, current_architecture, alternate_platform_name(), alternate_architecture_name());

    auto download_fn = [catalog_url, catalog_json](const std::string& url, const std::filesystem::path& destination,
                                                   StoragePluginPackageFetcher::ProgressCallback,
                                                   StoragePluginPackageFetcher::CancelCheck) {
        if (url != catalog_url) {
            throw std::runtime_error("Unexpected remote catalog URL");
        }
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        out << catalog_json;
    };

    StoragePluginManager manager(config_dir.path().string(), download_fn);
    REQUIRE(manager.remote_catalog_configured());
    REQUIRE(manager.refresh_remote_catalog());

    const auto plugin = manager.find_plugin("servercloud_support");
    REQUIRE(plugin.has_value());
    CHECK(plugin->name == "ServerCloud Storage Support");
    CHECK(plugin->version == "2.0.0");
    CHECK(plugin->platforms == std::vector<std::string>{current_platform});
    CHECK(plugin->architectures == std::vector<std::string>{current_architecture});

    StoragePluginManager reloaded(config_dir.path().string(), download_fn);
    const auto cached_plugin = reloaded.find_plugin("servercloud_support");
    REQUIRE(cached_plugin.has_value());
    CHECK(cached_plugin->version == "2.0.0");
}

TEST_CASE("StoragePluginManager reports when a remote catalog lacks a matching runtime") {
    TempDir config_dir;
    const std::string catalog_url = "https://plugins.example.invalid/storage/catalog.json";
    EnvVarGuard catalog_guard("AI_FILE_SORTER_STORAGE_PLUGIN_CATALOG_URL", catalog_url);
    const std::string catalog_json = fmt::format(R"json({{
  "plugins": [
    {{
      "id": "servercloud_support",
      "name": "ServerCloud Storage Support",
      "description": "Only published for a foreign runtime.",
      "version": "2.0.0",
      "provider_ids": ["servercloud"],
      "platforms": ["{}"],
      "architectures": ["{}"],
      "remote_manifest_url": "https://plugins.example.invalid/storage/servercloud/manifest.json",
      "entry_point_kind": "external_process",
      "entry_point": "servercloud_plugin"
    }}
  ]
}})json",
                                                 alternate_platform_name(), alternate_architecture_name());

    auto download_fn = [catalog_url, catalog_json](const std::string& url, const std::filesystem::path& destination,
                                                   StoragePluginPackageFetcher::ProgressCallback,
                                                   StoragePluginPackageFetcher::CancelCheck) {
        if (url != catalog_url) {
            throw std::runtime_error("Unexpected remote catalog URL");
        }
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        out << catalog_json;
    };

    StoragePluginManager manager(config_dir.path().string(), download_fn);
    REQUIRE(manager.remote_catalog_configured());

    std::string error;
    CHECK_FALSE(manager.refresh_remote_catalog(&error));
    CHECK(error == fmt::format("Plugin catalog does not contain any entries for this runtime ({}/{}).",
                               storage_plugin_current_platform(), storage_plugin_current_architecture()));
}

TEST_CASE("StoragePluginManager installs catalog plugins on demand") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    const std::string catalog_url = "https://plugins.example.invalid/storage/catalog.json";
    const std::string manifest_url = "https://plugins.example.invalid/storage/servercloud/manifest.json";
    const std::string archive_url = "https://plugins.example.invalid/storage/servercloud/package.aifsplugin";
    EnvVarGuard catalog_guard("AI_FILE_SORTER_STORAGE_PLUGIN_CATALOG_URL", catalog_url);

    const auto archive_path = config_dir.path() / "servercloud_support.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    const std::string archive_manifest = R"json({
  "id": "servercloud_support",
  "name": "ServerCloud Storage Support",
  "description": "Delivered from the remote plugin catalog.",
  "version": "2.1.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/servercloud_plugin",
  "package_paths": ["bin/servercloud_plugin"]
})json";
    create_signed_storage_plugin_archive(archive_path, archive_manifest, {{"bin/servercloud_plugin", stub_payload}});
    const auto archive_payload = read_binary_file(archive_path);
    const auto archive_sha = sha256_hex(archive_payload);

    const std::string catalog_json =
        fmt::format(R"json({{
  "plugins": [
    {{
      "id": "servercloud_support",
      "name": "ServerCloud Storage Support",
      "description": "Delivered from the remote plugin catalog.",
      "version": "2.1.0",
      "provider_ids": ["mockcloud"],
      "platforms": ["{}"],
      "architectures": ["{}"],
      "remote_manifest_url": "{}",
      "entry_point_kind": "external_process",
      "entry_point": "servercloud_plugin"
    }},
    {{
      "id": "servercloud_support",
      "name": "ServerCloud Storage Support",
      "description": "Foreign runtime variant.",
      "version": "3.0.0",
      "provider_ids": ["mockcloud"],
      "platforms": ["{}"],
      "architectures": ["{}"],
      "remote_manifest_url": "https://plugins.example.invalid/storage/servercloud/foreign-manifest.json",
      "entry_point_kind": "external_process",
      "entry_point": "servercloud_plugin"
    }}
  ]
}})json",
                    storage_plugin_current_platform(), storage_plugin_current_architecture(), manifest_url,
                    alternate_platform_name(), alternate_architecture_name());
    const std::string remote_manifest =
        fmt::format(R"json({{
  "id": "servercloud_support",
  "name": "ServerCloud Storage Support",
  "description": "Delivered from the remote plugin catalog.",
  "version": "2.1.0",
  "provider_ids": ["mockcloud"],
  "platforms": ["{}"],
  "architectures": ["{}"],
  "remote_manifest_url": "{}",
  "package_download_url": "{}",
  "package_sha256": "{}",
  "entry_point_kind": "external_process",
  "entry_point": "bin/servercloud_plugin",
  "package_paths": ["bin/servercloud_plugin"]
}})json",
                    storage_plugin_current_platform(), storage_plugin_current_architecture(), manifest_url, archive_url,
                    archive_sha);

    auto download_fn = [catalog_url, manifest_url, archive_url, catalog_json, remote_manifest, archive_payload](
                           const std::string& url, const std::filesystem::path& destination,
                           StoragePluginPackageFetcher::ProgressCallback, StoragePluginPackageFetcher::CancelCheck) {
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (url == catalog_url) {
            out << catalog_json;
            return;
        }
        if (url == manifest_url) {
            out << remote_manifest;
            return;
        }
        if (url == archive_url) {
            out.write(archive_payload.data(), static_cast<std::streamsize>(archive_payload.size()));
            return;
        }
        throw std::runtime_error("Unexpected remote catalog/plugin URL");
    };

    StoragePluginManager manager(config_dir.path().string(), download_fn, storage_package_test_keys());
    REQUIRE(manager.refresh_remote_catalog());
    REQUIRE(manager.install("servercloud_support"));
    CHECK(manager.is_installed("servercloud_support"));

    const auto plugin = manager.find_plugin("servercloud_support");
    REQUIRE(plugin.has_value());
    CHECK(plugin->version == "2.1.0");
    CHECK(std::filesystem::exists(plugin->entry_point));
}

TEST_CASE("StoragePluginManager updates installed plugins from remote manifests") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    const auto manifest_dir = StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string());

    const std::string package_manifest_url = "https://plugins.example.invalid/remotecloud/manifest.json";
    const std::string archive_v1_url = "https://plugins.example.invalid/remotecloud/package-v1.aifsplugin";
    const std::string archive_v2_url = "https://plugins.example.invalid/remotecloud/package-v2.aifsplugin";

    const auto archive_v1_path = config_dir.path() / "remotecloud_support-v1.aifsplugin";
    const auto archive_v2_path = config_dir.path() / "remotecloud_support-v2.aifsplugin";
    const auto stub_payload = read_binary_file(storage_plugin_stub_path());
    create_signed_storage_plugin_archive(archive_v1_path,
                                         R"json({
  "id": "remotecloud_support",
  "name": "RemoteCloud Storage Support",
  "description": "Version 1.0.0",
  "version": "1.0.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/remotecloud_plugin",
  "package_paths": ["bin/remotecloud_plugin"]
})json",
                                         {{"bin/remotecloud_plugin", stub_payload}});
    create_signed_storage_plugin_archive(archive_v2_path,
                                         R"json({
  "id": "remotecloud_support",
  "name": "RemoteCloud Storage Support",
  "description": "Version 1.3.0",
  "version": "1.3.0",
  "provider_ids": ["mockcloud"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/remotecloud_plugin",
  "package_paths": ["bin/remotecloud_plugin"]
})json",
                                         {{"bin/remotecloud_plugin", stub_payload + "v2"}});
    const auto archive_v1_payload = read_binary_file(archive_v1_path);
    const auto archive_v2_payload = read_binary_file(archive_v2_path);
    const auto archive_v1_sha = sha256_hex(archive_v1_payload);
    const auto archive_v2_sha = sha256_hex(archive_v2_payload);

    const StoragePluginManifest seed_manifest{.id = "remotecloud_support",
                                              .name = "RemoteCloud Storage Support",
                                              .description = "Seed manifest that resolves to a remote package source.",
                                              .version = "0.0.0",
                                              .provider_ids = {"mockcloud"},
                                              .remote_manifest_url = package_manifest_url,
                                              .entry_point_kind = "external_process",
                                              .entry_point = "remotecloud_plugin"};
    REQUIRE(save_storage_plugin_manifest_to_file(seed_manifest, manifest_dir / "remotecloud_support.json"));

    std::string current_remote_manifest = fmt::format(R"json({{
  "id": "remotecloud_support",
  "name": "RemoteCloud Storage Support",
  "description": "Version 1.0.0",
  "version": "1.0.0",
  "provider_ids": ["mockcloud"],
  "package_download_url": "{}",
  "package_sha256": "{}",
  "entry_point_kind": "external_process",
  "entry_point": "bin/remotecloud_plugin",
  "package_paths": ["bin/remotecloud_plugin"]
}})json",
                                                      archive_v1_url, archive_v1_sha);

    auto download_fn = [&current_remote_manifest, package_manifest_url, archive_v1_url, archive_v2_url,
                        archive_v1_payload, archive_v2_payload](
                           const std::string& url, const std::filesystem::path& destination,
                           StoragePluginPackageFetcher::ProgressCallback, StoragePluginPackageFetcher::CancelCheck) {
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (url == package_manifest_url) {
            out << current_remote_manifest;
            return;
        }
        if (url == archive_v1_url) {
            out.write(archive_v1_payload.data(), static_cast<std::streamsize>(archive_v1_payload.size()));
            return;
        }
        if (url == archive_v2_url) {
            out.write(archive_v2_payload.data(), static_cast<std::streamsize>(archive_v2_payload.size()));
            return;
        }
        throw std::runtime_error("Unexpected remote plugin URL");
    };

    StoragePluginManager plugin_manager(config_dir.path().string(), download_fn, storage_package_test_keys());
    REQUIRE(plugin_manager.install("remotecloud_support"));
    REQUIRE(plugin_manager.is_installed("remotecloud_support"));
    CHECK(plugin_manager.can_check_for_updates());
    CHECK_FALSE(plugin_manager.can_update("remotecloud_support"));

    current_remote_manifest = fmt::format(R"json({{
  "id": "remotecloud_support",
  "name": "RemoteCloud Storage Support",
  "description": "Version 1.3.0",
  "version": "1.3.0",
  "provider_ids": ["mockcloud"],
  "package_download_url": "{}",
  "package_sha256": "{}",
  "entry_point_kind": "external_process",
  "entry_point": "bin/remotecloud_plugin",
  "package_paths": ["bin/remotecloud_plugin"]
}})json",
                                          archive_v2_url, archive_v2_sha);

    REQUIRE(plugin_manager.refresh_remote_catalog());
    CHECK(plugin_manager.can_update("remotecloud_support"));

    REQUIRE(plugin_manager.update("remotecloud_support"));
    const auto plugin = plugin_manager.find_plugin("remotecloud_support");
    REQUIRE(plugin.has_value());
    CHECK(plugin->version == "1.3.0");
    CHECK(std::filesystem::exists(plugin->entry_point));
    CHECK(plugin->entry_point.find("packages/remotecloud_support/1.3.0/bin/remotecloud_plugin") != std::string::npos);
}

TEST_CASE("StoragePluginManager rejects plugin archives without manifest.json") {
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = archive_dir.path() / "broken_plugin.aifsplugin";
    create_zip_archive(archive_path, {{"README.txt", "missing manifest"}});

    StoragePluginManager plugin_manager(config_dir.path().string());
    std::string error;
    CHECK_FALSE(plugin_manager.install_from_archive(archive_path, nullptr, &error));
    CHECK(error.find("manifest.json") != std::string::npos);
}

TEST_CASE("StorageProviderRegistry resolves installed cloud provider ahead of local fallback") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;

    const auto archive_path = create_signed_stub_storage_plugin_archive(
        archive_dir.path(), "mockcloud_compat", "MockCloud Compatibility", "mockcloud", "0.1.0");
    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    REQUIRE(plugin_manager.install_from_archive(archive_path));
    StoragePluginLoader loader(StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string()));

    StorageProviderRegistry registry;
    auto local_provider = std::make_shared<LocalFsProvider>();
    registry.register_builtin(local_provider);
    for (auto& provider : loader.create_detection_providers()) {
        registry.register_builtin(std::move(provider));
    }
    for (auto& provider : loader.create_providers_for_installed_plugins(plugin_manager.installed_plugin_ids())) {
        registry.register_builtin(std::move(provider));
    }

    const std::string folder_path = "/Users/example/MockCloud Documents";
    const auto detection = registry.detect(folder_path);
    REQUIRE(detection.matched);
    CHECK(detection.provider_id == "mockcloud");
    CHECK_FALSE(detection.needs_additional_support);
    CHECK(detection.detection_source == "path_heuristic");

    const auto resolved = registry.resolve_for(folder_path);
    REQUIRE(resolved);
    CHECK(resolved->id() == "mockcloud");
}

TEST_CASE("UndoManager rejects external-process restores when revision metadata changed") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;
    TempDir data_dir;

    const auto archive_path = create_signed_stub_storage_plugin_archive(
        archive_dir.path(), "mockcloud_compat", "MockCloud Compatibility", "mockcloud", "0.1.0");
    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    REQUIRE(plugin_manager.install_from_archive(archive_path));

    StoragePluginLoader loader(StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string()));
    StorageProviderRegistry registry;
    registry.register_builtin(std::make_shared<LocalFsProvider>());
    for (auto& provider : loader.create_detection_providers()) {
        registry.register_builtin(std::move(provider));
    }
    for (auto& provider : loader.create_providers_for_installed_plugins(plugin_manager.installed_plugin_ids())) {
        registry.register_builtin(std::move(provider));
    }

    const auto cloud_dir = data_dir.path() / "MockCloud Documents";
    const auto source = cloud_dir / "report.docx";
    const auto destination = cloud_dir / "Sorted" / "report.docx";
    write_file(source);

    auto provider = registry.resolve_for(cloud_dir.string());
    REQUIRE(provider);
    REQUIRE(provider->id() == "mockcloud");

    const auto move_result = provider->move_entry(source.string(), destination.string());
    REQUIRE(move_result.success);

    {
        std::ofstream out(destination, std::ios::app);
        out << "changed";
    }

    const auto undo_dir = (cloud_dir / ".undo").string();
    UndoManager writer(undo_dir, &registry);
    REQUIRE(writer.save_plan(cloud_dir.string(), provider->id(),
                             {UndoManager::Entry{source.string(), destination.string(), move_result.metadata.size_bytes,
                                                 move_result.metadata.mtime, move_result.metadata.stable_identity,
                                                 move_result.metadata.revision_token}},
                             nullptr));

    UndoManager reader(undo_dir, &registry);
    const auto plan_path = reader.latest_plan_path();
    REQUIRE(plan_path.has_value());

    const auto undo_result = reader.undo_plan(*plan_path);
    CHECK(undo_result.restored == 0);
    CHECK(undo_result.skipped == 1);
    CHECK(std::filesystem::exists(destination));
}

TEST_CASE("StorageProviderRegistry resolves installed external process provider") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir source_dir;
    const auto manifest_dir = StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string());
    const auto staged_binary = source_dir.path() / "mockcloud_compat";
    copy_file_with_permissions(storage_plugin_stub_path(), staged_binary);

    const StoragePluginManifest manifest{.id = "mockcloud_compat",
                                         .name = "MockCloud Compatibility",
                                         .description = "Test plugin backed by an external process stub.",
                                         .version = "0.1.0",
                                         .provider_ids = {"mockcloud"},
                                         .entry_point_kind = "external_process",
                                         .entry_point = staged_binary.string()};
    REQUIRE(save_storage_plugin_manifest_to_file(manifest, manifest_dir / "mockcloud_compat.json"));

    StoragePluginLoader loader(manifest_dir);
    StoragePluginManager plugin_manager(config_dir.path().string());
    REQUIRE(plugin_manager.install("mockcloud_compat"));
    std::filesystem::remove(staged_binary);

    TempDir data_dir;
    const auto cloud_dir = data_dir.path() / "MockCloud Documents";
    write_file(cloud_dir / "sample.txt");

    StorageProviderRegistry registry;
    registry.register_builtin(std::make_shared<LocalFsProvider>());
    for (auto& provider : loader.create_detection_providers()) {
        registry.register_builtin(std::move(provider));
    }
    for (auto& provider : loader.create_providers_for_installed_plugins(plugin_manager.installed_plugin_ids())) {
        registry.register_builtin(std::move(provider));
    }

    const auto detection = registry.detect(cloud_dir.string());
    REQUIRE(detection.matched);
    CHECK(detection.provider_id == "mockcloud");
    CHECK_FALSE(detection.needs_additional_support);

    const auto resolved = registry.resolve_for(cloud_dir.string());
    REQUIRE(resolved);
    CHECK(resolved->id() == "mockcloud");

    const auto entries = resolved->list_directory(cloud_dir.string(), FileScanOptions::Files);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().file_name == "sample.txt");

    const auto original_path = cloud_dir / "sample.txt";
    const auto moved_path = cloud_dir / "sorted" / "sample.txt";
    const auto move_result = resolved->move_entry(original_path.string(), moved_path.string());
    CHECK(move_result.success);
    CHECK(std::filesystem::exists(moved_path));

    const auto undo_result = resolved->undo_move(original_path.string(), moved_path.string());
    CHECK(undo_result.success);
    CHECK(std::filesystem::exists(original_path));
}

TEST_CASE("StorageProviderRegistry resolves installed OneDrive external connector") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir archive_dir;
    TempDir onedrive_root;
    EnvVarGuard onedrive_guard("OneDrive", onedrive_root.path().string());

    const auto archive_path = create_signed_stub_storage_plugin_archive(
        archive_dir.path(), "onedrive_storage_support", "OneDrive Storage Support", "onedrive", "9.9.9");
    StoragePluginManager plugin_manager(config_dir.path().string(), {}, storage_package_test_keys());
    REQUIRE(plugin_manager.install_from_archive(archive_path));

    const auto plugin = plugin_manager.find_plugin("onedrive_storage_support");
    REQUIRE(plugin.has_value());
    CHECK(plugin->entry_point_kind == "external_process");
    CHECK(std::filesystem::exists(plugin->entry_point));
    CHECK(plugin->entry_point.find("packages/onedrive_storage_support/9.9.9/") != std::string::npos);

    StoragePluginLoader loader(StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string()));
    StorageProviderRegistry registry;
    registry.register_builtin(std::make_shared<LocalFsProvider>());
    for (auto& provider : loader.create_detection_providers()) {
        registry.register_builtin(std::move(provider));
    }
    for (auto& provider : loader.create_providers_for_installed_plugins(plugin_manager.installed_plugin_ids())) {
        registry.register_builtin(std::move(provider));
    }

    const auto detection = registry.detect(onedrive_root.path().string());
    REQUIRE(detection.matched);
    CHECK(detection.provider_id == "onedrive");
    CHECK_FALSE(detection.needs_additional_support);
    CHECK(detection.detection_source == "path_heuristic");

    const auto resolved = registry.resolve_for(onedrive_root.path().string());
    REQUIRE(resolved);
    CHECK(resolved->id() == "onedrive");

    const auto locked_file = onedrive_root.path() / "~$draft.docx";
    write_file(locked_file);
    const auto preflight =
        resolved->preflight_move(locked_file.string(), (onedrive_root.path() / "Sorted" / "draft.docx").string());
    CHECK_FALSE(preflight.allowed);
    CHECK(preflight.sync_locked);
    CHECK(preflight.should_retry);
}

TEST_CASE("StoragePluginManager uninstalls packaged external-process plugins") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt;
    TempDir config_dir;
    TempDir source_dir;
    const auto manifest_dir = StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string());
    const auto staged_binary = source_dir.path() / "mockcloud_compat";
    copy_file_with_permissions(storage_plugin_stub_path(), staged_binary);

    const StoragePluginManifest manifest{.id = "mockcloud_compat",
                                         .name = "MockCloud Compatibility",
                                         .description = "Test plugin backed by an external process stub.",
                                         .version = "0.1.0",
                                         .provider_ids = {"mockcloud"},
                                         .entry_point_kind = "external_process",
                                         .entry_point = staged_binary.string()};
    REQUIRE(save_storage_plugin_manifest_to_file(manifest, manifest_dir / "mockcloud_compat.json"));

    StoragePluginManager plugin_manager(config_dir.path().string());
    REQUIRE(plugin_manager.install("mockcloud_compat"));

    const auto package_root =
        StoragePluginManager::package_directory_for_config_dir(config_dir.path().string()) / "mockcloud_compat";
    REQUIRE(std::filesystem::exists(manifest_dir / "mockcloud_compat.json"));
    REQUIRE(std::filesystem::exists(package_root));

    REQUIRE(plugin_manager.uninstall("mockcloud_compat"));
    CHECK_FALSE(plugin_manager.is_installed("mockcloud_compat"));
    CHECK_FALSE(std::filesystem::exists(manifest_dir / "mockcloud_compat.json"));
    CHECK_FALSE(std::filesystem::exists(package_root));
}
