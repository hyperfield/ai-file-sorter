#ifndef APPTHEME_HPP
#define APPTHEME_HPP

#include <QCoreApplication>
#include <QString>

#include <string>
#include <string_view>

class QApplication;
class QPalette;

/**
 * @brief Shared helpers for Windows dark-mode setup and palette-aware widget styling.
 */
namespace AppTheme {

/**
 * @brief Returns a Windows Qt platform specification with dark-mode opt-in appended when needed.
 * @param current_platform_spec Existing `QT_QPA_PLATFORM` value.
 * @return Updated platform specification string.
 */
std::string windows_platform_dark_mode_spec(std::string_view current_platform_spec);

/**
 * @brief Enables the Qt Windows dark-mode platform option before `QApplication` is created.
 */
void configure_windows_platform_dark_mode();

/**
 * @brief Applies the preferred runtime widget style for the current Windows color scheme.
 * @param app Active `QApplication` instance.
 */
void apply_windows_runtime_theme(QApplication& app);

/**
 * @brief Builds the file-explorer panel stylesheet for the provided palette.
 * @param palette Palette used to derive container, border, and selection colors.
 * @return Stylesheet text.
 */
QString file_explorer_panel_style_sheet(const QPalette& palette);

/**
 * @brief Builds the main results/file-listing panel stylesheet for the provided palette.
 * @param palette Palette used to derive container, border, and selection colors.
 * @return Stylesheet text.
 */
QString file_listing_panel_style_sheet(const QPalette& palette);

/**
 * @brief Builds the review dialog stylesheet for the provided palette.
 * @param palette Palette used to derive table, button, and selection colors.
 * @return Stylesheet text.
 */
QString review_dialog_style_sheet(const QPalette& palette);

/**
 * @brief Builds the analysis progress dialog stylesheet for the provided palette.
 * @param palette Palette used to derive dialog, table, and button colors.
 * @return Stylesheet text.
 */
QString progress_dialog_style_sheet(const QPalette& palette);

} // namespace AppTheme

#endif // APPTHEME_HPP
