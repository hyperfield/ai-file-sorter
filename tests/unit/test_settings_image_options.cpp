#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <QSettings>

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
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
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
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::string("https://example.com/llava-model.gguf"));
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::string("https://example.com/mmproj-model-f16.gguf"));

    Settings settings;
    const bool loaded = settings.load();
    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(settings.get_analyze_images_by_content());
    REQUIRE_FALSE(settings.get_offer_rename_images());
}

TEST_CASE("Settings defaults use subcategories on when config key is missing") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    const std::filesystem::path config_path =
        std::filesystem::path(settings.get_config_dir()) / "config.ini";

    QSettings config(QString::fromStdString(config_path.string()), QSettings::IniFormat);
    config.beginGroup("Settings");
    config.setValue("CategorizeFiles", true);
    config.endGroup();
    config.sync();

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.get_use_subcategories());
}

TEST_CASE("Settings enforces rename-only implies offer rename") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
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

TEST_CASE("Settings persists options group expansion state") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.set_image_options_expanded(true);
    settings.set_document_options_expanded(false);
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.get_image_options_expanded());
    REQUIRE_FALSE(reloaded.get_document_options_expanded());
}

TEST_CASE("Settings persists image EXIF date/place rename toggle") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.set_offer_rename_images(true);
    settings.set_add_image_date_place_to_filename(true);
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.get_offer_rename_images());
    REQUIRE(reloaded.get_add_image_date_place_to_filename());
}

TEST_CASE("Settings defaults audio/video metadata rename toggle to enabled") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    const bool loaded = settings.load();
    REQUIRE_FALSE(loaded);
    REQUIRE(settings.get_add_audio_video_metadata_to_filename());
}

TEST_CASE("Settings persists audio/video metadata rename toggle") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.set_add_audio_video_metadata_to_filename(false);
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE_FALSE(reloaded.get_add_audio_video_metadata_to_filename());
}

TEST_CASE("Settings persists image date-to-category toggle") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.set_analyze_images_by_content(true);
    settings.set_add_image_date_to_category(true);
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.get_analyze_images_by_content());
    REQUIRE(reloaded.get_add_image_date_to_category());
}

TEST_CASE("Settings persists selected visual model backend") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.set_visual_model_id("gemma-3-4b-it");
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.get_visual_model_id() == "gemma-3-4b-it");
}

TEST_CASE("Settings persists benchmark probe signature metadata") {
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.set_suitability_benchmark_completed(true);
    settings.set_benchmark_probe_signature("driver=610.47|runtime=cudart64_13.dll");
    settings.set_benchmark_probe_schema_version(Settings::kBenchmarkProbeSchemaVersion);
    REQUIRE(settings.save());

    Settings reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.get_benchmark_probe_signature() ==
            "driver=610.47|runtime=cudart64_13.dll");
    REQUIRE(reloaded.get_benchmark_probe_schema_version() ==
            Settings::kBenchmarkProbeSchemaVersion);
    REQUIRE(reloaded.is_suitability_benchmark_current(
        "driver=610.47|runtime=cudart64_13.dll"));
}

TEST_CASE("Settings invalidates completed benchmark when probe signature changes") {
    Settings settings;
    settings.set_suitability_benchmark_completed(true);
    settings.set_benchmark_probe_signature("driver=old|runtime=missing");
    settings.set_benchmark_probe_schema_version(Settings::kBenchmarkProbeSchemaVersion);

    REQUIRE_FALSE(settings.is_suitability_benchmark_current(
        "driver=610.47|runtime=cudart64_13.dll"));

    Settings legacy_settings;
    legacy_settings.set_suitability_benchmark_completed(true);

    REQUIRE_FALSE(legacy_settings.is_suitability_benchmark_current(
        "driver=610.47|runtime=cudart64_13.dll"));
}

TEST_CASE("Settings invalidates benchmark when probe schema or completion state is stale") {
    const std::string current_signature = "driver=610.47|runtime=cudart64_13.dll";

    Settings old_schema_settings;
    old_schema_settings.set_suitability_benchmark_completed(true);
    old_schema_settings.set_benchmark_probe_signature(current_signature);
    old_schema_settings.set_benchmark_probe_schema_version(
        Settings::kBenchmarkProbeSchemaVersion - 1);

    REQUIRE_FALSE(old_schema_settings.is_suitability_benchmark_current(
        current_signature));

    Settings incomplete_settings;
    incomplete_settings.set_benchmark_probe_signature(current_signature);
    incomplete_settings.set_benchmark_probe_schema_version(
        Settings::kBenchmarkProbeSchemaVersion);

    REQUIRE_FALSE(incomplete_settings.is_suitability_benchmark_current(
        current_signature));
}

TEST_CASE("Benchmark probe signature includes backend environment controls") {
    EnvVarGuard backend_guard("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
    EnvVarGuard device_guard("LLAMA_ARG_DEVICE", "cpu");
    EnvVarGuard disable_cuda_guard("GGML_DISABLE_CUDA", "1");

    const std::string signature = Utils::benchmark_probe_signature();

    REQUIRE(signature.find("|backend_env=vulkan") != std::string::npos);
    REQUIRE(signature.find("|llama_device_env=cpu") != std::string::npos);
    REQUIRE(signature.find("|ggml_disable_cuda_env=1") != std::string::npos);
}
