#include <catch2/catch_test_macros.hpp>

#include "ErrorMessages.hpp"
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"

#include <memory>
#include <string>

namespace {

std::unique_ptr<MainApp> make_window(Settings& settings, LLMChoice choice)
{
    settings.set_llm_choice(choice);
    settings.set_language(Language::English);
    REQUIRE(settings.save());
    return std::make_unique<MainApp>(settings, false, true);
}

} // namespace

TEST_CASE("Custom API analysis does not require public internet", "[analysis-connection]")
{
    QtAppContext qt_context;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    for (const auto* base_url : {"http://127.0.0.1:1234/v1",
                                 "http://localhost:1234/v1",
                                 "http://[::1]:1234/v1",
                                 "http://192.168.1.10:1234/v1",
                                 "https://api.example.invalid/v1"}) {
        CAPTURE(base_url);
        Settings settings;
        CustomApiEndpoint endpoint;
        endpoint.name = "Custom API";
        endpoint.base_url = base_url;
        endpoint.model = "test-model";
        const auto id = settings.upsert_custom_api_endpoint(endpoint);
        REQUIRE_FALSE(id.empty());
        settings.set_active_custom_api_id(id);
        const auto window = make_window(settings, LLMChoice::Remote_Custom);

        int network_checks = 0;
        const auto error = MainAppTestAccess::analysis_connection_error(*window, [&]() {
            ++network_checks;
            return false;
        });

        CHECK_FALSE(error.has_value());
        CHECK(network_checks == 0);
    }
}

TEST_CASE("Custom API analysis still validates endpoint settings without internet", "[analysis-connection]")
{
    QtAppContext qt_context;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    settings.set_active_custom_api_id("missing-endpoint");
    const auto window = make_window(settings, LLMChoice::Remote_Custom);

    int network_checks = 0;
    const auto error = MainAppTestAccess::analysis_connection_error(*window, [&]() {
        ++network_checks;
        return false;
    });

    REQUIRE(error.has_value());
    CHECK(error->find("Custom API endpoint is missing required settings") != std::string::npos);
    CHECK(network_checks == 0);
}

TEST_CASE("Built-in API analysis reports unavailable internet", "[analysis-connection]")
{
    QtAppContext qt_context;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    for (const auto choice : {LLMChoice::Remote_OpenAI, LLMChoice::Remote_Gemini}) {
        CAPTURE(static_cast<int>(choice));
        Settings settings;
        const auto window = make_window(settings, choice);
        int network_checks = 0;
        const auto error = MainAppTestAccess::analysis_connection_error(*window, [&]() {
            ++network_checks;
            return false;
        });

        REQUIRE(error.has_value());
        CHECK(*error == ERR_NO_INTERNET_CONNECTION);
        CHECK(network_checks == 1);
    }
}

TEST_CASE("Remote analysis still requires credentials when internet is available", "[analysis-connection]")
{
    QtAppContext qt_context;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    for (const auto choice : {LLMChoice::Remote_OpenAI, LLMChoice::Remote_Gemini}) {
        CAPTURE(static_cast<int>(choice));
        Settings settings;
        const auto window = make_window(settings, choice);
        int network_checks = 0;
        const auto error = MainAppTestAccess::analysis_connection_error(*window, [&]() {
            ++network_checks;
            return true;
        });

        REQUIRE(error.has_value());
        CHECK(error->find("API key") != std::string::npos);
        CHECK(network_checks == 1);
    }
}

TEST_CASE("Local model analysis does not require internet", "[analysis-connection]")
{
    QtAppContext qt_context;
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    const auto window = make_window(settings, LLMChoice::Local_4b_Qwen35);

    int network_checks = 0;
    const auto error = MainAppTestAccess::analysis_connection_error(*window, [&]() {
        ++network_checks;
        return false;
    });

    CHECK_FALSE(error.has_value());
    CHECK(network_checks == 0);
}
