#ifndef USER_CATEGORIZATION_DIALOG_HPP
#define USER_CATEGORIZATION_DIALOG_HPP

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <string>

class UserCategorizationDialog : public QDialog {
public:
    UserCategorizationDialog(const std::string& file_name, 
                            const std::string& error_reason,
                            QWidget* parent = nullptr);
    
    std::string get_category() const;
    std::string get_subcategory() const;
    bool should_skip() const;

private:
    QLineEdit* category_input;
    QLineEdit* subcategory_input;
    QLabel* file_label;
    QLabel* reason_label;
    bool skip_file{false};
};

#endif // USER_CATEGORIZATION_DIALOG_HPP
