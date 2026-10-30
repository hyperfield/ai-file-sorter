#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "ImageAnalyzerFactory.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"
#include "VisualModelCatalog.hpp"
#include "VisualLlmRuntime.hpp"

#ifdef _WIN32
#include <QProcessEnvironment>
#endif

#include <fstream>

#ifdef _WIN32
namespace {

struct LlmStorageOverrideGuard {
    std::string previous = Utils::get_llm_storage_directory_override();
    ~LlmStorageOverrideGuard() { Utils::set_llm_storage_directory_override(previous); }
};

} // namespace
#endif

TEST_CASE("ImageAnalyzerFactory rejects invalid GGUF artifacts before analyzer startup")
{
    TempDir temp;

    const auto* descriptor = find_visual_model_descriptor("gemma-3-4b-it");
    REQUIRE(descriptor != nullptr);
    REQUIRE(descriptor->artifacts.size() == 2);

    const auto model_path = temp.path() / "model.gguf";
    const auto mmproj_path = temp.path() / "mmproj.gguf";
    std::ofstream(model_path, std::ios::binary).put('x');
    std::ofstream(mmproj_path, std::ios::binary).put('x');

    VisualLlmRuntime::Backend backend;
    backend.descriptor = descriptor;
    backend.artifacts.push_back({&descriptor->artifacts[0], model_path});
    backend.artifacts.push_back({&descriptor->artifacts[1], mmproj_path});

    ImageAnalyzerSettings settings;
    settings.use_gpu = false;

    REQUIRE_THROWS_WITH(
        ImageAnalyzerFactory::create(backend, settings),
        Catch::Matchers::ContainsSubstring(
            "Visual model artifact is invalid or incomplete (expected GGUF header):"));
}

#ifdef _WIN32
TEST_CASE("ImageAnalyzerFactory passes configured model storage to visual GPU preflight")
{
    LlmStorageOverrideGuard storage_guard;
    TempDir storage_dir;
    Utils::set_llm_storage_directory_override(storage_dir.path().string());

    const QProcessEnvironment environment =
        ImageAnalyzerFactoryTestAccess::visual_gpu_preflight_environment();

    CHECK(environment.value(QStringLiteral("AI_FILE_SORTER_VISUAL_SKIP_GPU_PREFLIGHT")) ==
          QStringLiteral("1"));
    CHECK(environment.value(QStringLiteral("AI_FILE_SORTER_VISUAL_USE_GPU")) ==
          QStringLiteral("1"));
    CHECK(environment.value(QStringLiteral("AI_FILE_SORTER_LLM_STORAGE_DIR")) ==
          QString::fromStdString(storage_dir.path().string()));
}
#endif
