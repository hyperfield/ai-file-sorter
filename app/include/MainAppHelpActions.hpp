#ifndef MAIN_APP_HELP_ACTIONS_HPP
#define MAIN_APP_HELP_ACTIONS_HPP

class QWidget;

class MainAppHelpActions {
public:
    static void show_about(QWidget* parent);
    static void show_agpl_info(QWidget* parent);
    static void open_support_page();
};

#endif // MAIN_APP_HELP_ACTIONS_HPP
