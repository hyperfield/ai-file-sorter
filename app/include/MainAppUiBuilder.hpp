#ifndef MAIN_APP_UI_BUILDER_HPP
#define MAIN_APP_UI_BUILDER_HPP

class MainApp;

class MainAppUiBuilder {
public:
    void build(MainApp& app);

private:
    void build_central_panel(MainApp& app);
    void build_menus(MainApp& app);
};

#endif
