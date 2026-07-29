#include <catch2/catch_test_macros.hpp>

#include "LLMSelectionVisualBackendModel.hpp"

TEST_CASE("LLM selection visual backend model builds built-in and visual custom items")
{
    CustomLLM text_only;
    text_only.id = "text-only";
    text_only.name = "Text Only";
    text_only.path = "model.gguf";

    CustomLLM visual_custom;
    visual_custom.id = "vision";
    visual_custom.name = "Vision Pair";
    visual_custom.path = "model.gguf";
    visual_custom.mmproj_path = "mmproj.gguf";

    const auto items = LLMSelectionVisualBackendModel::build_visual_backend_items(
        {text_only, visual_custom},
        QStringLiteral("Recommended"),
        QStringLiteral("Custom: %1"));

    REQUIRE(items.size() == visual_model_descriptors().size() + 1);
    CHECK(items[0].id == "llava-v1.6-mistral-7b");
    CHECK(items[1].id == "gemma-3-4b-it");
    CHECK(items[1].label == QStringLiteral("Gemma 3 4B IT (Recommended)"));
    CHECK(items.back().id == "custom:vision");
    CHECK(items.back().label == QStringLiteral("Custom: Vision Pair"));
}

TEST_CASE("LLM selection visual backend model chooses requested default and fallback ids")
{
    const auto items = LLMSelectionVisualBackendModel::build_visual_backend_items(
        {},
        QStringLiteral("Recommended"),
        QStringLiteral("Custom: %1"));

    CHECK(LLMSelectionVisualBackendModel::choose_visual_backend_id("llava-v1.6-mistral-7b", items)
          == "llava-v1.6-mistral-7b");
    CHECK(LLMSelectionVisualBackendModel::choose_visual_backend_id("missing", items)
          == "gemma-3-4b-it");
    CHECK(LLMSelectionVisualBackendModel::choose_visual_backend_id("", {}) == "");
    CHECK(LLMSelectionVisualBackendModel::index_of_visual_backend_id(items, "gemma-3-4b-it") == 1);
    CHECK(LLMSelectionVisualBackendModel::index_of_visual_backend_id(items, "missing") == -1);
}

TEST_CASE("LLM selection visual backend model resolves canonical descriptors")
{
    const auto* custom_descriptor =
        LLMSelectionVisualBackendModel::selected_visual_model_descriptor("custom:vision");
    REQUIRE(custom_descriptor != nullptr);
    CHECK(std::string(custom_descriptor->id) == "custom");
    CHECK(LLMSelectionVisualBackendModel::canonical_visual_backend_id("custom:vision")
          == "custom:vision");

    const auto* fallback_descriptor =
        LLMSelectionVisualBackendModel::selected_visual_model_descriptor("missing");
    REQUIRE(fallback_descriptor != nullptr);
    CHECK(std::string(fallback_descriptor->id) == "gemma-3-4b-it");
    CHECK(LLMSelectionVisualBackendModel::canonical_visual_backend_id("missing")
          == "gemma-3-4b-it");
}
