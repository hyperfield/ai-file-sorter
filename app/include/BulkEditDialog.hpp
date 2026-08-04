#ifndef BULKEDITDIALOG_HPP
#define BULKEDITDIALOG_HPP

#include <QDialog>

#include <string>

class QLineEdit;
class QPushButton;
class QWidget;

/**
 * @brief Dialog for applying category and optional subcategory edits to selected review rows.
 */
class BulkEditDialog final : public QDialog {
public:
    /**
     * @brief Creates the bulk edit dialog.
     * @param allow_subcategory True to show a subcategory input in addition to category.
     * @param parent Optional parent widget.
     */
    explicit BulkEditDialog(bool allow_subcategory, QWidget* parent = nullptr);

    /**
     * @brief Returns the trimmed category text entered by the user.
     * @return Category value, or an empty string when unchanged.
     */
    std::string category() const;

    /**
     * @brief Returns the trimmed subcategory text entered by the user.
     * @return Subcategory value, or an empty string when unchanged or unavailable.
     */
    std::string subcategory() const;

private:
    /**
     * @brief Enables the OK button only when at least one editable field has a value.
     */
    void update_ok_state();

    QLineEdit* category_edit_{nullptr};
    QLineEdit* subcategory_edit_{nullptr};
    QPushButton* ok_button_{nullptr};
    bool allow_subcategory_{false};
};

#endif // BULKEDITDIALOG_HPP
