#include <catch2/catch_test_macros.hpp>

#include "LLMDownloader.hpp"
#include "LLMSelectionDialog.hpp"
#include "LLMSelectionDialogTestAccess.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"
#include "Utils.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>

#include <curl/curl.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#ifndef _WIN32
namespace {

std::string file_url_for(const std::filesystem::path& path)
{
    return std::string("file://") + path.string();
}

void write_bytes(const std::filesystem::path& path, std::size_t count)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    for (std::size_t i = 0; i < count; ++i) {
        out.put('x');
    }
}

bool wait_for_label(QLabel* label, const QString& starts_with, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (label && label->text().startsWith(starts_with)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return label && label->text().startsWith(starts_with);
}

} // namespace

TEST_CASE("Visual LLaVA entry shows missing env var state") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", std::nullopt);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);

    Settings settings;
    LLMSelectionDialog dialog(settings);

    const auto entry = LLMSelectionDialogTestAccess::llava_model_entry(dialog);
    REQUIRE(entry.status_label != nullptr);
    REQUIRE(entry.download_button != nullptr);
    CHECK(entry.status_label->text() ==
          QStringLiteral("Missing download URL environment variable (LLAVA_MODEL_URL)."));
    CHECK_FALSE(entry.download_button->isEnabled());
}

TEST_CASE("Visual LLaVA entry shows resume state for partial downloads") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    const std::filesystem::path source_file = temp.path() / "llava-model.gguf";
    write_bytes(source_file, 16);
    const std::string model_url = file_url_for(source_file);

    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", model_url);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);

    Settings settings;
    LLMSelectionDialog dialog(settings);

    auto entry = LLMSelectionDialogTestAccess::llava_model_entry(dialog);
    REQUIRE(entry.download_button != nullptr);
    REQUIRE(entry.status_label != nullptr);
    REQUIRE(entry.downloader != nullptr);

    const std::filesystem::path dest_path =
        Utils::make_default_path_to_file_from_download_url(model_url);
    write_bytes(dest_path, 4);
    LLMDownloader::LLMDownloaderTestAccess::set_resume_headers(*entry.downloader, 16);

    LLMSelectionDialogTestAccess::update_llava_model_entry(dialog);

    CHECK(entry.status_label->text() == QStringLiteral("Partial download detected. You can resume."));
    CHECK(entry.download_button->text() == QStringLiteral("Resume download"));
    CHECK(entry.download_button->isEnabled());
}

TEST_CASE("Visual LLaVA entry reports download errors") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    const std::filesystem::path source_file = temp.path() / "llava-model.gguf";
    write_bytes(source_file, 16);
    const std::string model_url = file_url_for(source_file);

    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", model_url);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);

    Settings settings;
    LLMSelectionDialog dialog(settings);
    LLMSelectionDialogTestAccess::set_network_available_override(dialog, true);

    auto entry = LLMSelectionDialogTestAccess::llava_model_entry(dialog);
    REQUIRE(entry.status_label != nullptr);
    REQUIRE(entry.downloader != nullptr);

    TestHooks::set_llm_download_probe([](long, const std::string&) {
        return CURLE_COULDNT_CONNECT;
    });

    LLMSelectionDialogTestAccess::start_llava_model_download(dialog);

    const bool updated = wait_for_label(entry.status_label,
                                        QStringLiteral("Download error:"),
                                        std::chrono::milliseconds(300));
    CHECK(updated);

    TestHooks::reset_llm_download_probe();
}
#endif
