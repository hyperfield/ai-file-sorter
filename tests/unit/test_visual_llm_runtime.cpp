#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "Utils.hpp"
#include "VisualLlmRuntime.hpp"
#include "VisualModelCatalog.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

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
    meta << "content_length=1\n";
}

struct VisualRuntimeTempEnv {
    TempDir home_dir;
    TempDir appdata_dir;
    EnvVarGuard home_guard;
    EnvVarGuard appdata_guard;
    EnvVarGuard localappdata_guard;

    VisualRuntimeTempEnv()
        : home_guard("HOME", home_dir.path().string()),
          appdata_guard("APPDATA", appdata_dir.path().string()),
          localappdata_guard("LOCALAPPDATA", appdata_dir.path().string())
    {
    }
};

} // namespace

TEST_CASE("Default visual model descriptor exposes the MTMD backend catalog") {
    const auto& descriptors = visual_model_descriptors();
    REQUIRE(descriptors.size() >= 2);

    const auto& descriptor = default_visual_model_descriptor();

    CHECK(std::string(descriptor.id) == "gemma-3-4b-it");
    CHECK(std::string(descriptor.display_name) == "Gemma 3 4B IT");
    CHECK(descriptor.architecture == VisualModelArchitecture::MtmdProjector);
    CHECK(descriptor.prompt_policy == VisualPromptPolicy::StructuredVisionInstruct);
    REQUIRE(descriptor.artifacts.size() == 2);
    CHECK(descriptor.artifacts[0].kind == VisualModelArtifactKind::Model);
    CHECK(std::string(descriptor.artifacts[0].url_env) == "GEMMA3_4B_MODEL_URL");
    CHECK(std::string(descriptor.artifacts[0].local_storage_name) == "model.gguf");
    CHECK(descriptor.artifacts[1].kind == VisualModelArtifactKind::Mmproj);
    CHECK(std::string(descriptor.artifacts[1].url_env) == "GEMMA3_4B_MMPROJ_URL");
    CHECK(std::string(descriptor.artifacts[1].local_storage_name) == "mmproj.gguf");

    const auto* llava = find_visual_model_descriptor("llava-v1.6-mistral-7b");
    REQUIRE(llava != nullptr);
    CHECK(std::string(llava->display_name) == "LLaVA 1.6 Mistral 7B");
    CHECK(llava->prompt_policy == VisualPromptPolicy::LegacyLlava);
    REQUIRE(llava->artifacts.size() == 2);
    CHECK(std::string(llava->artifacts[0].url_env) == "LLAVA_MODEL_URL");
    CHECK(std::string(llava->artifacts[1].url_env) == "LLAVA_MMPROJ_URL");
    CHECK(find_visual_model_descriptor("llava-v1.6-vicuna-7b") == nullptr);

    const auto* gemma = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(gemma != nullptr);
    CHECK(std::string(gemma->display_name) == "Gemma 3 4B IT");
    CHECK(gemma->prompt_policy == VisualPromptPolicy::StructuredVisionInstruct);
    REQUIRE(gemma->artifacts.size() == 2);
    CHECK(std::string(gemma->artifacts[0].url_env) == "GEMMA3_4B_MODEL_URL");
    CHECK(std::string(gemma->artifacts[1].url_env) == "GEMMA3_4B_MMPROJ_URL");
}

TEST_CASE("VisualLlmRuntime resolves the active backend through descriptor artifacts") {
    VisualRuntimeTempEnv env;
    const std::string model_url = "https://example.com/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-gemma-3-4b-it-Q4_K_M.gguf";
    EnvVarGuard model_guard("GEMMA3_4B_MODEL_URL", model_url);
    EnvVarGuard mmproj_guard("GEMMA3_4B_MMPROJ_URL", mmproj_url);

    const auto& descriptor = default_visual_model_descriptor();
    const auto model_path = visual_artifact_storage_path(descriptor, descriptor.artifacts[0]);
    const auto mmproj_path = visual_artifact_storage_path(descriptor, descriptor.artifacts[1]);
    write_gguf_file(model_path);
    write_gguf_file(mmproj_path);

    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend({}, &error);
    REQUIRE(backend.has_value());
    CHECK(error.empty());
    REQUIRE(backend->descriptor != nullptr);
    CHECK(std::string(backend->descriptor->id) == "gemma-3-4b-it");
    REQUIRE(backend->artifacts.size() == 2);
    REQUIRE(backend->path_for(VisualModelArtifactKind::Model).has_value());
    REQUIRE(backend->path_for(VisualModelArtifactKind::Mmproj).has_value());
    CHECK(*backend->path_for(VisualModelArtifactKind::Model) == model_path);
    CHECK(*backend->path_for(VisualModelArtifactKind::Mmproj) == mmproj_path);

    const auto legacy_paths = VisualLlmRuntime::resolve_paths({}, &error);
    REQUIRE(legacy_paths.has_value());
    CHECK(legacy_paths->model_path == model_path);
    CHECK(legacy_paths->mmproj_path == mmproj_path);
}

TEST_CASE("VisualLlmRuntime resolves custom visual LLM artifacts") {
    TempDir temp;
    const auto model_path = temp.path() / "custom-model.gguf";
    const auto mmproj_path = temp.path() / "custom-mmproj.gguf";
    write_gguf_file(model_path);
    write_gguf_file(mmproj_path);

    CustomLLM custom;
    custom.id = "custom-vision";
    custom.name = "Custom Vision";
    custom.path = model_path.string();
    custom.mmproj_path = mmproj_path.string();

    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend(
        custom_visual_model_id_for_llm(custom.id),
        std::vector<CustomLLM>{custom},
        &error);

    REQUIRE(backend.has_value());
    CHECK(error.empty());
    REQUIRE(backend->descriptor != nullptr);
    CHECK(std::string(backend->descriptor->id) == "custom");
    CHECK(backend->descriptor->prompt_policy == VisualPromptPolicy::StructuredVisionInstruct);
    REQUIRE(backend->path_for(VisualModelArtifactKind::Model).has_value());
    REQUIRE(backend->path_for(VisualModelArtifactKind::Mmproj).has_value());
    CHECK(*backend->path_for(VisualModelArtifactKind::Model) == model_path);
    CHECK(*backend->path_for(VisualModelArtifactKind::Mmproj) == mmproj_path);
}

TEST_CASE("VisualLlmRuntime rejects custom visual LLMs without MMProj") {
    TempDir temp;
    const auto model_path = temp.path() / "custom-model.gguf";
    write_gguf_file(model_path);

    CustomLLM custom;
    custom.id = "custom-without-mmproj";
    custom.name = "Custom without MMProj";
    custom.path = model_path.string();

    std::string error;
    CHECK_FALSE(VisualLlmRuntime::resolve_active_backend(
                    custom_visual_model_id_for_llm(custom.id),
                    std::vector<CustomLLM>{custom},
                    &error)
                    .has_value());
    CHECK(error == "Custom visual LLM requires both a model file and an MMProj file. "
                   "Edit the custom LLM entry and add an MMProj file.");
}

TEST_CASE("VisualLlmRuntime reports missing backend URLs before resolving artifacts") {
    VisualRuntimeTempEnv env;
    EnvVarGuard model_guard("GEMMA3_4B_MODEL_URL", std::nullopt);
    EnvVarGuard mmproj_guard("GEMMA3_4B_MMPROJ_URL", std::nullopt);

    std::string error;
    CHECK_FALSE(VisualLlmRuntime::resolve_active_backend({}, &error).has_value());
    CHECK(error == "Missing visual LLM download URLs. Check GEMMA3_4B_MODEL_URL and GEMMA3_4B_MMPROJ_URL.");
}

TEST_CASE("VisualLlmRuntime resolves a non-default backend by id") {
    VisualRuntimeTempEnv env;
    const std::string model_url = "https://example.com/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-gemma-3-4b-it-Q4_K_M.gguf";
    EnvVarGuard model_guard("GEMMA3_4B_MODEL_URL", model_url);
    EnvVarGuard mmproj_guard("GEMMA3_4B_MMPROJ_URL", mmproj_url);

    const auto* descriptor = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(descriptor != nullptr);
    const auto model_path = visual_artifact_storage_path(*descriptor, descriptor->artifacts[0]);
    const auto mmproj_path = visual_artifact_storage_path(*descriptor, descriptor->artifacts[1]);
    write_gguf_file(model_path);
    write_gguf_file(mmproj_path);

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

TEST_CASE("VisualLlmRuntime accepts legacy generic mmproj files when metadata matches the backend") {
    VisualRuntimeTempEnv env;
    const std::string model_url = "https://example.com/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-model-f16.gguf";
    EnvVarGuard model_guard("GEMMA3_4B_MODEL_URL", model_url);
    EnvVarGuard mmproj_guard("GEMMA3_4B_MMPROJ_URL", mmproj_url);

    const auto* descriptor = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(descriptor != nullptr);

    const auto model_path = visual_artifact_storage_path(*descriptor, descriptor->artifacts[0]);
    const auto legacy_mmproj_path =
        std::filesystem::path(Utils::make_default_path_to_file_from_download_url(mmproj_url));

    write_gguf_file(model_path);
    write_gguf_file(legacy_mmproj_path);
    write_download_metadata(legacy_mmproj_path, mmproj_url);

    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend("gemma-3-4b-it", &error);
    REQUIRE(backend.has_value());
    CHECK(error.empty());
    REQUIRE(backend->path_for(VisualModelArtifactKind::Mmproj).has_value());
    CHECK(*backend->path_for(VisualModelArtifactKind::Mmproj) == legacy_mmproj_path);
}

TEST_CASE("VisualLlmRuntime accepts the legacy LLaVA generic mmproj without metadata") {
    VisualRuntimeTempEnv env;
    const std::string model_url = "https://example.com/llava-model.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-model-f16.gguf";
    EnvVarGuard model_guard("LLAVA_MODEL_URL", model_url);
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", mmproj_url);

    const auto* descriptor = find_visual_model_descriptor("llava-v1.6-mistral-7b");
    REQUIRE(descriptor != nullptr);
    const auto model_path = visual_artifact_storage_path(*descriptor, descriptor->artifacts[0]);
    const auto legacy_mmproj_path =
        std::filesystem::path(Utils::make_default_path_to_file_from_download_url(mmproj_url));

    write_gguf_file(model_path);
    write_gguf_file(legacy_mmproj_path);

    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend("llava-v1.6-mistral-7b", &error);
    REQUIRE(backend.has_value());
    CHECK(error.empty());
    REQUIRE(backend->path_for(VisualModelArtifactKind::Mmproj).has_value());
    CHECK(*backend->path_for(VisualModelArtifactKind::Mmproj) == legacy_mmproj_path);
}

TEST_CASE("VisualLlmRuntime does not misattribute a legacy generic mmproj from another backend") {
    VisualRuntimeTempEnv env;
    const std::string gemma_model_url = "https://example.com/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string gemma_mmproj_url = "https://example.com/mmproj-model-f16.gguf";
    const std::string llava_mmproj_url = "https://example.com/llava-mmproj-model-f16.gguf";
    EnvVarGuard model_guard("GEMMA3_4B_MODEL_URL", gemma_model_url);
    EnvVarGuard mmproj_guard("GEMMA3_4B_MMPROJ_URL", gemma_mmproj_url);

    const auto* descriptor = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(descriptor != nullptr);

    const auto model_path = visual_artifact_storage_path(*descriptor, descriptor->artifacts[0]);
    const auto legacy_mmproj_path =
        std::filesystem::path(Utils::make_default_path_to_file_from_download_url(gemma_mmproj_url));

    write_gguf_file(model_path);
    write_gguf_file(legacy_mmproj_path);
    write_download_metadata(legacy_mmproj_path, llava_mmproj_url);

    std::string error;
    CHECK_FALSE(VisualLlmRuntime::resolve_active_backend("gemma-3-4b-it", &error).has_value());
    CHECK(error ==
          std::string("Visual LLM mmproj file is missing: ")
              + visual_artifact_storage_path(*descriptor, descriptor->artifacts[1]).string());
}

TEST_CASE("VisualLlmRuntime rejects invalid preferred GGUF artifacts") {
    VisualRuntimeTempEnv env;
    const std::string model_url = "https://example.com/gemma-3-4b-it-Q4_K_M.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-gemma-3-4b-it-Q4_K_M.gguf";
    EnvVarGuard model_guard("GEMMA3_4B_MODEL_URL", model_url);
    EnvVarGuard mmproj_guard("GEMMA3_4B_MMPROJ_URL", mmproj_url);

    const auto& descriptor = default_visual_model_descriptor();
    const auto model_path = visual_artifact_storage_path(descriptor, descriptor.artifacts[0]);
    const auto mmproj_path = visual_artifact_storage_path(descriptor, descriptor.artifacts[1]);
    std::filesystem::create_directories(model_path.parent_path());
    std::ofstream(model_path, std::ios::binary | std::ios::trunc).put('x');
    std::ofstream(mmproj_path, std::ios::binary | std::ios::trunc).put('x');

    std::string error;
    CHECK_FALSE(VisualLlmRuntime::resolve_active_backend({}, &error).has_value());
    CHECK(error ==
          std::string("Visual LLM model file is missing: ") + model_path.string());
}
