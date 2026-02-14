#ifndef CUSTOMLLMDIALOG_HPP
#define CUSTOMLLMDIALOG_HPP

#include "Types.hpp"

#include <QDialog>

class QLineEdit;
class QPushButton;
class QTextEdit;

/**
 * @brief Dialog for creating or editing custom local LLM entries.
 */
class CustomLLMDialog : public QDialog
{
public:
    /**
     * @brief Construct a dialog for a new custom LLM entry.
     * @param parent Parent widget.
     */
    explicit CustomLLMDialog(QWidget* parent = nullptr);
    /**
     * @brief Construct a dialog pre-populated with an existing entry.
     * @param parent Parent widget.
     * @param existing Existing custom LLM values to edit.
     */
    explicit CustomLLMDialog(QWidget* parent, const CustomLLM& existing);

    /**
     * @brief Return the dialog values as a CustomLLM entry.
     */
    CustomLLM result() const;

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
     * @param existing Existing custom LLM values to load.
     */
    void apply_existing(const CustomLLM& existing);
    /**
     * @brief Validate inputs and update the ok button state.
     */
    void validate_inputs();
    /**
     * @brief Open a file picker to select a local model file.
     */
    void browse_for_file();

    QLineEdit* name_edit{nullptr};
    QTextEdit* description_edit{nullptr};
    QLineEdit* path_edit{nullptr};
    QPushButton* browse_button{nullptr};
    QPushButton* ok_button{nullptr};
};

#endif // CUSTOMLLMDIALOG_HPP
