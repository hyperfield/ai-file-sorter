#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "Utils.hpp"
#include "VisualLlmRuntime.hpp"
#include "VisualModelCatalog.hpp"

#include <filesystem>
#include <fstream>

TEST_CASE("Default visual model descriptor exposes the MTMD backend catalog") {
    const auto& descriptors = visual_model_descriptors();
    REQUIRE(descriptors.size() >= 3);

    const auto& descriptor = default_visual_model_descriptor();

    CHECK(std::string(descriptor.id) == "llava-v1.6-mistral-7b");
    CHECK(std::string(descriptor.display_name) == "LLaVA 1.6 Mistral 7B");
    CHECK(descriptor.architecture == VisualModelArchitecture::MtmdProjector);
    CHECK(descriptor.prompt_policy == VisualPromptPolicy::LegacyLlava);
    REQUIRE(descriptor.artifacts.size() == 2);
    CHECK(descriptor.artifacts[0].kind == VisualModelArtifactKind::Model);
    CHECK(std::string(descriptor.artifacts[0].url_env) == "LLAVA_MODEL_URL");
    CHECK(descriptor.artifacts[1].kind == VisualModelArtifactKind::Mmproj);
    CHECK(std::string(descriptor.artifacts[1].url_env) == "LLAVA_MMPROJ_URL");

    const auto* vicuna = find_visual_model_descriptor("llava-v1.6-vicuna-7b");
    REQUIRE(vicuna != nullptr);
    CHECK(std::string(vicuna->display_name) == "LLaVA 1.6 Vicuna 7B");
    CHECK(vicuna->prompt_policy == VisualPromptPolicy::LegacyLlava);
    REQUIRE(vicuna->artifacts.size() == 2);
    CHECK(std::string(vicuna->artifacts[0].url_env) == "LLAVA_VICUNA_MODEL_URL");
    CHECK(std::string(vicuna->artifacts[1].url_env) == "LLAVA_VICUNA_MMPROJ_URL");

    const auto* gemma = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(gemma != nullptr);
    CHECK(std::string(gemma->display_name) == "Gemma 3 4B IT");
    CHECK(gemma->prompt_policy == VisualPromptPolicy::StructuredVisionInstruct);
    REQUIRE(gemma->artifacts.size() == 2);
    CHECK(std::string(gemma->artifacts[0].url_env) == "GEMMA3_4B_MODEL_URL");
    CHECK(std::string(gemma->artifacts[1].url_env) == "GEMMA3_4B_MMPROJ_URL");
}

TEST_CASE("VisualLlmRuntime resolves the active backend through descriptor artifacts") {
    TempDir home_dir;
    EnvVarGuard home_guard("HOME", home_dir.path().string());
    const std::string model_url = "https://example.com/llava-model.gguf";
    const std::string mmproj_url = "https://example.com/llava-mmproj.gguf";
    EnvVarGuard model_guard("LLAVA_MODEL_URL", model_url);
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", mmproj_url);

    const auto model_path = std::filesystem::path(
        Utils::make_default_path_to_file_from_download_url(model_url));
    const auto mmproj_fallback_path =
        std::filesystem::path(Utils::get_default_llm_destination()) / "mmproj-model-f16.gguf";
    std::filesystem::create_directories(model_path.parent_path());
    std::ofstream(model_path).put('x');
    std::ofstream(mmproj_fallback_path).put('x');

    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend({}, &error);
    REQUIRE(backend.has_value());
    CHECK(error.empty());
    REQUIRE(backend->descriptor != nullptr);
    CHECK(std::string(backend->descriptor->id) == "llava-v1.6-mistral-7b");
    REQUIRE(backend->artifacts.size() == 2);
    REQUIRE(backend->path_for(VisualModelArtifactKind::Model).has_value());
    REQUIRE(backend->path_for(VisualModelArtifactKind::Mmproj).has_value());
    CHECK(*backend->path_for(VisualModelArtifactKind::Model) == model_path);
    CHECK(*backend->path_for(VisualModelArtifactKind::Mmproj) == mmproj_fallback_path);

    const auto legacy_paths = VisualLlmRuntime::resolve_paths({}, &error);
    REQUIRE(legacy_paths.has_value());
    CHECK(legacy_paths->model_path == model_path);
    CHECK(legacy_paths->mmproj_path == mmproj_fallback_path);
}

TEST_CASE("VisualLlmRuntime reports missing backend URLs before resolving artifacts") {
    TempDir home_dir;
    EnvVarGuard home_guard("HOME", home_dir.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::nullopt);
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::nullopt);

    std::string error;
    CHECK_FALSE(VisualLlmRuntime::resolve_active_backend({}, &error).has_value());
    CHECK(error == "Missing visual LLM download URLs. Check LLAVA_MODEL_URL and LLAVA_MMPROJ_URL.");
}

TEST_CASE("VisualLlmRuntime resolves a non-default backend by id") {
    TempDir home_dir;
    EnvVarGuard home_guard("HOME", home_dir.path().string());
    const std::string model_url = "https://example.com/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-gemma-3-4b-it-Q4_K_M.gguf";
    EnvVarGuard model_guard("GEMMA3_4B_MODEL_URL", model_url);
    EnvVarGuard mmproj_guard("GEMMA3_4B_MMPROJ_URL", mmproj_url);

    const auto model_path = std::filesystem::path(
        Utils::make_default_path_to_file_from_download_url(model_url));
    const auto mmproj_path = std::filesystem::path(
        Utils::make_default_path_to_file_from_download_url(mmproj_url));
    std::filesystem::create_directories(model_path.parent_path());
    std::ofstream(model_path).put('x');
    std::ofstream(mmproj_path).put('x');

    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend("gemma-3-4b-it", &error);
    REQUIRE(backend.has_value());
    CHECK(error.empty());
    REQUIRE(backend->descriptor != nullptr);
    CHECK(std::string(backend->descriptor->id) == "gemma-3-4b-it");
    REQUIRE(backend->path_for(VisualModelArtifactKind::Model).has_value());
    REQUIRE(backend->path_for(VisualModelArtifactKind::Mmproj).has_value());
    CHECK(*backend->path_for(VisualModelArtifactKind::Model) == model_path);
    CHECK(*backend->path_for(VisualModelArtifactKind::Mmproj) == mmproj_path);
}
