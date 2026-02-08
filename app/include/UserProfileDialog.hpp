#ifndef USER_PROFILE_DIALOG_HPP
#define USER_PROFILE_DIALOG_HPP

#include "Types.hpp"
#include <QDialog>
#include <memory>

class QLabel;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QTabWidget;

class UserProfileDialog : public QDialog {
public:
    explicit UserProfileDialog(const UserProfile& profile, QWidget* parent = nullptr);
    ~UserProfileDialog() override = default;

private:
    void setup_ui();
    void setup_overview_tab();
    void setup_characteristics_tab();
    void setup_folder_insights_tab();
    void setup_templates_tab();
    void populate_characteristics();
    void populate_folder_insights();
    void populate_templates();
    
    void add_characteristic_item(QTreeWidgetItem* parent,
                                const std::string& trait_name,
                                const std::string& value,
                                float confidence);
    
    UserProfile profile_;
    
    // UI elements
    QTabWidget* tab_widget_;
    
    // Overview tab
    QLabel* user_id_label_;
    QLabel* created_label_;
    QLabel* last_updated_label_;
    QTextEdit* summary_text_;
    
    // Characteristics tab
    QTreeWidget* characteristics_tree_;
    
    // Folder insights tab
    QTreeWidget* folder_insights_tree_;
    
    // Templates tab
    QTreeWidget* templates_tree_;
    
    QPushButton* close_button_;
};

#endif // USER_PROFILE_DIALOG_HPP
