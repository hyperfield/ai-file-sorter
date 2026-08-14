#include <catch2/catch_test_macros.hpp>

#include "AppTheme.hpp"
#include "CategorizationProgressDialog.hpp"
#include "TestHelpers.hpp"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QPalette>

TEST_CASE("AppTheme appends Windows dark-mode opt-in only when appropriate")
{
    CHECK(AppTheme::windows_platform_dark_mode_spec("") == "windows:darkmode=2");
    CHECK(AppTheme::windows_platform_dark_mode_spec("windows") == "windows:darkmode=2");
    CHECK(AppTheme::windows_platform_dark_mode_spec("windows:fontengine=freetype") ==
          "windows:fontengine=freetype,darkmode=2");
    CHECK(AppTheme::windows_platform_dark_mode_spec("windows:darkmode=1") ==
          "windows:darkmode=1");
    CHECK(AppTheme::windows_platform_dark_mode_spec("offscreen") == "offscreen");
}

TEST_CASE("AppTheme review stylesheet follows the active palette")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#1e1f22")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#f0f3f8")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#151618")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#202327")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#f0f3f8")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#272a2f")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f0f3f8")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3d7eff")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#f5f7fa")));
    palette.setColor(QPalette::Mid, QColor(QStringLiteral("#4a4f58")));
    palette.setColor(QPalette::Dark, QColor(QStringLiteral("#0e1013")));
    palette.setColor(QPalette::Shadow, QColor(QStringLiteral("#050607")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#7f8794")));

    const QString style_sheet = AppTheme::review_dialog_style_sheet(palette);

    CHECK(style_sheet.contains(QStringLiteral("#151618")));
    CHECK(style_sheet.contains(QStringLiteral("#3d7eff")));
    CHECK(style_sheet.contains(QStringLiteral("#f0f3f8")));
    CHECK_FALSE(style_sheet.contains(QStringLiteral("#fbfcfe")));
    CHECK_FALSE(style_sheet.contains(QStringLiteral("#ffffff")));
    CHECK_FALSE(style_sheet.contains(QStringLiteral("#f4f6f8")));
    CHECK_FALSE(style_sheet.contains(QStringLiteral("#d7e8f7")));
}

TEST_CASE("AppTheme ignores broken light-mode alternate row colors")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#f2f2f2")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#111111")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#000000")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#111111")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#f7f7f7")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#111111")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#0a64c8")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Mid, QColor(QStringLiteral("#b8b8b8")));
    palette.setColor(QPalette::Dark, QColor(QStringLiteral("#8f8f8f")));
    palette.setColor(QPalette::Shadow, QColor(QStringLiteral("#5f5f5f")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#7f7f7f")));

    const QString style_sheet = AppTheme::file_listing_panel_style_sheet(palette);

    CHECK(style_sheet.contains(QStringLiteral("#fefefe")));
    CHECK_FALSE(style_sheet.contains(QStringLiteral("#000000")));
}

TEST_CASE("Progress dialog palette changes do not recursively rewrite styles")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizationProgressDialog dialog(nullptr, nullptr, false);
    const QString initial_style_sheet = dialog.styleSheet();
    REQUIRE_FALSE(initial_style_sheet.isEmpty());

    for (int i = 0; i < 20; ++i) {
        QEvent palette_change(QEvent::PaletteChange);
        QApplication::sendEvent(&dialog, &palette_change);
        CHECK(dialog.styleSheet() == initial_style_sheet);
    }
}
