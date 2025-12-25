#ifndef FOLDER_LEARNING_DIALOG_HPP
#define FOLDER_LEARNING_DIALOG_HPP

#include <QDialog>
#include <string>

class QComboBox;
class QLabel;
class QPushButton;
class DatabaseManager;

class FolderLearningDialog : public QDialog {
    Q_OBJECT

public:
    explicit FolderLearningDialog(const std::string& folder_path,
                                  DatabaseManager& db_manager,
                                  QWidget* parent = nullptr);
    ~FolderLearningDialog() override = default;

    std::string get_selected_level() const;

private:
    void setup_ui();
    void load_current_setting();

    std::string folder_path_;
    DatabaseManager& db_manager_;
    
    QLabel* folder_label_;
    QLabel* explanation_label_;
    QComboBox* level_combo_;
    QPushButton* ok_button_;
    QPushButton* cancel_button_;
};

#endif // FOLDER_LEARNING_DIALOG_HPP
