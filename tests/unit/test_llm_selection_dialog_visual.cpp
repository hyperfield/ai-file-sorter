#include <catch2/catch_test_macros.hpp>

#include "LLMDownloader.hpp"
#include "LLMSelectionDialog.hpp"
#include "LLMSelectionDialogTestAccess.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"
#include "Utils.hpp"

#include <QComboBox>
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

void write_gguf_file(const std::filesystem::path& path, std::size_t size = 16)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write("GGUF", 4);
    for (std::size_t i = 4; i < size; ++i) {
        out.put('\0');
    }
}

void write_download_metadata(const std::filesystem::path& artifact_path, const std::string& url)
{
    std::ofstream meta(artifact_path.string() + ".aifs.meta", std::ios::trunc);
    meta << "url=" << url << "\n";
    meta << "content_length=16\n";
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

TEST_CASE("Visual model entry shows missing env var state") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", std::nullopt);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);
    EnvVarGuard gemma_model_guard("GEMMA3_4B_MODEL_URL", std::nullopt);
    EnvVarGuard gemma_mmproj_guard("GEMMA3_4B_MMPROJ_URL", std::nullopt);

    Settings settings;
    settings.set_visual_model_id("llava-v1.6-mistral-7b");
    LLMSelectionDialog dialog(settings);

    const auto entry = LLMSelectionDialogTestAccess::llava_model_entry(dialog);
    REQUIRE(entry.status_label != nullptr);
    REQUIRE(entry.download_button != nullptr);
    CHECK(entry.status_label->text() ==
          QStringLiteral("Missing download URL environment variable (LLAVA_MODEL_URL)."));
    CHECK_FALSE(entry.download_button->isEnabled());
}

TEST_CASE("Visual model entry shows resume state for partial downloads") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    const std::filesystem::path source_file = temp.path() / "llava-model.gguf";
    write_gguf_file(source_file, 16);
    const std::string model_url = file_url_for(source_file);

    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", model_url);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);
    EnvVarGuard gemma_model_guard("GEMMA3_4B_MODEL_URL", std::nullopt);
    EnvVarGuard gemma_mmproj_guard("GEMMA3_4B_MMPROJ_URL", std::nullopt);

    Settings settings;
    settings.set_visual_model_id("llava-v1.6-mistral-7b");
    LLMSelectionDialog dialog(settings);

    auto entry = LLMSelectionDialogTestAccess::llava_model_entry(dialog);
    REQUIRE(entry.download_button != nullptr);
    REQUIRE(entry.status_label != nullptr);
    REQUIRE(entry.downloader != nullptr);

    const std::filesystem::path dest_path(entry.downloader->get_download_destination());
    write_bytes(dest_path, 4);
    LLMDownloader::LLMDownloaderTestAccess::set_resume_headers(*entry.downloader, 16);

    LLMSelectionDialogTestAccess::update_llava_model_entry(dialog);

    CHECK(entry.status_label->text() == QStringLiteral("Partial download detected. You can resume."));
    CHECK(entry.download_button->text() == QStringLiteral("Resume download"));
    CHECK(entry.download_button->isEnabled());
}

TEST_CASE("Visual model entry reports download errors") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    const std::filesystem::path source_file = temp.path() / "llava-model.gguf";
    write_gguf_file(source_file, 16);
    const std::string model_url = file_url_for(source_file);

    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", model_url);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);
    EnvVarGuard gemma_model_guard("GEMMA3_4B_MODEL_URL", std::nullopt);
    EnvVarGuard gemma_mmproj_guard("GEMMA3_4B_MMPROJ_URL", std::nullopt);

    Settings settings;
    settings.set_visual_model_id("llava-v1.6-mistral-7b");
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

TEST_CASE("Visual backend selection switches descriptor-driven download state") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", std::nullopt);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);
    EnvVarGuard gemma_model_guard("GEMMA3_4B_MODEL_URL", std::nullopt);
    EnvVarGuard gemma_mmproj_guard("GEMMA3_4B_MMPROJ_URL", std::nullopt);

    Settings settings;
    settings.set_visual_model_id("llava-v1.6-mistral-7b");

    LLMSelectionDialog dialog(settings);

    CHECK(LLMSelectionDialogTestAccess::selected_visual_model_id(dialog) == "llava-v1.6-mistral-7b");

    const auto default_entry =
        LLMSelectionDialogTestAccess::visual_entry_for_env_var(dialog, "LLAVA_MODEL_URL");
    REQUIRE(default_entry.status_label != nullptr);
    CHECK(default_entry.status_label->text() ==
          QStringLiteral("Missing download URL environment variable (LLAVA_MODEL_URL)."));

    LLMSelectionDialogTestAccess::select_visual_backend(dialog, "gemma-3-4b-it");

    CHECK(LLMSelectionDialogTestAccess::selected_visual_model_id(dialog) == "gemma-3-4b-it");

    const auto gemma_entry =
        LLMSelectionDialogTestAccess::visual_entry_for_env_var(dialog, "GEMMA3_4B_MODEL_URL");
    REQUIRE(gemma_entry.status_label != nullptr);
    CHECK(gemma_entry.status_label->text() ==
          QStringLiteral("Missing download URL environment variable (GEMMA3_4B_MODEL_URL)."));
}

TEST_CASE("Visual dialog defaults to recommended Qwen backend") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    LLMSelectionDialog dialog(settings);

    CHECK(settings.get_visual_model_id() == "qwen3.5-4b");
    CHECK(LLMSelectionDialogTestAccess::selected_visual_model_id(dialog) == "qwen3.5-4b");
    CHECK(LLMSelectionDialogTestAccess::selected_visual_model_label(dialog) ==
          "Qwen3.5 4B (Recommended)");
    auto* combo = LLMSelectionDialogTestAccess::visual_model_combo(dialog);
    REQUIRE(combo != nullptr);
    CHECK(combo->currentIndex() == 0);
}

TEST_CASE("Visual dialog does not mark another backend's legacy generic mmproj as downloaded") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    const std::string llava_model_url = "https://llava.example/models/llava-model.gguf";
    const std::string llava_mmproj_url = "https://llava.example/models/mmproj-model-f16.gguf";
    const std::string gemma_model_url = "https://gemma.example/models/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string gemma_mmproj_url = "https://gemma.example/models/mmproj-model-f16.gguf";

    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", llava_model_url);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", llava_mmproj_url);
    EnvVarGuard gemma_model_guard("GEMMA3_4B_MODEL_URL", gemma_model_url);
    EnvVarGuard gemma_mmproj_guard("GEMMA3_4B_MMPROJ_URL", gemma_mmproj_url);

    const auto legacy_mmproj_path =
        std::filesystem::path(Utils::make_default_path_to_file_from_download_url(llava_mmproj_url));
    write_gguf_file(legacy_mmproj_path, 16);
    write_download_metadata(legacy_mmproj_path, llava_mmproj_url);

    const auto* gemma_descriptor = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(gemma_descriptor != nullptr);
    const auto gemma_model_path =
        visual_artifact_storage_path(*gemma_descriptor, gemma_descriptor->artifacts[0]);
    write_gguf_file(gemma_model_path, 16);

    Settings settings;
    settings.set_visual_model_id("gemma-3-4b-it");

    LLMSelectionDialog dialog(settings);

    const auto gemma_mmproj_entry =
        LLMSelectionDialogTestAccess::visual_entry_for_env_var(dialog, "GEMMA3_4B_MMPROJ_URL");
    REQUIRE(gemma_mmproj_entry.status_label != nullptr);
    CHECK(gemma_mmproj_entry.status_label->text() == QStringLiteral("Download required."));
}

TEST_CASE("Visual dialog accepts the legacy LLaVA generic mmproj without metadata") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    const std::string model_url = "https://llava.example/models/llava-model.gguf";
    const std::string mmproj_url = "https://llava.example/models/mmproj-model-f16.gguf";

    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", model_url);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", mmproj_url);
    EnvVarGuard gemma_model_guard("GEMMA3_4B_MODEL_URL", std::nullopt);
    EnvVarGuard gemma_mmproj_guard("GEMMA3_4B_MMPROJ_URL", std::nullopt);

    const auto legacy_mmproj_path =
        std::filesystem::path(Utils::make_default_path_to_file_from_download_url(mmproj_url));
    write_gguf_file(legacy_mmproj_path, 16);

    Settings settings;
    settings.set_visual_model_id("llava-v1.6-mistral-7b");

    LLMSelectionDialog dialog(settings);

    const auto llava_mmproj_entry =
        LLMSelectionDialogTestAccess::visual_entry_for_env_var(dialog, "LLAVA_MMPROJ_URL");
    REQUIRE(llava_mmproj_entry.status_label != nullptr);
    CHECK(llava_mmproj_entry.status_label->text() == QStringLiteral("Model ready."));
}

TEST_CASE("Visual dialog does not mark invalid preferred artifacts as ready") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    const std::string model_url = "https://gemma.example/models/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string mmproj_url = "https://gemma.example/models/mmproj-gemma-3-4b-it-Q4_K_M.gguf";

    EnvVarGuard llava_model_guard("LLAVA_MODEL_URL", std::nullopt);
    EnvVarGuard llava_mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);
    EnvVarGuard gemma_model_guard("GEMMA3_4B_MODEL_URL", model_url);
    EnvVarGuard gemma_mmproj_guard("GEMMA3_4B_MMPROJ_URL", mmproj_url);

    const auto* gemma_descriptor = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(gemma_descriptor != nullptr);
    write_gguf_file(visual_artifact_storage_path(*gemma_descriptor, gemma_descriptor->artifacts[1]), 16);
    const auto gemma_model_path =
        visual_artifact_storage_path(*gemma_descriptor, gemma_descriptor->artifacts[0]);
    write_bytes(gemma_model_path, 16);

    Settings settings;
    settings.set_visual_model_id("gemma-3-4b-it");
    LLMSelectionDialog dialog(settings);

    const auto gemma_model_entry =
        LLMSelectionDialogTestAccess::visual_entry_for_env_var(dialog, "GEMMA3_4B_MODEL_URL");
    REQUIRE(gemma_model_entry.status_label != nullptr);
    CHECK(gemma_model_entry.status_label->text()
          == QStringLiteral("Downloaded file is invalid or incomplete. Delete it and download again."));
}
#endif
