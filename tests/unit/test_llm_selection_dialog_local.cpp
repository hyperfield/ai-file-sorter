#include <catch2/catch_test_macros.hpp>

#include "LLMDownloader.hpp"
#include "LLMSelectionDialog.hpp"
#include "LLMSelectionDialogTestAccess.hpp"
#include "LlmCatalog.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"
#include "VisualModelCatalog.hpp"

#include <filesystem>
#include <fstream>

namespace {
constexpr char kLegacyLlama3BQ4Url[] =
    "https://huggingface.co/Mungert/Llama-3.2-3B-Instruct-GGUF/resolve/main/"
    "Llama-3.2-3B-Instruct-bf16-q4_k.gguf";

void write_gguf_file(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    const char magic[] = {'G', 'G', 'U', 'F'};
    out.write(magic, sizeof(magic));
    out.put('\0');
}
} // namespace

TEST_CASE("LLM selection dialog lists built-in local models in Gemma Mistral Gemma order") {
    QtAppContext qt;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();

    LLMSelectionDialog dialog(settings);
    const std::vector<std::string> labels =
        LLMSelectionDialogTestAccess::local_builtin_labels(dialog);

    REQUIRE(labels.size() == 3);
    CHECK(labels[0] == default_llm_label_for_choice(LLMChoice::Local_4b_Gemma).toStdString());
    CHECK(labels[1] == default_llm_label_for_choice(LLMChoice::Local_7b).toStdString());
    CHECK(labels[2] == default_llm_label_for_choice(LLMChoice::Local_7b_Gemma).toStdString());
    CHECK(labels[0].find("Recommended") == std::string::npos);
    CHECK(labels[1].find("Recommended") == std::string::npos);
    CHECK(labels[2].find("Recommended") == std::string::npos);
}

TEST_CASE("LLM selection dialog defaults to the Gemma 4B local model") {
    QtAppContext qt;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();

    LLMSelectionDialog dialog(settings);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_4b_Gemma);
}

TEST_CASE("LLM selection dialog keeps the legacy LLaMa choice when the previous Q4 artifact exists") {
    QtAppContext qt;
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    write_gguf_file(Utils::make_default_path_to_file_from_download_url(kLegacyLlama3BQ4Url));

    Settings settings;
    settings.load();
    settings.set_llm_choice(LLMChoice::Local_3b_legacy);

    LLMSelectionDialog dialog(settings);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_3b_legacy);
}

TEST_CASE("LLM selection dialog downloads Gemma 4B to the shared visual-model path") {
    QtAppContext qt;
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
    EnvVarGuard localappdata_guard("LOCALAPPDATA", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard local_url_guard(
        "LOCAL_LLM_3B_DOWNLOAD_URL",
        std::string("https://local.example/models/gemma-3-4b-it-Q4_K_M.gguf"));
    EnvVarGuard visual_url_guard(
        "GEMMA3_4B_MODEL_URL",
        std::string("https://visual.example/downloads/gemma-3-4b-it-Q4_K_M.gguf"));

    const auto* descriptor = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(descriptor != nullptr);
    REQUIRE_FALSE(descriptor->artifacts.empty());
    REQUIRE(descriptor->artifacts.front().kind == VisualModelArtifactKind::Model);
    const auto shared_model_path =
        visual_artifact_storage_path(*descriptor, descriptor->artifacts.front());

    Settings settings;
    settings.load();
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);

    LLMSelectionDialog dialog(settings);
    auto* downloader = LLMSelectionDialogTestAccess::local_downloader(dialog);
    REQUIRE(downloader != nullptr);

    CHECK(std::filesystem::path(downloader->get_download_destination()) == shared_model_path);
    CHECK(std::filesystem::path(downloader->get_download_destination())
          != std::filesystem::path(Utils::make_default_path_to_file_from_download_url(
              "https://local.example/models/gemma-3-4b-it-Q4_K_M.gguf")));
}
