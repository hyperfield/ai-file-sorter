#include <catch2/catch_test_macros.hpp>

#include "LLMDownloader.hpp"
#include "LLMSelectionDialog.hpp"
#include "LLMSelectionDialogTestAccess.hpp"
#include "LlmCatalog.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"
#include "VisualModelCatalog.hpp"

#include <QComboBox>
#include <QRadioButton>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {
constexpr char kLegacyLlama3BQ4Url[] =
    "https://huggingface.co/Mungert/Llama-3.2-3B-Instruct-GGUF/resolve/main/"
    "Llama-3.2-3B-Instruct-bf16-q4_k.gguf";
constexpr char kMistralUrl[] = "https://example.com/mistral-7b-instruct-v0.2.Q5_K_M.gguf";

struct LlmStorageOverrideGuard {
    std::string previous = Utils::get_llm_storage_directory_override();

    ~LlmStorageOverrideGuard()
    {
        Utils::set_llm_storage_directory_override(previous);
    }
};

void select_builtin_model(LLMSelectionDialog& dialog, LLMChoice choice)
{
    const QString label = default_llm_label_for_choice(choice);
    const auto radios = dialog.findChildren<QRadioButton*>();
    const auto selected = std::find_if(radios.begin(), radios.end(), [&label](const auto* radio) {
        return radio->text() == label;
    });
    REQUIRE(selected != radios.end());
    REQUIRE_FALSE((*selected)->isHidden());
    (*selected)->click();
}

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

TEST_CASE("LLM selection dialog hides retired models on fresh installations") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    settings.set_llm_storage_dir((config_dir.path() / "models").string());

    LLMSelectionDialog dialog(settings);
    const std::vector<std::string> labels =
        LLMSelectionDialogTestAccess::local_builtin_labels(dialog);

    REQUIRE(labels.size() == 3);
    CHECK(labels[0] == default_llm_label_for_choice(LLMChoice::Local_4b_Gemma).toStdString());
    CHECK(labels[1] == default_llm_label_for_choice(LLMChoice::Local_4b_Qwen35).toStdString());
    CHECK(labels[2] == default_llm_label_for_choice(LLMChoice::Local_7b_Gemma).toStdString());
    CHECK(labels[0].find("Recommended") == std::string::npos);
    CHECK(labels[1].find("Recommended") == std::string::npos);
    CHECK(labels[2].find("Recommended") == std::string::npos);
}

TEST_CASE("LLM selection dialog retains installed Mistral without changing the saved model") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard mistral_url_guard("LOCAL_LLM_7B_DOWNLOAD_URL", std::string(kMistralUrl));

    Settings settings;
    settings.load();
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);
    settings.set_llm_storage_dir((temp.path() / "models").string());
    REQUIRE(settings.save());
    const auto model_path = temp.path() / "models" / "mistral-7b-instruct-v0.2.Q5_K_M.gguf";
    write_gguf_file(model_path);

    LLMSelectionDialog dialog(settings);
    const auto labels = LLMSelectionDialogTestAccess::local_builtin_labels(dialog);
    REQUIRE(labels.size() == 4);
    CHECK(labels[0] == default_llm_label_for_choice(LLMChoice::Local_4b_Gemma).toStdString());
    CHECK(labels[1] == default_llm_label_for_choice(LLMChoice::Local_4b_Qwen35).toStdString());
    CHECK(labels[2] == default_llm_label_for_choice(LLMChoice::Local_7b).toStdString());
    CHECK(labels[3] == default_llm_label_for_choice(LLMChoice::Local_7b_Gemma).toStdString());
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_4b_Gemma);
    select_builtin_model(dialog, LLMChoice::Local_7b);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_7b);
    auto* downloader = LLMSelectionDialogTestAccess::local_downloader(dialog);
    REQUIRE(downloader != nullptr);
    CHECK(std::filesystem::path(downloader->get_download_destination()) == model_path);
    CHECK(settings.get_llm_choice() == LLMChoice::Local_4b_Gemma);
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_llm_choice() == LLMChoice::Local_4b_Gemma);
}

TEST_CASE("LLM selection dialog restores saved Mistral when its weights are missing") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard mistral_url_guard("LOCAL_LLM_7B_DOWNLOAD_URL", std::string(kMistralUrl));

    const auto settings_dir = temp.path() / "AIFileSorter";
    std::filesystem::create_directories(settings_dir);
    std::ofstream(settings_dir / "config.ini") << "[Settings]\nLLMChoice=Local_7b\n";
    Settings settings;
    REQUIRE(settings.load());
    REQUIRE(settings.get_llm_choice() == LLMChoice::Local_7b);
    settings.set_llm_storage_dir((temp.path() / "models").string());
    REQUIRE(settings.save());
    Settings restored;
    REQUIRE(restored.load());
    REQUIRE(restored.get_llm_choice() == LLMChoice::Local_7b);

    LLMSelectionDialog dialog(restored);
    CHECK_FALSE(builtin_llm_artifact_available(LLMChoice::Local_7b));
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_7b);
    const auto labels = LLMSelectionDialogTestAccess::local_builtin_labels(dialog);
    CHECK(std::find(labels.begin(), labels.end(),
                    default_llm_label_for_choice(LLMChoice::Local_7b).toStdString()) != labels.end());
    auto* downloader = LLMSelectionDialogTestAccess::local_downloader(dialog);
    REQUIRE(downloader != nullptr);
    CHECK(std::filesystem::path(downloader->get_download_destination())
          == temp.path() / "models" / "mistral-7b-instruct-v0.2.Q5_K_M.gguf");

    select_builtin_model(dialog, LLMChoice::Local_4b_Qwen35);
    select_builtin_model(dialog, LLMChoice::Local_7b);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_7b);
}

TEST_CASE("LLM selection dialog hides unsaved Mistral after its weights are removed") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard mistral_url_guard("LOCAL_LLM_7B_DOWNLOAD_URL", std::string(kMistralUrl));

    Settings settings;
    settings.load();
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);
    settings.set_llm_storage_dir((temp.path() / "models").string());
    const auto model_path = temp.path() / "models" / "mistral-7b-instruct-v0.2.Q5_K_M.gguf";
    write_gguf_file(model_path);
    LLMSelectionDialog dialog(settings);
    select_builtin_model(dialog, LLMChoice::Local_7b);
    REQUIRE(dialog.get_selected_llm_choice() == LLMChoice::Local_7b);

    REQUIRE(std::filesystem::remove(model_path));
    LLMSelectionDialogTestAccess::refresh_local_choices(dialog);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_4b_Gemma);
    const auto labels = LLMSelectionDialogTestAccess::local_builtin_labels(dialog);
    CHECK(std::find(labels.begin(), labels.end(),
                    default_llm_label_for_choice(LLMChoice::Local_7b).toStdString()) == labels.end());
}

TEST_CASE("LLM selection dialog restores Qwen and shares its visual text weights") {
    QtAppContext qt;
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
    EnvVarGuard localappdata_guard("LOCALAPPDATA", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard local_url_guard("LOCAL_LLM_QWEN35_4B_DOWNLOAD_URL",
                               std::string("https://example.com/Qwen3.5-4B-Q4_K_M.gguf"));
    EnvVarGuard visual_url_guard("QWEN35_4B_MODEL_URL",
                                std::string("https://example.com/Qwen3.5-4B-Q4_K_M.gguf"));

    Settings settings;
    settings.load();
    settings.set_llm_choice(LLMChoice::Local_4b_Qwen35);
    REQUIRE(settings.save());
    Settings restored;
    REQUIRE(restored.load());
    CHECK(restored.get_llm_choice() == LLMChoice::Local_4b_Qwen35);

    LLMSelectionDialog dialog(restored);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_4b_Qwen35);
    const auto* descriptor = find_visual_model_descriptor("qwen3.5-4b");
    REQUIRE(descriptor != nullptr);
    auto* downloader = LLMSelectionDialogTestAccess::local_downloader(dialog);
    REQUIRE(downloader != nullptr);
    CHECK(std::filesystem::path(downloader->get_download_destination())
          == visual_artifact_storage_path(*descriptor, descriptor->artifacts.front()));
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

TEST_CASE("LLM selection dialog emphasizes the visual model selector") {
    QtAppContext qt;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();

    LLMSelectionDialog dialog(settings);
    auto* combo = LLMSelectionDialogTestAccess::visual_model_combo(dialog);
    REQUIRE(combo != nullptr);

    CHECK(combo->objectName() == QStringLiteral("visualBackendCombo"));
    CHECK(combo->minimumHeight() >= 32);
    CHECK(combo->minimumContentsLength() >= 28);
    CHECK(combo->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);
    CHECK(combo->sizePolicy().verticalPolicy() == QSizePolicy::Fixed);
    CHECK(combo->styleSheet().contains(QStringLiteral("border: 2px solid #1f6feb")));
    CHECK(combo->styleSheet().contains(QStringLiteral("background-color: #eef6ff")));
    CHECK(combo->styleSheet().contains(QStringLiteral("font-weight: 600")));
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
