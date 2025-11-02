#ifndef MAIN_APP_UI_BUILDER_HPP
#define MAIN_APP_UI_BUILDER_HPP

#include <QIcon>
#include <QStyle>

class MainApp;

class MainAppUiBuilder {
public:
    void build(MainApp& app);

private:
    void build_central_panel(MainApp& app);
    void build_menus(MainApp& app);
    void build_file_menu(MainApp& app);
    void build_edit_menu(MainApp& app);
    void build_view_menu(MainApp& app);
    void build_settings_menu(MainApp& app);
    void build_help_menu(MainApp& app);
    static QIcon icon_for(MainApp& app, const char* name, QStyle::StandardPixmap fallback);
};

#endif
