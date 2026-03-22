#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Updater.hpp"
#include "UpdaterTestAccess.hpp"

#include <QAbstractButton>
#include <QMessageBox>
#include <QTimer>

#include <memory>

namespace {

void schedule_message_box_button_click(const QString& target_text, bool* saw_button = nullptr)
{
    auto clicker = std::make_shared<std::function<void(int)>>();
    *clicker = [clicker, target_text, saw_button](int attempts_remaining) {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            if (attempts_remaining > 0) {
                QTimer::singleShot(0, [clicker, attempts_remaining]() {
                    (*clicker)(attempts_remaining - 1);
                });
                return;
            }
            if (saw_button) {
                *saw_button = false;
            }
            return;
        }

        for (auto* button : box->buttons()) {
            if (button && button->text() == target_text) {
                if (saw_button) {
                    *saw_button = true;
                }
                button->click();
                return;
            }
        }

        if (saw_button) {
            *saw_button = false;
        }
        if (auto* ok_button = box->button(QMessageBox::Ok)) {
            ok_button->click();
        }
    };

    QTimer::singleShot(0, [clicker]() {
        (*clicker)(10);
    });
}

} // namespace

TEST_CASE("Updater error dialog offers manual update fallback without quitting when not requested")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    EnvVarGuard update_spec_guard("UPDATE_SPEC_FILE_URL", "https://example.com/aifs_version.json");

    Settings settings;
    Updater updater(settings);

    std::string opened_url;
    bool quit_called = false;
    bool saw_manual_button = false;

    UpdaterTestAccess::set_open_download_url_handler(updater, [&](const std::string& url) {
        opened_url = url;
    });
    UpdaterTestAccess::set_quit_handler(updater, [&]() {
        quit_called = true;
    });

    UpdateInfo info;
    info.download_url = "https://filesorter.app/download";

    schedule_message_box_button_click(QStringLiteral("Update manually"), &saw_manual_button);

    const bool result = UpdaterTestAccess::handle_update_error(
        updater,
        info,
        QStringLiteral("Failed to prepare the update installer."),
        nullptr,
        false);

    CHECK(result);
    CHECK(saw_manual_button);
    CHECK(opened_url == info.download_url);
    CHECK_FALSE(quit_called);
}

TEST_CASE("Updater error dialog can request quit after manual fallback")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    EnvVarGuard update_spec_guard("UPDATE_SPEC_FILE_URL", "https://example.com/aifs_version.json");

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
    info.download_url = "https://filesorter.app/download";

    schedule_message_box_button_click(QStringLiteral("Update manually"));

    const bool result = UpdaterTestAccess::handle_update_error(
        updater,
        info,
        QStringLiteral("The installer could not be launched."),
        nullptr,
        true);

    CHECK(result);
    CHECK(opened_url == info.download_url);
    CHECK(quit_called);
}

TEST_CASE("Updater error dialog omits manual fallback when no download URL is available")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    EnvVarGuard update_spec_guard("UPDATE_SPEC_FILE_URL", "https://example.com/aifs_version.json");

    Settings settings;
    Updater updater(settings);

    std::string opened_url;
    bool quit_called = false;
    bool saw_manual_button = true;

    UpdaterTestAccess::set_open_download_url_handler(updater, [&](const std::string& url) {
        opened_url = url;
    });
    UpdaterTestAccess::set_quit_handler(updater, [&]() {
        quit_called = true;
    });

    UpdateInfo info;

    schedule_message_box_button_click(QStringLiteral("Update manually"), &saw_manual_button);

    const bool result = UpdaterTestAccess::handle_update_error(
        updater,
        info,
        QStringLiteral("No download target is available for this update."),
        nullptr,
        true);

    CHECK_FALSE(result);
    CHECK_FALSE(saw_manual_button);
    CHECK(opened_url.empty());
    CHECK_FALSE(quit_called);
}
