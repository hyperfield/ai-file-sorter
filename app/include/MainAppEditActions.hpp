#ifndef MAIN_APP_EDIT_ACTIONS_HPP
#define MAIN_APP_EDIT_ACTIONS_HPP

#include <QString>

class QLineEdit;

class MainAppEditActions {
public:
    static void on_paste(QLineEdit* line_edit);
    static void on_copy(QLineEdit* line_edit);
    static void on_cut(QLineEdit* line_edit);
    static void on_delete(QLineEdit* line_edit);

private:
    static void copy_to_clipboard(const QString& text);
    static QString get_selection(QLineEdit* line_edit, bool delete_selection);
};

#endif
