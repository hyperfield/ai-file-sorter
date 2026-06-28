#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("Custom LLM entries persist across Settings load/save") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();

    CustomLLM llm;
    llm.name = "My Local Model";
    llm.path = "/models/custom.gguf";

    const std::string id = settings.upsert_custom_llm(llm);
    REQUIRE_FALSE(id.empty());
    settings.set_active_custom_llm_id(id);
    REQUIRE(settings.save());

    Settings reloaded;
    reloaded.load();
    const CustomLLM loaded = reloaded.find_custom_llm(id);

    REQUIRE(loaded.id == id);
    REQUIRE(loaded.name == llm.name);
    REQUIRE(loaded.path == llm.path);
    REQUIRE(reloaded.get_active_custom_llm_id() == id);
}

TEST_CASE("Settings maps legacy Local_3b choices to Gemma 4B") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    const std::filesystem::path settings_dir = config_dir.path() / "AIFileSorter";
    std::filesystem::create_directories(settings_dir);
    std::ofstream out(settings_dir / "config.ini");
    REQUIRE(out.is_open());
    out << "[Settings]\n";
    out << "LLMChoice=Local_3b\n";
    out.close();

    Settings settings;
    REQUIRE(settings.load());
    CHECK(settings.get_llm_choice() == LLMChoice::Local_4b_Gemma);
}

TEST_CASE("Built-in Gemma 7B choice persists across Settings load/save") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    settings.set_llm_choice(LLMChoice::Local_7b_Gemma);
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_llm_choice() == LLMChoice::Local_7b_Gemma);
}

TEST_CASE("Legacy local LLaMa resolves the previous Q4 artifact without marking Gemma 4B ready") {
    TempDir config_dir;
    EnvVarGuard home_guard("HOME", config_dir.path().string());
    EnvVarGuard appdata_guard("APPDATA", config_dir.path().string());
    EnvVarGuard localappdata_guard("LOCALAPPDATA", config_dir.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    const std::filesystem::path legacy_path =
        Utils::make_default_path_to_file_from_download_url(kLegacyLlama3BQ4Url);
    write_gguf_file(legacy_path);

    const auto resolved_path = resolve_downloaded_builtin_llm_path(LLMChoice::Local_3b_legacy);
    REQUIRE(resolved_path.has_value());
    CHECK(*resolved_path == legacy_path);
    CHECK(builtin_llm_artifact_available(LLMChoice::Local_3b_legacy));
    CHECK_FALSE(builtin_llm_artifact_available(LLMChoice::Local_4b_Gemma));
}

TEST_CASE("Built-in Gemma 4B resolves the shared visual-model artifact path") {
    TempDir config_dir;
    EnvVarGuard home_guard("HOME", config_dir.path().string());
    EnvVarGuard appdata_guard("APPDATA", config_dir.path().string());
    EnvVarGuard localappdata_guard("LOCALAPPDATA", config_dir.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
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

    const auto visual_model_path =
        visual_artifact_storage_path(*descriptor, descriptor->artifacts.front());
    write_gguf_file(visual_model_path);

    const auto resolved_path = resolve_downloaded_builtin_llm_path(LLMChoice::Local_4b_Gemma);
    REQUIRE(resolved_path.has_value());
    CHECK(*resolved_path == visual_model_path);
    CHECK(builtin_llm_artifact_available(LLMChoice::Local_4b_Gemma));
}

TEST_CASE("Built-in Gemma 4B prefers the shared visual-model artifact path for new downloads") {
    TempDir config_dir;
    EnvVarGuard home_guard("HOME", config_dir.path().string());
    EnvVarGuard appdata_guard("APPDATA", config_dir.path().string());
    EnvVarGuard localappdata_guard("LOCALAPPDATA", config_dir.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
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

    const auto visual_model_path =
        visual_artifact_storage_path(*descriptor, descriptor->artifacts.front());

    CHECK(preferred_builtin_llm_path(LLMChoice::Local_4b_Gemma) == visual_model_path);
    CHECK_FALSE(resolve_downloaded_builtin_llm_path(LLMChoice::Local_4b_Gemma).has_value());
}
