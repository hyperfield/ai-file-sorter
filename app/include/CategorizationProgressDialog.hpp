#ifndef CATEGORIZATIONPROGRESSDIALOG_HPP
#define CATEGORIZATIONPROGRESSDIALOG_HPP

#include <QDialog>

#include <string>

class MainApp;
class QPlainTextEdit;
class QPushButton;

class CategorizationProgressDialog : public QDialog
{
public:
    CategorizationProgressDialog(QWidget* parent, MainApp* main_app, bool show_subcategory_col);

    void show();
    void hide();
    void append_text(const std::string& text);

private:
    void setup_ui(bool show_subcategory_col);
    void request_stop();

    MainApp* main_app;
    QPlainTextEdit* text_view{nullptr};
    QPushButton* stop_button{nullptr};
};

#endif // CATEGORIZATIONPROGRESSDIALOG_HPP
