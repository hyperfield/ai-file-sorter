#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "UpdateFeed.hpp"
#include "Updater.hpp"
#include "UpdaterBuildConfig.hpp"
#include "UpdaterLaunchOptions.hpp"
#include "UpdaterTestAccess.hpp"

#include <QApplication>

namespace {

constexpr UpdaterBuildConfig::Mode expected_mode()
{
#if defined(AI_FILE_SORTER_EXPECTED_UPDATE_MODE_DISABLED)
    return UpdaterBuildConfig::Mode::Disabled;
#elif defined(AI_FILE_SORTER_EXPECTED_UPDATE_MODE_NOTIFY_ONLY)
    return UpdaterBuildConfig::Mode::NotifyOnly;
#elif defined(AI_FILE_SORTER_EXPECTED_UPDATE_MODE_AUTO_INSTALL)
    return UpdaterBuildConfig::Mode::AutoInstall;
#else
    static_assert(false, "Expected updater mode macro is not defined for this test target.");
#endif
}

void ensure_qt_application()
{
    if (QApplication::instance()) {
        return;
    }

    static int argc = 1;
    static char arg0[] = "updater-build-mode-tests";
    static char* argv[] = {arg0, nullptr};
    static QApplication app(argc, argv);
    Q_UNUSED(app);
}

} // namespace

TEST_CASE("Updater build config matches the expected mode for this target")
{
    CHECK(UpdaterBuildConfig::current_mode() == expected_mode());
    CHECK(UpdaterBuildConfig::update_checks_enabled()
          == (expected_mode() != UpdaterBuildConfig::Mode::Disabled));
    CHECK(UpdaterBuildConfig::auto_install_enabled()
          == (expected_mode() == UpdaterBuildConfig::Mode::AutoInstall));
}

TEST_CASE("Updater begin schedules work only when update checks are enabled")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    ensure_qt_application();

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    EnvVarGuard update_spec_guard("UPDATE_SPEC_FILE_URL", std::nullopt);

    Settings settings;
    Updater updater(settings);

    updater.begin();

    CHECK(UpdaterTestAccess::has_update_task(updater)
          == (expected_mode() != UpdaterBuildConfig::Mode::Disabled));
    UpdaterTestAccess::wait_for_update_task(updater);
}

TEST_CASE("Notify-only updater opens the download page instead of preparing an installer")
{
    if constexpr (expected_mode() != UpdaterBuildConfig::Mode::NotifyOnly) {
        SUCCEED("This target is not built in notify-only mode.");
        return;
    }

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    Updater updater(settings);

    std::string opened_url;
    bool quit_called = false;

    UpdaterTestAccess::set_open_download_url_handler(updater, [&](const std::string& url) {
        opened_url = url;
    });
    UpdaterTestAccess::set_quit_handler(updater, [&]() {
        quit_called = true;
    });

    UpdateInfo info;
    info.current_version = "9.9.9";
    info.download_url = "https://filesorter.app/downloads/AIFileSorterSetup.zip";
    info.installer_url = "https://filesorter.app/downloads/AIFileSorterSetup.exe";
    info.installer_sha256 = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";

    const bool result = UpdaterTestAccess::trigger_update_action(updater, info, nullptr, true);

    CHECK(result);
    CHECK(opened_url == info.download_url);
    CHECK(quit_called);
}

TEST_CASE("Disabled updater mode ignores live-test environment when startup checks are blocked")
{
    if constexpr (expected_mode() != UpdaterBuildConfig::Mode::Disabled) {
        SUCCEED("This target still allows updater startup checks.");
        return;
    }

    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    ensure_qt_application();

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    EnvVarGuard update_spec_guard("UPDATE_SPEC_FILE_URL", std::nullopt);
    EnvVarGuard live_test_mode_guard(UpdaterLaunchOptions::kLiveTestModeEnv, std::string("1"));
    EnvVarGuard live_test_url_guard(UpdaterLaunchOptions::kLiveTestUrlEnv,
                                    std::string("https://filesorter.app/downloads/AIFileSorterSetup.zip"));
    EnvVarGuard live_test_sha_guard(UpdaterLaunchOptions::kLiveTestSha256Env,
                                    std::string("AABBCCDDEEFF00112233445566778899AABBCCDDEEFF00112233445566778899"));

    Settings settings;
    Updater updater(settings);

    updater.begin();

    CHECK_FALSE(UpdaterTestAccess::has_update_task(updater));
    CHECK_FALSE(UpdaterTestAccess::current_update_info(updater).has_value());
}
