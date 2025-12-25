#include "FolderLearningDialog.hpp"
#include "DatabaseManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFont>

FolderLearningDialog::FolderLearningDialog(const std::string& folder_path,
                                           DatabaseManager& db_manager,
                                           QWidget* parent)
    : QDialog(parent)
    , folder_path_(folder_path)
    , db_manager_(db_manager)
    , folder_label_(nullptr)
    , explanation_label_(nullptr)
    , level_combo_(nullptr)
    , ok_button_(nullptr)
    , cancel_button_(nullptr)
{
    setWindowTitle("Folder Learning Settings");
    setMinimumWidth(500);
    setup_ui();
    load_current_setting();
}

void FolderLearningDialog::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    
    // Folder path label
    folder_label_ = new QLabel(QString("Folder: <b>%1</b>").arg(QString::fromStdString(folder_path_)));
    folder_label_->setWordWrap(true);
    main_layout->addWidget(folder_label_);
    
    main_layout->addSpacing(10);
    
    // Explanation
    explanation_label_ = new QLabel(
        "Choose how this folder contributes to your user profile:\n\n"
        "• <b>Full Learning</b>: Analyze files to learn about your interests and work patterns\n"
        "• <b>Partial Learning</b>: Track folder statistics only, no personal characteristic inference\n"
        "• <b>No Learning</b>: Completely exclude this folder from the learning system"
    );
    explanation_label_->setWordWrap(true);
    main_layout->addWidget(explanation_label_);
    
    main_layout->addSpacing(10);
    
    // Combo box for level selection
    auto* combo_layout = new QHBoxLayout();
    auto* combo_label = new QLabel("Learning Level:");
    level_combo_ = new QComboBox();
    level_combo_->addItem("Full Learning", "full");
    level_combo_->addItem("Partial Learning", "partial");
    level_combo_->addItem("No Learning", "none");
    
    combo_layout->addWidget(combo_label);
    combo_layout->addWidget(level_combo_, 1);
    main_layout->addLayout(combo_layout);
    
    main_layout->addSpacing(20);
    
    // Buttons
    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    
    cancel_button_ = new QPushButton("Cancel");
    ok_button_ = new QPushButton("OK");
    ok_button_->setDefault(true);
    
    button_layout->addWidget(cancel_button_);
    button_layout->addWidget(ok_button_);
    
    main_layout->addLayout(button_layout);
    
    // Connect signals
    connect(ok_button_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
}

void FolderLearningDialog::load_current_setting() {
    std::string current_level = db_manager_.get_folder_inclusion_level(folder_path_);
    
    // Find and select the matching item
    for (int i = 0; i < level_combo_->count(); ++i) {
        if (level_combo_->itemData(i).toString().toStdString() == current_level) {
            level_combo_->setCurrentIndex(i);
            break;
        }
    }
}

std::string FolderLearningDialog::get_selected_level() const {
    return level_combo_->currentData().toString().toStdString();
}
