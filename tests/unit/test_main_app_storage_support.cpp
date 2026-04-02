#include <catch2/catch_test_macros.hpp>

#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "StoragePluginManager.hpp"
#include "TestHelpers.hpp"

#ifndef _WIN32

TEST_CASE("MainApp resolves installable storage support when plugin is not installed")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.load();
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);

    StorageProviderDetection detection;
    detection.provider_id = "dropbox";
    detection.matched = true;
    detection.needs_additional_support = true;
    detection.detection_source = "path_heuristic";

    CHECK(MainAppTestAccess::resolve_storage_support_state_name(window, detection) ==
          "detected_but_plugin_not_installed");
    CHECK(MainAppTestAccess::resolve_storage_support_plugin_id(window, detection) ==
          std::optional<std::string>{"cloud_storage_compat"});
}

TEST_CASE("MainApp resolves plugin-backed storage support when plugin is installed")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.load();
    REQUIRE(settings.save());

    StoragePluginManager plugin_manager(settings.get_config_dir());
    REQUIRE(plugin_manager.install("cloud_storage_compat"));

    MainApp window(settings, /*development_mode=*/false);

    StorageProviderDetection detection;
    detection.provider_id = "dropbox";
    detection.matched = true;
    detection.needs_additional_support = true;
    detection.detection_source = "path_heuristic";

    CHECK(MainAppTestAccess::resolve_storage_support_state_name(window, detection) ==
          "detected_and_supported_via_plugin");
    CHECK(MainAppTestAccess::resolve_storage_support_plugin_id(window, detection) ==
          std::optional<std::string>{"cloud_storage_compat"});
}

TEST_CASE("MainApp resolves unsupported storage support when no plugin exists")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    settings.load();
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);

    StorageProviderDetection detection;
    detection.provider_id = "google_drive";
    detection.matched = true;
    detection.needs_additional_support = true;
    detection.detection_source = "path_heuristic";
    detection.message = "Google Drive folder detected.";

    CHECK(MainAppTestAccess::resolve_storage_support_state_name(window, detection) ==
          "detected_but_no_plugin_exists");
    CHECK_FALSE(MainAppTestAccess::resolve_storage_support_plugin_id(window, detection).has_value());
}

#endif
