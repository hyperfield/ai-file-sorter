#include <catch2/catch_test_macros.hpp>

#include "CacheMaintenanceDialog.hpp"
#include "CacheMaintenanceService.hpp"
#include "TestHelpers.hpp"

#include <QApplication>
#include <QLabel>
#include <QPushButton>

#ifndef _WIN32
TEST_CASE("CacheMaintenanceDialog exposes tooltips for each cache target")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir config_dir;
    TempDir log_dir;
    CacheMaintenanceService service(
        config_dir.path().string(),
        {},
        [log_path = log_dir.path()]() {
            return log_path;
        });

    CacheMaintenanceDialog dialog(service, /*busy=*/false);

    auto* categorization_button = dialog.findChild<QPushButton*>("categorizationClearButton");
    auto* image_location_button = dialog.findChild<QPushButton*>("imageLocationClearButton");
    auto* logs_button = dialog.findChild<QPushButton*>("logsClearButton");

    REQUIRE(categorization_button != nullptr);
    REQUIRE(image_location_button != nullptr);
    REQUIRE(logs_button != nullptr);

    CHECK(categorization_button->isEnabled());
    CHECK(categorization_button->toolTip().contains("sorting suggestions"));
    CHECK(image_location_button->toolTip().contains("photo locations"));
    CHECK(logs_button->toolTip().contains("troubleshooting"));
}

TEST_CASE("CacheMaintenanceDialog lays out wrapped labels without overlapping section titles")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir config_dir;
    TempDir log_dir;
    CacheMaintenanceService service(
        config_dir.path().string(),
        {},
        [log_path = log_dir.path()]() {
            return log_path;
        });

    CacheMaintenanceDialog dialog(service, /*busy=*/false);
    dialog.resize(520, 420);
    dialog.show();
    QApplication::processEvents();

    auto* image_title = dialog.findChild<QLabel*>("imageLocationTitleLabel");
    auto* image_description = dialog.findChild<QLabel*>("imageLocationDescriptionLabel");
    auto* image_path_value = dialog.findChild<QLabel*>("imageLocationPathValueLabel");

    REQUIRE(image_title != nullptr);
    REQUIRE(image_description != nullptr);
    REQUIRE(image_path_value != nullptr);

    CHECK(image_title->wordWrap());
    CHECK(image_description->wordWrap());
    CHECK(image_path_value->wordWrap());
    CHECK(image_title->geometry().bottom() < image_description->geometry().top());
    CHECK(image_description->sizeHint().height() > image_title->fontMetrics().height());
}

TEST_CASE("CacheMaintenanceDialog disables cache clearing controls while busy")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir config_dir;
    TempDir log_dir;
    CacheMaintenanceService service(
        config_dir.path().string(),
        {},
        [log_path = log_dir.path()]() {
            return log_path;
        });

    CacheMaintenanceDialog dialog(service, /*busy=*/true);

    auto* categorization_button = dialog.findChild<QPushButton*>("categorizationClearButton");
    auto* image_location_button = dialog.findChild<QPushButton*>("imageLocationClearButton");
    auto* logs_button = dialog.findChild<QPushButton*>("logsClearButton");
    auto* busy_label = dialog.findChild<QLabel*>("cacheBusyLabel");

    REQUIRE(categorization_button != nullptr);
    REQUIRE(image_location_button != nullptr);
    REQUIRE(logs_button != nullptr);
    REQUIRE(busy_label != nullptr);

    CHECK_FALSE(categorization_button->isEnabled());
    CHECK_FALSE(image_location_button->isEnabled());
    CHECK_FALSE(logs_button->isEnabled());
    CHECK(busy_label->text().contains("analysis is running"));
}
#endif
