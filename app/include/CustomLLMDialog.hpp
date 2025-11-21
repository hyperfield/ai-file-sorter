#ifndef CUSTOMLLMDIALOG_HPP
#define CUSTOMLLMDIALOG_HPP

#include "Types.hpp"

#include <QDialog>

class QLineEdit;
class QPushButton;
class QTextEdit;

class CustomLLMDialog : public QDialog
{
public:
    explicit CustomLLMDialog(QWidget* parent = nullptr);
    explicit CustomLLMDialog(QWidget* parent, const CustomLLM& existing);

    CustomLLM result() const;

private:
    void setup_ui();
    void wire_signals();
    void apply_existing(const CustomLLM& existing);
    void validate_inputs();
    void browse_for_file();

    QLineEdit* name_edit{nullptr};
    QTextEdit* description_edit{nullptr};
    QLineEdit* path_edit{nullptr};
    QPushButton* browse_button{nullptr};
    QPushButton* ok_button{nullptr};
};

#endif // CUSTOMLLMDIALOG_HPP
