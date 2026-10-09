#include <catch2/catch_test_macros.hpp>

#include "ApiImageAnalyzer.hpp"
#include "LLMSelectionDialog.hpp"
#include "LLMSelectionDialogTestAccess.hpp"
#include "LLMSelectionVisualBackendModel.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "VisualLlmRuntime.hpp"
#include "VisualModelCatalog.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace {

CustomApiEndpoint make_endpoint(const std::string& id,
                                const std::string& name,
                                bool supports_vision,
                                const std::string& model = "gemma-4-12b-it-UD-Q6_K_XL")
{
    CustomApiEndpoint endpoint;
    endpoint.id = id;
    endpoint.name = name;
    endpoint.base_url = "http://127.0.0.1:8888/v1";
    endpoint.api_key = "not-needed";
    endpoint.model = model;
    endpoint.supports_vision = supports_vision;
    return endpoint;
}

} // namespace

TEST_CASE("Custom API endpoints persist the vision flag and keep their id") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    const std::string id = settings.upsert_custom_api_endpoint(make_endpoint("", "Unsloth Local", true));
    REQUIRE_FALSE(id.empty());
    REQUIRE(settings.save());

    Settings reloaded;
    reloaded.load();
    const CustomApiEndpoint loaded = reloaded.find_custom_api_endpoint(id);
    REQUIRE(loaded.id == id);
    REQUIRE(loaded.supports_vision);

    reloaded.upsert_custom_api_endpoint(make_endpoint(id, "Unsloth Local", false));
    REQUIRE(reloaded.save());
    Settings text_only;
    text_only.load();
    REQUIRE(text_only.find_custom_api_endpoint(id).supports_vision == false);
    REQUIRE(text_only.find_custom_api_endpoint(id).id == id);
}

TEST_CASE("Custom API endpoints saved without a vision field load as text-only") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    const std::string id = settings.upsert_custom_api_endpoint(make_endpoint("api_legacy", "Legacy", true));
    REQUIRE(settings.save());

    // Simulate a config written before the vision field existed by removing that key.
    const auto config_file = config_dir.path() / "AIFileSorter" / "config.ini";
    std::ifstream in(config_file);
    REQUIRE(in.is_open());
    std::string kept;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("SupportsVision") == std::string::npos) {
            kept += line + "\n";
        }
    }
    in.close();
    {
        std::ofstream out(config_file, std::ios::trunc);
        out << kept;
    }

    Settings legacy;
    legacy.load();
    const CustomApiEndpoint loaded = legacy.find_custom_api_endpoint(id);
    REQUIRE(loaded.id == id);
    REQUIRE(loaded.name == "Legacy");
    REQUIRE_FALSE(loaded.supports_vision);
}

TEST_CASE("API visual backend ids are distinct from custom local ids") {
    REQUIRE(api_visual_model_id_for_endpoint("7f4c") == "api:7f4c");
    REQUIRE(api_visual_model_id_for_endpoint("").empty());
    REQUIRE(is_api_visual_model_id("api:7f4c"));
    REQUIRE_FALSE(is_custom_visual_model_id("api:7f4c"));
    REQUIRE(is_custom_visual_model_id("custom:llm1"));
    REQUIRE_FALSE(is_api_visual_model_id("custom:llm1"));
    REQUIRE_FALSE(is_api_visual_model_id("api:"));
    REQUIRE_FALSE(api_endpoint_id_from_visual_model_id("api:").has_value());
}

TEST_CASE("Visual backend list offers only vision-capable API endpoints") {
    const std::vector<CustomApiEndpoint> endpoints = {
        make_endpoint("api_vis", "Unsloth Local", true),
        make_endpoint("api_txt", "Text Only", false),
    };
    const auto items = LLMSelectionVisualBackendModel::build_visual_backend_items(
        {},
        "Recommended",
        "Custom: %1",
        endpoints,
        "API: %1");

    const std::string vision_id = "api:api_vis";
    const std::string text_id = "api:api_txt";
    REQUIRE(LLMSelectionVisualBackendModel::index_of_visual_backend_id(items, vision_id) >= 0);
    REQUIRE(LLMSelectionVisualBackendModel::index_of_visual_backend_id(items, text_id) < 0);

    const int index = LLMSelectionVisualBackendModel::index_of_visual_backend_id(items, vision_id);
    REQUIRE(items[static_cast<size_t>(index)].label == QStringLiteral("API: Unsloth Local"));

    // Built-in entries keep their existing ids.
    REQUIRE(LLMSelectionVisualBackendModel::index_of_visual_backend_id(
                items, default_visual_model_descriptor().id) >= 0);
}

TEST_CASE("Visual backend list keeps existing custom local ids unchanged") {
    CustomLLM custom;
    custom.id = "llm1";
    custom.name = "Local vision";
    custom.path = "/models/text.gguf";
    custom.mmproj_path = "/models/mmproj.gguf";

    const auto items = LLMSelectionVisualBackendModel::build_visual_backend_items(
        {custom},
        "Recommended",
        "Custom: %1",
        {make_endpoint("api_vis", "Unsloth Local", true)},
        "API: %1");

    REQUIRE(LLMSelectionVisualBackendModel::index_of_visual_backend_id(items, "custom:llm1") >= 0);
    REQUIRE(LLMSelectionVisualBackendModel::index_of_visual_backend_id(items, "api:api_vis") >= 0);
}

TEST_CASE("Deleted API visual selection falls back to the default visual backend") {
    const auto items = LLMSelectionVisualBackendModel::build_visual_backend_items(
        {},
        "Recommended",
        "Custom: %1",
        {make_endpoint("api_vis", "Unsloth Local", true)},
        "API: %1");

    const std::string chosen = LLMSelectionVisualBackendModel::choose_visual_backend_id("api:api_gone", items);
    REQUIRE(chosen == std::string(default_visual_model_descriptor().id));
}

TEST_CASE("API visual selections are canonical and have no local descriptor") {
    REQUIRE(LLMSelectionVisualBackendModel::canonical_visual_backend_id("api:api_vis") == "api:api_vis");
    REQUIRE(LLMSelectionVisualBackendModel::selected_visual_model_descriptor("api:api_vis") == nullptr);
}

TEST_CASE("Settings keep an API visual model id instead of resetting it") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    settings.set_visual_model_id("api:api_vis");
    REQUIRE(settings.get_visual_model_id() == "api:api_vis");
}

TEST_CASE("API visual backend never resolves local model files") {
    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend("api:api_vis", {}, &error);
    REQUIRE_FALSE(backend.has_value());
    REQUIRE_FALSE(error.empty());

    std::string paths_error;
    REQUIRE_FALSE(VisualLlmRuntime::resolve_paths("api:api_vis", {}, &paths_error).has_value());
}

TEST_CASE("API visual endpoint resolution explains each failure") {
    CustomApiEndpoint incomplete = make_endpoint("api_bad", "Incomplete", true, "");
    const std::vector<CustomApiEndpoint> endpoints = {
        make_endpoint("api_vis", "Unsloth Local", true),
        make_endpoint("api_txt", "Text Only", false),
        incomplete,
    };

    std::string error;
    const auto ok = ApiImageAnalyzer::resolve_visual_api_endpoint("api:api_vis", endpoints, &error);
    REQUIRE(ok.has_value());
    REQUIRE(ok->name == "Unsloth Local");

    error.clear();
    REQUIRE_FALSE(ApiImageAnalyzer::resolve_visual_api_endpoint("api:api_txt", endpoints, &error).has_value());
    REQUIRE(error.find("image") != std::string::npos);

    error.clear();
    REQUIRE_FALSE(ApiImageAnalyzer::resolve_visual_api_endpoint("api:api_bad", endpoints, &error).has_value());
    REQUIRE(error.find("incomplete") != std::string::npos);

    error.clear();
    REQUIRE_FALSE(ApiImageAnalyzer::resolve_visual_api_endpoint("api:api_gone", endpoints, &error).has_value());
    REQUIRE(error.find("missing") != std::string::npos);

    error.clear();
    REQUIRE_FALSE(ApiImageAnalyzer::resolve_visual_api_endpoint("custom:llm1", endpoints, &error).has_value());
    REQUIRE(error.find("not an API endpoint") != std::string::npos);
}

TEST_CASE("LLM concurrency control is enabled for an API visual backend even with a local text model") {
    QtAppContext qt_context;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);
    const std::string endpoint_id = settings.upsert_custom_api_endpoint(make_endpoint("", "Unsloth Local", true));
    REQUIRE_FALSE(endpoint_id.empty());

    LLMSelectionDialog dialog(settings);

    LLMSelectionDialogTestAccess::select_visual_backend(dialog, std::string(default_visual_model_descriptor().id));
    REQUIRE_FALSE(LLMSelectionDialogTestAccess::llm_concurrency_enabled(dialog));

    LLMSelectionDialogTestAccess::select_visual_backend(dialog, api_visual_model_id_for_endpoint(endpoint_id));
    REQUIRE(LLMSelectionDialogTestAccess::llm_concurrency_enabled(dialog));
}
