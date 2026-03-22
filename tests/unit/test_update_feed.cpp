#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "UpdateFeed.hpp"
#include "UpdateInstaller.hpp"
#include "UpdateInstallerTestAccess.hpp"
#include "TestHelpers.hpp"

#include <QByteArrayView>
#include <QCryptographicHash>

#include <filesystem>
#include <fstream>
#include <vector>

#include <zip.h>

namespace {

std::string sha256_hex(std::string_view payload)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView(payload.data(), static_cast<qsizetype>(payload.size())));
    return hash.result().toHex().toStdString();
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void create_zip_archive(const std::filesystem::path& archive_path,
                        const std::vector<std::pair<std::string, std::string>>& entries)
{
    int error_code = 0;
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error_code);
    REQUIRE(archive != nullptr);

    for (const auto& [name, payload] : entries) {
        zip_source_t* source = zip_source_buffer(archive,
                                                 payload.data(),
                                                 static_cast<zip_uint64_t>(payload.size()),
                                                 0);
        REQUIRE(source != nullptr);
        const zip_int64_t index = zip_file_add(archive,
                                               name.c_str(),
                                               source,
                                               ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        REQUIRE(index >= 0);
    }

    REQUIRE(zip_close(archive) == 0);
}

} // namespace

TEST_CASE("UpdateFeed selects the correct platform stream") {
    const std::string json = R"json(
        {
            "update": {
                "windows": {
                    "current_version": "1.8.0",
                    "min_version": "1.7.0",
                    "download_url": "https://filesorter.app/windows",
                    "installer_url": "https://filesorter.app/aifs-1.8.0.exe",
                    "installer_sha256": "ABCDEF"
                },
                "macos": {
                    "current_version": "1.8.1",
                    "min_version": "1.6.0",
                    "download_url": "https://filesorter.app/macos"
                },
                "linux": {
                    "current_version": "1.8.2",
                    "min_version": "1.5.0",
                    "download_url": "https://filesorter.app/linux"
                }
            }
        }
    )json";

    const auto windows = UpdateFeed::parse_for_platform(json, UpdateFeed::Platform::Windows);
    REQUIRE(windows.has_value());
    CHECK(windows->current_version == "1.8.0");
    CHECK(windows->min_version == "1.7.0");
    CHECK(windows->download_url == "https://filesorter.app/windows");
    CHECK(windows->installer_url == "https://filesorter.app/aifs-1.8.0.exe");
    CHECK(windows->installer_sha256 == "abcdef");

    const auto macos = UpdateFeed::parse_for_platform(json, UpdateFeed::Platform::MacOS);
    REQUIRE(macos.has_value());
    CHECK(macos->current_version == "1.8.1");
    CHECK(macos->download_url == "https://filesorter.app/macos");
    CHECK(macos->installer_url.empty());

    const auto linux = UpdateFeed::parse_for_platform(json, UpdateFeed::Platform::Linux);
    REQUIRE(linux.has_value());
    CHECK(linux->current_version == "1.8.2");
    CHECK(linux->download_url == "https://filesorter.app/linux");
}

TEST_CASE("UpdateFeed falls back to the legacy single-stream schema") {
    const std::string json = R"json(
        {
            "update": {
                "current_version": "1.7.1",
                "min_version": "1.6.0",
                "download_url": "https://filesorter.app/download"
            }
        }
    )json";

    const auto info = UpdateFeed::parse_for_platform(json, UpdateFeed::Platform::Windows);
    REQUIRE(info.has_value());
    CHECK(info->current_version == "1.7.1");
    CHECK(info->min_version == "1.6.0");
    CHECK(info->download_url == "https://filesorter.app/download");
}

TEST_CASE("UpdateInstaller downloads, verifies, and reuses a cached installer") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    const std::string payload = "signed-installer-payload";
    const std::string expected_sha256 = sha256_hex(payload);

    int download_calls = 0;
    std::filesystem::path prepared_path;
    std::filesystem::path launched_path;

    UpdateInstaller installer(
        settings,
        [&](const std::string&,
            const std::filesystem::path& destination_path,
            UpdateInstaller::ProgressCallback progress_cb,
            UpdateInstaller::CancelCheck) {
            ++download_calls;
            prepared_path = destination_path;
            if (progress_cb) {
                progress_cb(0.5, "Downloading");
                progress_cb(1.0, "Downloaded");
            }
            std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
            out << payload;
        },
        [&](const std::filesystem::path& path) {
            launched_path = path;
            return true;
        });

    UpdateInfo info;
    info.current_version = "1.8.0";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.exe";
    info.installer_sha256 = expected_sha256;

    const auto first = installer.prepare(info);
    REQUIRE(first.status == UpdatePreparationResult::Status::Ready);
    CHECK(download_calls == 1);
    CHECK(std::filesystem::exists(first.installer_path));
    CHECK(first.installer_path.extension() == ".exe");
    CHECK(prepared_path.extension() == ".part");

    const auto second = installer.prepare(info);
    REQUIRE(second.status == UpdatePreparationResult::Status::Ready);
    CHECK(download_calls == 1);
    CHECK(second.installer_path == first.installer_path);

    CHECK(installer.launch(second.installer_path));
    CHECK(launched_path == second.installer_path);
}

TEST_CASE("UpdateInstaller rejects installers that fail SHA-256 verification") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    UpdateInstaller installer(
        settings,
        [&](const std::string&,
            const std::filesystem::path& destination_path,
            UpdateInstaller::ProgressCallback,
            UpdateInstaller::CancelCheck) {
            std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
            out << "tampered";
        });

    UpdateInfo info;
    info.current_version = "1.8.0";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.exe";
    info.installer_sha256 = sha256_hex("expected");

    const auto result = installer.prepare(info);
    REQUIRE(result.status == UpdatePreparationResult::Status::Failed);
    CHECK(result.message.find("SHA-256") != std::string::npos);
    CHECK(result.installer_path.empty());
}

TEST_CASE("UpdateInstaller extracts installer payloads from ZIP update packages") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    const std::string installer_payload = "installer-from-zip";
    const auto archive_path = tmp.path() / "AIFileSorterSetup.zip";
    create_zip_archive(archive_path, {{"nested/AIFileSorterSetup.exe", installer_payload}});
    const std::string expected_sha256 = sha256_hex(read_file(archive_path));

    int download_calls = 0;
    UpdateInstaller installer(
        settings,
        [&](const std::string&,
            const std::filesystem::path& destination_path,
            UpdateInstaller::ProgressCallback,
            UpdateInstaller::CancelCheck) {
            ++download_calls;
            std::filesystem::copy_file(
                archive_path,
                destination_path,
                std::filesystem::copy_options::overwrite_existing);
        });

    UpdateInfo info;
    info.current_version = "1.8.0";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.zip";
    info.installer_sha256 = expected_sha256;

    const auto first = installer.prepare(info);
    REQUIRE(first.status == UpdatePreparationResult::Status::Ready);
    CHECK(download_calls == 1);
    CHECK(first.installer_path.extension() == ".exe");
    CHECK(first.installer_path.filename() == "AIFileSorterSetup.exe");
    CHECK(read_file(first.installer_path) == installer_payload);

    const auto second = installer.prepare(info);
    REQUIRE(second.status == UpdatePreparationResult::Status::Ready);
    CHECK(download_calls == 1);
    CHECK(second.installer_path == first.installer_path);
}

TEST_CASE("UpdateInstaller rejects ZIP update packages without an installer payload") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    const auto archive_path = tmp.path() / "AIFileSorterSetup.zip";
    create_zip_archive(archive_path, {{"README.txt", "no installer here"}});
    const std::string expected_sha256 = sha256_hex(read_file(archive_path));

    UpdateInstaller installer(
        settings,
        [&](const std::string&,
            const std::filesystem::path& destination_path,
            UpdateInstaller::ProgressCallback,
            UpdateInstaller::CancelCheck) {
            std::filesystem::copy_file(
                archive_path,
                destination_path,
                std::filesystem::copy_options::overwrite_existing);
        });

    UpdateInfo info;
    info.current_version = "1.8.0";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.zip";
    info.installer_sha256 = expected_sha256;

    const auto result = installer.prepare(info);
    REQUIRE(result.status == UpdatePreparationResult::Status::Failed);
    CHECK(result.message.find("ZIP archive") != std::string::npos);
    CHECK(result.installer_path.empty());
}

TEST_CASE("UpdateInstaller redownloads cached installers that fail verification") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    const std::string payload = "fresh-installer-payload";
    const std::string expected_sha256 = sha256_hex(payload);

    int download_calls = 0;
    UpdateInstaller installer(
        settings,
        [&](const std::string&,
            const std::filesystem::path& destination_path,
            UpdateInstaller::ProgressCallback,
            UpdateInstaller::CancelCheck) {
            ++download_calls;
            std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
            out << payload;
        });

    UpdateInfo info;
    info.current_version = "1.8.0";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.exe";
    info.installer_sha256 = expected_sha256;

    const auto first = installer.prepare(info);
    REQUIRE(first.status == UpdatePreparationResult::Status::Ready);
    CHECK(download_calls == 1);

    {
        std::ofstream out(first.installer_path, std::ios::binary | std::ios::trunc);
        out << "tampered-cache";
    }

    const auto second = installer.prepare(info);
    REQUIRE(second.status == UpdatePreparationResult::Status::Ready);
    CHECK(download_calls == 2);
    CHECK(second.installer_path == first.installer_path);
    CHECK(read_file(second.installer_path) == payload);
}

TEST_CASE("UpdateInstaller reports canceled downloads and removes partial files") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    std::filesystem::path partial_path;
    UpdateInstaller installer(
        settings,
        [&](const std::string&,
            const std::filesystem::path& destination_path,
            UpdateInstaller::ProgressCallback,
            UpdateInstaller::CancelCheck cancel_check) {
            partial_path = destination_path;
            std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
            out << "partial";
            if (cancel_check && cancel_check()) {
                throw UpdateInstaller::DownloadCanceledError();
            }
        });

    UpdateInfo info;
    info.current_version = "1.8.0";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.exe";
    info.installer_sha256 = sha256_hex("payload");

    const auto result = installer.prepare(info, {}, []() { return true; });
    REQUIRE(result.status == UpdatePreparationResult::Status::Canceled);
    CHECK(result.message.find("cancel") != std::string::npos);
    CHECK(result.installer_path.empty());
    CHECK_FALSE(partial_path.empty());
    CHECK_FALSE(std::filesystem::exists(partial_path));
}

TEST_CASE("UpdateInstaller requires installer metadata before preparing") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    UpdateInstaller installer(settings);

    UpdateInfo missing_url;
    missing_url.current_version = "1.8.0";
    missing_url.installer_sha256 = sha256_hex("payload");

    const auto missing_url_result = installer.prepare(missing_url);
    REQUIRE(missing_url_result.status == UpdatePreparationResult::Status::Failed);
    CHECK(missing_url_result.message.find("URL") != std::string::npos);

    UpdateInfo missing_sha;
    missing_sha.current_version = "1.8.0";
    missing_sha.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.exe";

    const auto missing_sha_result = installer.prepare(missing_sha);
    REQUIRE(missing_sha_result.status == UpdatePreparationResult::Status::Failed);
    CHECK(missing_sha_result.message.find("SHA-256") != std::string::npos);
}

TEST_CASE("UpdateInstaller builds launch requests for EXE and MSI installers") {
    const auto exe_request = UpdateInstallerTestAccess::build_launch_request(
        std::filesystem::path("C:/Program Files/AI File Sorter/AIFileSorterSetup.exe"));
    CHECK(exe_request.program == "C:/Program Files/AI File Sorter/AIFileSorterSetup.exe");
    CHECK(exe_request.arguments.empty());

    const auto msi_request = UpdateInstallerTestAccess::build_launch_request(
        std::filesystem::path("C:/Program Files/AI File Sorter/AIFileSorterSetup.MSI"));
    CHECK(msi_request.program == "msiexec.exe");
    REQUIRE(msi_request.arguments.size() == 2);
    CHECK(msi_request.arguments[0] == "/i");
    CHECK(msi_request.arguments[1] == "C:/Program Files/AI File Sorter/AIFileSorterSetup.MSI");
}

TEST_CASE("UpdateInstaller auto-install support remains Windows-only") {
    TempDir tmp;
    EnvVarGuard config_root("AI_FILE_SORTER_CONFIG_DIR", tmp.path().string());

    Settings settings;
    UpdateInstaller installer(settings);

    UpdateInfo info;
    info.current_version = "1.8.0";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.exe";
    info.installer_sha256 = sha256_hex("payload");

#ifdef _WIN32
    CHECK(installer.supports_auto_install(info));
#else
    CHECK_FALSE(installer.supports_auto_install(info));
#endif
}
