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
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QToolButton>

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
    auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("localModelCombo"));
    REQUIRE(combo != nullptr);
    const int index = combo->findData(static_cast<int>(choice));
    REQUIRE(index >= 0);
    combo->setCurrentIndex(index);
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
    CHECK(labels[0] == default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_4b_Qwen35)).toStdString()
                          + " (Recommended)");
    CHECK(labels[1] == default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_4b_Gemma)).toStdString());
    CHECK(labels[2] == default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_7b_Gemma)).toStdString());
    CHECK(labels[0].find("Recommended") != std::string::npos);
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
    CHECK(labels[0] == default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_4b_Qwen35)).toStdString()
                          + " (Recommended)");
    CHECK(labels[1] == default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_4b_Gemma)).toStdString());
    CHECK(labels[2] == default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_7b)).toStdString());
    CHECK(labels[3] == default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_7b_Gemma)).toStdString());
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
                    default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_7b)).toStdString()) != labels.end());
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
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_4b_Qwen35);
    const auto labels = LLMSelectionDialogTestAccess::local_builtin_labels(dialog);
    CHECK(std::find(labels.begin(), labels.end(),
                    default_llm_name(*default_llm_entry_for_choice(LLMChoice::Local_7b)).toStdString()) == labels.end());
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

TEST_CASE("LLM selection dialog defaults to the Qwen 4B local model") {
    QtAppContext qt;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();

    LLMSelectionDialog dialog(settings);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_4b_Qwen35);
}

TEST_CASE("LLM selection dialog keeps technical download details collapsed and compact") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    const std::string base_url = "https://example.com/" + std::string(1200, 'a') + "/";
    EnvVarGuard local_url_guard("LOCAL_LLM_QWEN35_4B_DOWNLOAD_URL", base_url + "Qwen3.5-4B.gguf");
    EnvVarGuard visual_url_guard("QWEN35_4B_MODEL_URL", base_url + "Qwen3.5-4B.gguf");
    EnvVarGuard mmproj_url_guard("QWEN35_4B_MMPROJ_URL", base_url + "mmproj.gguf");
    EnvVarGuard gemma_url_guard("LOCAL_LLM_3B_DOWNLOAD_URL", std::nullopt);
    Settings settings;
    settings.set_llm_storage_dir((config_dir.path() / "models").string());
    settings.set_llm_downloads_expanded(true);
    LLMSelectionDialog dialog(settings);
    dialog.show();
    QApplication::processEvents();
    const int compact_width = dialog.width();

    for (const auto& prefix : {QStringLiteral("localDownloadDetails"),
                               QStringLiteral("visualDownloadDetails_QWEN35_4B_MODEL_URL"),
                               QStringLiteral("visualDownloadDetails_QWEN35_4B_MMPROJ_URL")}) {
        auto* toggle = dialog.findChild<QToolButton*>(prefix + QStringLiteral("Toggle"));
        auto* remote = dialog.findChild<QLabel*>(prefix + QStringLiteral("RemoteUrl"));
        auto* path = dialog.findChild<QLabel*>(prefix + QStringLiteral("LocalPath"));
        auto* metadata = dialog.findChild<QLabel*>(prefix + QStringLiteral("Metadata"));
        REQUIRE(toggle != nullptr);
        REQUIRE(remote != nullptr);
        REQUIRE(path != nullptr);
        REQUIRE(metadata != nullptr);
        CHECK(toggle->isVisibleTo(&dialog));
        CHECK(toggle->text() == QStringLiteral("See details"));
        CHECK_FALSE(remote->isVisibleTo(&dialog));
        CHECK_FALSE(path->isVisibleTo(&dialog));
        CHECK_FALSE(metadata->isVisibleTo(&dialog));

        toggle->click();
        QApplication::processEvents();
        CHECK(toggle->text() == QStringLiteral("Hide details"));
        CHECK(remote->isVisibleTo(&dialog));
        CHECK(path->isVisibleTo(&dialog));
        CHECK(metadata->isVisibleTo(&dialog));
        CHECK(remote->text().contains(QString::fromStdString(base_url)));
        CHECK(path->text().contains(QString::fromStdString(config_dir.path().string())));
        CHECK(metadata->text().contains(QStringLiteral("GGUF")));
        CHECK(metadata->text().contains(QStringLiteral("llama.cpp")));
        CHECK(metadata->text().contains(QStringLiteral("Download URL setting")));
        CHECK(metadata->text().contains(QStringLiteral("qwen3.5-4b")));
        CHECK(dialog.width() <= compact_width + 32);

        toggle->click();
        QApplication::processEvents();
        CHECK_FALSE(remote->isVisibleTo(&dialog));
        CHECK(toggle->text() == QStringLiteral("See details"));
    }

    select_builtin_model(dialog, LLMChoice::Local_4b_Gemma);
    auto* remote = dialog.findChild<QLabel*>(QStringLiteral("localDownloadDetailsRemoteUrl"));
    auto* metadata = dialog.findChild<QLabel*>(QStringLiteral("localDownloadDetailsMetadata"));
    REQUIRE(remote != nullptr);
    REQUIRE(metadata != nullptr);
    CHECK(remote->text() == QStringLiteral("Remote URL: unknown"));
    CHECK(metadata->text().contains(QStringLiteral("gemma-3-4b-it")));
    CHECK_FALSE(metadata->text().contains(QStringLiteral("qwen3.5-4b")));
}

TEST_CASE("LLM selection dialog switches between grouped model dropdowns") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    settings.set_llm_storage_dir((config_dir.path() / "models").string());
    LLMSelectionDialog dialog(settings);

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("llmSourceTabs"));
    auto* local = dialog.findChild<QComboBox*>(QStringLiteral("localModelCombo"));
    auto* api = dialog.findChild<QComboBox*>(QStringLiteral("apiProviderCombo"));
    auto* description = dialog.findChild<QLabel*>(QStringLiteral("localModelDescription"));
    auto* gemini = dialog.findChild<QWidget*>(QStringLiteral("geminiInputs"));
    auto* openai = dialog.findChild<QWidget*>(QStringLiteral("openaiInputs"));
    auto* custom_api = dialog.findChild<QWidget*>(QStringLiteral("customApiControls"));
    auto* key = dialog.findChild<QLineEdit*>(QStringLiteral("openaiApiKeyEdit"));
    auto* model = dialog.findChild<QLineEdit*>(QStringLiteral("openaiModelEdit"));
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    REQUIRE(tabs != nullptr);
    REQUIRE(local != nullptr);
    REQUIRE(api != nullptr);
    REQUIRE(description != nullptr);
    REQUIRE(gemini != nullptr);
    REQUIRE(openai != nullptr);
    REQUIRE(custom_api != nullptr);
    REQUIRE(key != nullptr);
    REQUIRE(model != nullptr);
    REQUIRE(buttons != nullptr);

    REQUIRE(tabs->count() == 2);
    CHECK(tabs->tabText(0) == QStringLiteral("Local LLMs"));
    CHECK(tabs->tabText(1) == QStringLiteral("API connections"));
    CHECK(tabs->currentIndex() == 0);
    CHECK(local->currentIndex() == 0);
    CHECK(local->currentText().contains(QStringLiteral("Recommended")));
    CHECK(description->text().contains(QStringLiteral("High precision")));
    CHECK(description->text().contains(QStringLiteral("existing folder structures")));
    CHECK(api->count() == 3);

    select_builtin_model(dialog, LLMChoice::Local_4b_Gemma);
    CHECK(description->text().contains(QStringLiteral("Balanced local model")));
    tabs->setCurrentIndex(1);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Remote_Gemini);
    CHECK_FALSE(gemini->isHidden());
    CHECK(openai->isHidden());
    CHECK(custom_api->isHidden());
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());

    api->setCurrentIndex(api->findData(static_cast<int>(LLMChoice::Remote_OpenAI)));
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Remote_OpenAI);
    CHECK(gemini->isHidden());
    CHECK_FALSE(openai->isHidden());
    key->setText(QStringLiteral(" example-key "));
    model->setText(QStringLiteral(" example-model "));
    CHECK(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(dialog.get_openai_api_key() == "example-key");
    CHECK(dialog.get_openai_model() == "example-model");
    model->clear();
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    model->setText(QStringLiteral("example-model"));

    tabs->setCurrentIndex(0);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Local_4b_Gemma);
    tabs->setCurrentIndex(1);
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Remote_OpenAI);
    CHECK(dialog.get_openai_api_key() == "example-key");
    CHECK(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(settings.get_llm_choice() == LLMChoice::Unset);
}

TEST_CASE("LLM selection dialog restores saved API and custom dropdown selections") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    for (const auto choice : {LLMChoice::Remote_Gemini, LLMChoice::Remote_OpenAI,
                              LLMChoice::Remote_Custom, LLMChoice::Custom}) {
        DYNAMIC_SECTION("Saved choice " << static_cast<int>(choice)) {
            Settings settings;
            CustomLLM local;
            local.name = "Another local model";
            local.path = (config_dir.path() / "custom.gguf").string();
            settings.upsert_custom_llm(local);
            local.name = "My local model";
            const auto local_id = settings.upsert_custom_llm(local);
            CustomApiEndpoint endpoint;
            endpoint.name = "Another endpoint";
            endpoint.base_url = "http://localhost:1234/v1";
            endpoint.model = "example-model";
            settings.upsert_custom_api_endpoint(endpoint);
            endpoint.name = "My local endpoint";
            const auto api_id = settings.upsert_custom_api_endpoint(endpoint);
            settings.set_active_custom_llm_id(local_id);
            settings.set_active_custom_api_id(api_id);
            settings.set_llm_choice(choice);
            REQUIRE(settings.save());

            Settings restored;
            REQUIRE(restored.load());
            LLMSelectionDialog dialog(restored);
            auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("llmSourceTabs"));
            auto* local_combo = dialog.findChild<QComboBox*>(QStringLiteral("localModelCombo"));
            auto* api_combo = dialog.findChild<QComboBox*>(QStringLiteral("apiProviderCombo"));
            auto* custom_local = dialog.findChild<QComboBox*>(QStringLiteral("customLocalCombo"));
            auto* custom_api = dialog.findChild<QComboBox*>(QStringLiteral("customApiCombo"));
            REQUIRE(tabs != nullptr);
            REQUIRE(local_combo != nullptr);
            REQUIRE(api_combo != nullptr);
            REQUIRE(custom_local != nullptr);
            REQUIRE(custom_api != nullptr);
            CHECK(dialog.get_selected_llm_choice() == choice);
            CHECK(tabs->currentIndex() == (is_remote_choice(choice) ? 1 : 0));
            CHECK((is_remote_choice(choice) ? api_combo : local_combo)->currentData().toInt()
                  == static_cast<int>(choice));
            CHECK(dialog.get_selected_custom_llm_id() == local_id);
            CHECK(dialog.get_selected_custom_api_id() == api_id);
            CHECK(custom_local->currentData().toString().toStdString() == local_id);
            CHECK(custom_api->currentData().toString().toStdString() == api_id);
            CHECK(custom_local->parentWidget()->isHidden() == (choice != LLMChoice::Custom));
            CHECK(custom_api->parentWidget()->isHidden() == (choice != LLMChoice::Remote_Custom));
        }
    }
}

TEST_CASE("LLM selection dialog requires a custom entry when its dropdown is empty") {
    QtAppContext qt;
    LlmStorageOverrideGuard storage_guard;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    LLMSelectionDialog dialog(settings);
    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("llmSourceTabs"));
    auto* local = dialog.findChild<QComboBox*>(QStringLiteral("localModelCombo"));
    auto* api = dialog.findChild<QComboBox*>(QStringLiteral("apiProviderCombo"));
    auto* custom_local = dialog.findChild<QComboBox*>(QStringLiteral("customLocalCombo"));
    auto* custom_api = dialog.findChild<QComboBox*>(QStringLiteral("customApiCombo"));
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    REQUIRE(tabs != nullptr);
    REQUIRE(local != nullptr);
    REQUIRE(api != nullptr);
    REQUIRE(custom_local != nullptr);
    REQUIRE(custom_api != nullptr);
    REQUIRE(buttons != nullptr);

    local->setCurrentIndex(local->findData(static_cast<int>(LLMChoice::Custom)));
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Custom);
    CHECK_FALSE(custom_local->parentWidget()->isHidden());
    CHECK(dialog.get_selected_custom_llm_id().empty());
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    tabs->setCurrentIndex(1);
    api->setCurrentIndex(api->findData(static_cast<int>(LLMChoice::Remote_Custom)));
    CHECK(dialog.get_selected_llm_choice() == LLMChoice::Remote_Custom);
    CHECK_FALSE(custom_api->parentWidget()->isHidden());
    CHECK(dialog.get_selected_custom_api_id().empty());
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
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
