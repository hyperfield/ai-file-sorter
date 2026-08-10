#include "AppTheme.hpp"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace {

bool starts_with_case_insensitive(std::string_view value, std::string_view prefix)
{
    if (value.size() < prefix.size()) {
        return false;
    }

    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const unsigned char lhs = static_cast<unsigned char>(value[i]);
        const unsigned char rhs = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
    }
    return true;
}

bool contains_case_insensitive(std::string_view value, std::string_view needle)
{
    return std::search(
               value.begin(),
               value.end(),
               needle.begin(),
               needle.end(),
               [](char lhs, char rhs) {
                   return std::tolower(static_cast<unsigned char>(lhs)) ==
                          std::tolower(static_cast<unsigned char>(rhs));
               }) != value.end();
}

void set_process_env(const char* key, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

bool palette_is_dark(const QPalette& palette)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    return window.lightnessF() < text.lightnessF();
}

bool alternate_base_looks_unusable(const QColor& base,
                                   const QColor& alternate_base,
                                   bool dark)
{
    if (!alternate_base.isValid() || alternate_base == base) {
        return true;
    }

    const qreal base_lightness = base.lightnessF();
    const qreal alternate_lightness = alternate_base.lightnessF();
    const qreal lightness_delta = alternate_lightness - base_lightness;

    if (!dark && lightness_delta < -0.18) {
        return true;
    }
    if (dark && lightness_delta > 0.18) {
        return true;
    }

    return false;
}

QColor blend_colors(const QColor& first, const QColor& second, qreal second_weight)
{
    second_weight = std::clamp(second_weight, 0.0, 1.0);
    const qreal first_weight = 1.0 - second_weight;
    return QColor(
        static_cast<int>(std::lround(first.red() * first_weight + second.red() * second_weight)),
        static_cast<int>(std::lround(first.green() * first_weight + second.green() * second_weight)),
        static_cast<int>(std::lround(first.blue() * first_weight + second.blue() * second_weight)));
}

QString css_color(const QColor& color)
{
    return color.name(QColor::HexRgb);
}

QString available_style_key(QString preferred)
{
    const QStringList keys = QStyleFactory::keys();
    const auto it = std::find_if(keys.cbegin(), keys.cend(), [&preferred](const QString& key) {
        return key.compare(preferred, Qt::CaseInsensitive) == 0;
    });
    return it != keys.cend() ? *it : QString();
}

QString preferred_windows_native_style_key()
{
    for (const QString& preferred : {QStringLiteral("Windows11"),
                                     QStringLiteral("WindowsVista"),
                                     QStringLiteral("Windows")}) {
        const QString key = available_style_key(preferred);
        if (!key.isEmpty()) {
            return key;
        }
    }
    return QString();
}

bool current_style_is_fusion(const QApplication& app)
{
    const QStyle* style = app.style();
    if (!style) {
        return false;
    }

    const QString object_name = style->objectName();
    if (object_name.compare(QStringLiteral("Fusion"), Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString class_name = QString::fromLatin1(style->metaObject()->className());
    return class_name.contains(QStringLiteral("Fusion"), Qt::CaseInsensitive);
}

struct ThemeColors {
    QColor panel_background;
    QColor border;
    QColor view_background;
    QColor alternate_background;
    QColor text;
    QColor selection_background;
    QColor selection_text;
    QColor header_background;
    QColor header_text;
    QColor button_background;
    QColor button_text;
    QColor button_border;
    QColor button_hover_background;
    QColor button_hover_border;
    QColor button_pressed_background;
    QColor button_pressed_border;
    QColor button_disabled_background;
    QColor button_disabled_border;
    QColor button_disabled_text;
    QColor primary_background;
    QColor primary_border;
    QColor primary_hover_background;
    QColor primary_hover_border;
    QColor primary_pressed_background;
    QColor primary_pressed_border;
    QColor primary_text;
};

ThemeColors build_theme_colors(const QPalette& palette)
{
    const bool dark = palette_is_dark(palette);
    const QColor window = palette.color(QPalette::Window);
    const QColor window_text = palette.color(QPalette::WindowText);
    const QColor base = palette.color(QPalette::Base);
    QColor alternate_base = palette.color(QPalette::AlternateBase);
    const QColor button = palette.color(QPalette::Button);
    const QColor button_text = palette.color(QPalette::ButtonText);
    const QColor highlight = palette.color(QPalette::Highlight);
    QColor highlighted_text = palette.color(QPalette::HighlightedText);
    const QColor mid = palette.color(QPalette::Mid);
    const QColor dark_role = palette.color(QPalette::Dark);
    const QColor shadow = palette.color(QPalette::Shadow);
    QColor disabled_text = palette.color(QPalette::Disabled, QPalette::Text);

    if (alternate_base_looks_unusable(base, alternate_base, dark)) {
        alternate_base = blend_colors(base, window, dark ? 0.22 : 0.08);
    }
    if (!highlighted_text.isValid()) {
        highlighted_text = highlight.lightnessF() < 0.55 ? QColor(Qt::white) : QColor(Qt::black);
    }
    if (!disabled_text.isValid()) {
        disabled_text = blend_colors(button_text, button, 0.48);
    }

    ThemeColors colors;
    colors.panel_background = blend_colors(window, base, dark ? 0.16 : 0.06);
    colors.border = blend_colors(mid, shadow, dark ? 0.22 : 0.10);
    colors.view_background = base;
    colors.alternate_background = alternate_base;
    colors.text = palette.color(QPalette::Text);
    colors.selection_background = highlight;
    colors.selection_text = highlighted_text;
    colors.header_background = blend_colors(button, window, dark ? 0.10 : 0.03);
    colors.header_text = button_text;
    colors.button_background = button;
    colors.button_text = button_text;
    colors.button_border = blend_colors(mid, shadow, dark ? 0.28 : 0.14);
    colors.button_hover_background = blend_colors(button, highlight, dark ? 0.18 : 0.10);
    colors.button_hover_border = blend_colors(colors.button_border, highlight, 0.55);
    colors.button_pressed_background = blend_colors(button, highlight, dark ? 0.30 : 0.18);
    colors.button_pressed_border = blend_colors(colors.button_border, dark_role, 0.45);
    colors.button_disabled_background = blend_colors(button, window, 0.45);
    colors.button_disabled_border = blend_colors(colors.button_border, window, 0.45);
    colors.button_disabled_text = disabled_text;
    colors.primary_background = highlight;
    colors.primary_border = blend_colors(highlight, dark_role, 0.28);
    colors.primary_hover_background = blend_colors(highlight, button_text, dark ? 0.14 : 0.08);
    colors.primary_hover_border = blend_colors(colors.primary_border, highlight, 0.30);
    colors.primary_pressed_background = blend_colors(highlight, shadow, dark ? 0.26 : 0.16);
    colors.primary_pressed_border = blend_colors(colors.primary_border, shadow, 0.30);
    colors.primary_text = highlighted_text;
    return colors;
}

QString shared_panel_style_sheet(const ThemeColors& colors,
                                 const QString& panel_selector,
                                 const QString& first_view_selector,
                                 const QString& second_view_selector)
{
    return QStringLiteral(R"(
        %1 {
            background-color: %2;
            border: 1px solid %3;
            border-radius: 6px;
        }
        %4,
        %5 {
            background-color: %6;
            alternate-background-color: %7;
            color: %8;
            border: none;
            outline: 0;
        }
        %4::item:selected,
        %5::item:selected {
            background-color: %9;
            color: %10;
        }
    )")
        .arg(panel_selector,
             css_color(colors.panel_background),
             css_color(colors.border),
             first_view_selector,
             second_view_selector,
             css_color(colors.view_background),
             css_color(colors.alternate_background),
             css_color(colors.text),
             css_color(colors.selection_background),
             css_color(colors.selection_text));
}

} // namespace

namespace AppTheme {

std::string windows_platform_dark_mode_spec(std::string_view current_platform_spec)
{
    if (current_platform_spec.empty()) {
        return "windows:darkmode=2";
    }
    if (!starts_with_case_insensitive(current_platform_spec, "windows")) {
        return std::string(current_platform_spec);
    }
    if (contains_case_insensitive(current_platform_spec, "darkmode=")) {
        return std::string(current_platform_spec);
    }

    const char separator = current_platform_spec.find(':') == std::string_view::npos ? ':' : ',';
    std::string updated(current_platform_spec);
    updated.push_back(separator);
    updated += "darkmode=2";
    return updated;
}

void configure_windows_platform_dark_mode()
{
#ifdef _WIN32
    const char* current = std::getenv("QT_QPA_PLATFORM");
    const std::string updated = windows_platform_dark_mode_spec(current ? current : "");
    if (!current || updated != current) {
        set_process_env("QT_QPA_PLATFORM", updated);
    }
#endif
}

void apply_windows_runtime_theme(QApplication& app)
{
#ifdef _WIN32
    QStyleHints* style_hints = QGuiApplication::styleHints();
    if (!style_hints) {
        return;
    }

    const bool use_dark_theme = style_hints->colorScheme() == Qt::ColorScheme::Dark;
    if (use_dark_theme) {
        const QString fusion = available_style_key(QStringLiteral("Fusion"));
        if (!fusion.isEmpty() && !current_style_is_fusion(app)) {
            app.setStyle(fusion);
        }
        return;
    }

    if (!current_style_is_fusion(app)) {
        return;
    }

    const QString native_style = preferred_windows_native_style_key();
    if (!native_style.isEmpty()) {
        app.setStyle(native_style);
    }
#else
    Q_UNUSED(app);
#endif
}

QString file_explorer_panel_style_sheet(const QPalette& palette)
{
    const ThemeColors colors = build_theme_colors(palette);
    return QStringLiteral(R"(
        QFrame#aifsFileExplorerPanel {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
        QTreeWidget#aifsNetworkLocationsView,
        QTreeView#aifsFileExplorerView {
            background-color: %3;
            alternate-background-color: %4;
            color: %5;
            border: 1px solid %6;
            border-radius: 4px;
            outline: 0;
        }
        QTreeWidget#aifsNetworkLocationsView::item:selected,
        QTreeView#aifsFileExplorerView::item:selected {
            background-color: %7;
            color: %8;
        }
    )")
        .arg(css_color(colors.panel_background),
             css_color(colors.border),
             css_color(colors.view_background),
             css_color(colors.alternate_background),
             css_color(colors.text),
             css_color(colors.button_border),
             css_color(colors.selection_background),
             css_color(colors.selection_text));
}

QString file_listing_panel_style_sheet(const QPalette& palette)
{
    const ThemeColors colors = build_theme_colors(palette);
    return shared_panel_style_sheet(colors,
                                    QStringLiteral("QFrame#aifsFileListingPanel"),
                                    QStringLiteral("QTreeView#aifsCategorizedResultsView"),
                                    QStringLiteral("QTreeView#aifsFolderContentsView"));
}

QString review_dialog_style_sheet(const QPalette& palette)
{
    const ThemeColors colors = build_theme_colors(palette);
    return QStringLiteral(R"(
        QFrame#aifsReviewTablePanel {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
        QTableView#aifsReviewTable {
            background-color: %3;
            alternate-background-color: %4;
            color: %5;
            border: none;
            gridline-color: %2;
            outline: 0;
            selection-background-color: %6;
            selection-color: %7;
        }
        QTableView#aifsReviewTable QHeaderView::section {
            background-color: %8;
            color: %9;
            border: none;
            border-right: 1px solid %2;
            border-bottom: 1px solid %2;
            padding: 4px 6px;
        }
        QPushButton[aifsReviewActionButton="true"] {
            background-color: %10;
            color: %11;
            border: 1px solid %12;
            border-radius: 6px;
            padding: 5px 13px;
            min-height: 24px;
        }
        QPushButton[aifsReviewActionButton="true"]:hover {
            background-color: %13;
            border-color: %14;
        }
        QPushButton[aifsReviewActionButton="true"]:pressed {
            background-color: %15;
            border-color: %16;
        }
        QPushButton[aifsReviewActionButton="true"]:disabled {
            background-color: %17;
            border-color: %18;
            color: %19;
        }
        QPushButton#aifsReviewPrimaryButton {
            background-color: %20;
            border: 1px solid %21;
            color: %22;
            font-weight: 600;
        }
        QPushButton#aifsReviewPrimaryButton:hover {
            background-color: %23;
            border-color: %24;
        }
        QPushButton#aifsReviewPrimaryButton:pressed {
            background-color: %25;
            border-color: %26;
        }
    )")
        .arg(css_color(colors.panel_background),
             css_color(colors.border),
             css_color(colors.view_background),
             css_color(colors.alternate_background),
             css_color(colors.text),
             css_color(colors.selection_background),
             css_color(colors.selection_text),
             css_color(colors.header_background),
             css_color(colors.header_text),
             css_color(colors.button_background),
             css_color(colors.button_text),
             css_color(colors.button_border),
             css_color(colors.button_hover_background),
             css_color(colors.button_hover_border),
             css_color(colors.button_pressed_background),
             css_color(colors.button_pressed_border),
             css_color(colors.button_disabled_background),
             css_color(colors.button_disabled_border),
             css_color(colors.button_disabled_text),
             css_color(colors.primary_background),
             css_color(colors.primary_border),
             css_color(colors.primary_text),
             css_color(colors.primary_hover_background),
             css_color(colors.primary_hover_border),
             css_color(colors.primary_pressed_background),
             css_color(colors.primary_pressed_border));
}

QString progress_dialog_style_sheet(const QPalette& palette)
{
    const ThemeColors colors = build_theme_colors(palette);
    return QStringLiteral(R"(
        QDialog#analysisProgressDialog {
            background-color: %1;
            color: %2;
        }
        QFrame#analysisStatusPanel,
        QFrame#analysisLogPanel {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 6px;
        }
        QTableWidget#analysisStatusTable {
            background-color: %5;
            alternate-background-color: %6;
            color: %2;
            border: none;
            gridline-color: %4;
            outline: 0;
            selection-background-color: %7;
            selection-color: %8;
        }
        QTableWidget#analysisStatusTable QHeaderView::section {
            background-color: %9;
            color: %10;
            border: none;
            border-right: 1px solid %4;
            border-bottom: 1px solid %4;
            padding: 4px 6px;
        }
        QLabel#analysisLogLabel {
            color: %2;
            font-weight: 600;
        }
        QPlainTextEdit#analysisLogView {
            background-color: %5;
            color: %2;
            border: none;
            border-radius: 4px;
            padding: 6px;
        }
        QPushButton#stopAnalysisButton {
            background-color: %11;
            color: %12;
            border: 1px solid %13;
            border-radius: 6px;
            padding: 5px 13px;
            min-height: 24px;
        }
        QPushButton#stopAnalysisButton:hover {
            background-color: %14;
            border-color: %15;
        }
        QPushButton#stopAnalysisButton:pressed {
            background-color: %16;
            border-color: %17;
        }
        QPushButton#stopAnalysisButton:disabled {
            background-color: %18;
            border-color: %19;
            color: %20;
        }
    )")
        .arg(css_color(colors.panel_background),
             css_color(colors.text),
             css_color(colors.panel_background),
             css_color(colors.border),
             css_color(colors.view_background),
             css_color(colors.alternate_background),
             css_color(colors.selection_background),
             css_color(colors.selection_text),
             css_color(colors.header_background),
             css_color(colors.header_text),
             css_color(colors.button_background),
             css_color(colors.button_text),
             css_color(colors.button_border),
             css_color(colors.button_hover_background),
             css_color(colors.button_hover_border),
             css_color(colors.button_pressed_background),
             css_color(colors.button_pressed_border),
             css_color(colors.button_disabled_background),
             css_color(colors.button_disabled_border),
             css_color(colors.button_disabled_text));
}

} // namespace AppTheme
