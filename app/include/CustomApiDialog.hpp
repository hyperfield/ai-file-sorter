#ifndef CUSTOMAPIDIALOG_HPP
#define CUSTOMAPIDIALOG_HPP

#include "Types.hpp"

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTextEdit;

/**
 * @brief Dialog for creating or editing custom OpenAI-compatible API entries.
 */
class CustomApiDialog : public QDialog
{
public:
    /**
     * @brief Construct a dialog for a new custom API entry.
     * @param parent Parent widget.
     */
    explicit CustomApiDialog(QWidget* parent = nullptr);
    /**
     * @brief Construct a dialog pre-populated with an existing entry.
     * @param parent Parent widget.
     * @param existing Existing custom API values to edit.
     */
    explicit CustomApiDialog(QWidget* parent, const CustomApiEndpoint& existing);

    /**
     * @brief Return the dialog values as a CustomApiEndpoint entry.
     */
    CustomApiEndpoint result() const;

private:
    /**
     * @brief Build the dialog layout and widgets.
     */
    void setup_ui();
    /**
     * @brief Connect widget signals to validation and handlers.
     */
    void wire_signals();
    /**
     * @brief Apply existing values to the input fields.
     * @param existing Existing custom API values to load.
     */
    void apply_existing(const CustomApiEndpoint& existing);
    /**
     * @brief Validate inputs and update the ok button state.
     */
    void validate_inputs();

    QLineEdit* name_edit{nullptr};
    QTextEdit* description_edit{nullptr};
    QLineEdit* base_url_edit{nullptr};
    QLineEdit* model_edit{nullptr};
    QLineEdit* api_key_edit{nullptr};
    QCheckBox* show_api_key_checkbox{nullptr};
    QPushButton* ok_button{nullptr};
};

#endif // CUSTOMAPIDIALOG_HPP
