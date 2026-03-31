#ifndef MAIN_APP_UI_BUILDER_HPP
#define MAIN_APP_UI_BUILDER_HPP

#include <QIcon>
#include <QStyle>
#include "UiTranslator.hpp"

class MainApp;

/**
 * @brief Builds the MainApp widget tree, menus, and translation dependencies.
 */
class MainAppUiBuilder {
public:
    /**
     * @brief Builds the main window UI for the provided application instance.
     * @param app Main application window to populate.
     */
    void build(MainApp& app);
    /**
     * @brief Collects the translator dependency bundle from the current UI state.
     * @param app Main application window whose controls are referenced.
     * @return Dependency bundle used by UiTranslator.
     */
    UiTranslator::Dependencies build_translator_dependencies(MainApp& app) const;

private:
    void build_central_panel(MainApp& app);
    void build_menus(MainApp& app);
    void build_file_menu(MainApp& app);
    void build_edit_menu(MainApp& app);
    void build_view_menu(MainApp& app);
    void build_settings_menu(MainApp& app);
    void build_plugins_menu(MainApp& app);
    void build_development_menu(MainApp& app);
    void build_help_menu(MainApp& app);
    static QIcon icon_for(MainApp& app, const char* name, QStyle::StandardPixmap fallback);
};

#endif
