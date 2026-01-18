#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"

TEST_CASE("Custom API endpoints persist across Settings load/save") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();

    CustomApiEndpoint endpoint;
    endpoint.name = "Local LM Studio";
    endpoint.description = "Local OpenAI-compatible endpoint";
    endpoint.base_url = "http://localhost:1234/v1";
    endpoint.api_key = "local-key";
    endpoint.model = "llama-3.1";

    const std::string id = settings.upsert_custom_api_endpoint(endpoint);
    REQUIRE_FALSE(id.empty());
    settings.set_active_custom_api_id(id);
    REQUIRE(settings.save());

    Settings reloaded;
    reloaded.load();
    const CustomApiEndpoint loaded = reloaded.find_custom_api_endpoint(id);

    REQUIRE(loaded.id == id);
    REQUIRE(loaded.name == endpoint.name);
    REQUIRE(loaded.description == endpoint.description);
    REQUIRE(loaded.base_url == endpoint.base_url);
    REQUIRE(loaded.api_key == endpoint.api_key);
    REQUIRE(loaded.model == endpoint.model);
    REQUIRE(reloaded.get_active_custom_api_id() == id);
}
