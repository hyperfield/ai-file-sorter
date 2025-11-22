#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"

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
