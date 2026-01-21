#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <filesystem>
#include <fstream>

namespace {

void create_visual_llm_files()
{
    const std::string model_url = "https://example.com/llava-model.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-model-f16.gguf";

    const std::filesystem::path model_path =
        Utils::make_default_path_to_file_from_download_url(model_url);
    const std::filesystem::path mmproj_path =
        Utils::make_default_path_to_file_from_download_url(mmproj_url);

    std::filesystem::create_directories(model_path.parent_path());
    std::ofstream(model_path.string(), std::ios::binary).put('x');
    std::ofstream(mmproj_path.string(), std::ios::binary).put('x');
}

} // namespace

TEST_CASE("Settings defaults image analysis off even when visual LLM files exist") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::string("https://example.com/llava-model.gguf"));
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::string("https://example.com/mmproj-model-f16.gguf"));

    create_visual_llm_files();

    Settings settings;
    const bool loaded = settings.load();
    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(settings.get_analyze_images_by_content());
    REQUIRE_FALSE(settings.get_offer_rename_images());
}

TEST_CASE("Settings defaults image analysis off when visual LLM files are missing") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::string("https://example.com/llava-model.gguf"));
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::string("https://example.com/mmproj-model-f16.gguf"));

    Settings settings;
    const bool loaded = settings.load();
    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(settings.get_analyze_images_by_content());
    REQUIRE_FALSE(settings.get_offer_rename_images());
}

TEST_CASE("Settings enforces rename-only implies offer rename") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::string("https://example.com/llava-model.gguf"));
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::string("https://example.com/mmproj-model-f16.gguf"));

    Settings settings;
    settings.set_analyze_images_by_content(true);
    settings.set_offer_rename_images(false);
    settings.set_rename_images_only(true);
    settings.set_process_images_only(true);
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.get_offer_rename_images());
    REQUIRE(reloaded.get_rename_images_only());
    REQUIRE(reloaded.get_process_images_only());
}
