#include "UserProfileDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QTabWidget>
#include <QHeaderView>
#include <QFont>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>

UserProfileDialog::UserProfileDialog(const UserProfile& profile, QWidget* parent)
    : QDialog(parent)
    , profile_(profile)
    , tab_widget_(nullptr)
    , user_id_label_(nullptr)
    , created_label_(nullptr)
    , last_updated_label_(nullptr)
    , summary_text_(nullptr)
    , characteristics_tree_(nullptr)
    , folder_insights_tree_(nullptr)
    , close_button_(nullptr)
{
    setWindowTitle("User Profile");
    setMinimumSize(800, 600);
    setup_ui();
}

void UserProfileDialog::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    
    // Create tab widget
    tab_widget_ = new QTabWidget(this);
    
    setup_overview_tab();
    setup_characteristics_tab();
    setup_folder_insights_tab();
    
    main_layout->addWidget(tab_widget_);
    
    // Close button
    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    
    close_button_ = new QPushButton("Close", this);
    connect(close_button_, &QPushButton::clicked, this, &QDialog::accept);
    button_layout->addWidget(close_button_);
    
    main_layout->addLayout(button_layout);
}

void UserProfileDialog::setup_overview_tab() {
    auto* overview_widget = new QWidget();
    auto* layout = new QVBoxLayout(overview_widget);
    
    // Profile info section
    auto* info_layout = new QVBoxLayout();
    
    user_id_label_ = new QLabel();
    user_id_label_->setText(QString("User ID: %1").arg(QString::fromStdString(profile_.user_id)));
    QFont bold_font = user_id_label_->font();
    bold_font.setBold(true);
    bold_font.setPointSize(bold_font.pointSize() + 2);
    user_id_label_->setFont(bold_font);
    info_layout->addWidget(user_id_label_);
    
    created_label_ = new QLabel();
    created_label_->setText(QString("Profile Created: %1").arg(QString::fromStdString(profile_.created_at)));
    info_layout->addWidget(created_label_);
    
    last_updated_label_ = new QLabel();
    last_updated_label_->setText(QString("Last Updated: %1").arg(QString::fromStdString(profile_.last_updated)));
    info_layout->addWidget(last_updated_label_);
    
    layout->addLayout(info_layout);
    
    // Summary section
    auto* summary_label = new QLabel("Profile Summary:");
    summary_label->setFont(bold_font);
    layout->addWidget(summary_label);
    
    summary_text_ = new QTextEdit();
    summary_text_->setReadOnly(true);
    
    // Generate summary
    std::stringstream summary;
    summary << "This profile has been built by analyzing " << profile_.folder_insights.size() 
            << " folders and tracking " << profile_.characteristics.size() << " characteristics.\n\n";
    
    // Group characteristics by trait name
    std::unordered_map<std::string, std::vector<const UserCharacteristic*>> grouped_traits;
    for (const auto& characteristic : profile_.characteristics) {
        grouped_traits[characteristic.trait_name].push_back(&characteristic);
    }
    
    // Display hobbies
    if (grouped_traits.count("hobby") > 0) {
        summary << "**Interests & Hobbies:**\n";
        auto& hobbies = grouped_traits["hobby"];
        std::sort(hobbies.begin(), hobbies.end(), 
                  [](const auto* a, const auto* b) { return a->confidence > b->confidence; });
        for (const auto* hobby : hobbies) {
            int confidence_pct = static_cast<int>(hobby->confidence * 100);
            summary << "  • " << hobby->value << " (confidence: " << confidence_pct << "%)\n";
        }
        summary << "\n";
    }
    
    // Display work patterns
    if (grouped_traits.count("work_pattern") > 0) {
        summary << "**Work Patterns:**\n";
        for (const auto* trait : grouped_traits["work_pattern"]) {
            int confidence_pct = static_cast<int>(trait->confidence * 100);
            summary << "  • " << trait->value << " (confidence: " << confidence_pct << "%)\n";
        }
        summary << "\n";
    }
    
    // Display organization style
    if (grouped_traits.count("organization_style") > 0) {
        summary << "**Organization Style:**\n";
        for (const auto* trait : grouped_traits["organization_style"]) {
            int confidence_pct = static_cast<int>(trait->confidence * 100);
            summary << "  • " << trait->value << " (confidence: " << confidence_pct << "%)\n";
        }
        summary << "\n";
    }
    
    // Display folder stats
    if (!profile_.folder_insights.empty()) {
        summary << "**Folder Analysis:**\n";
        int total_files = 0;
        for (const auto& insight : profile_.folder_insights) {
            total_files += insight.file_count;
        }
        summary << "  • Total files analyzed: " << total_files << "\n";
        summary << "  • Folders tracked: " << profile_.folder_insights.size() << "\n";
    }
    
    summary_text_->setText(QString::fromStdString(summary.str()));
    layout->addWidget(summary_text_);
    
    tab_widget_->addTab(overview_widget, "Overview");
}

void UserProfileDialog::setup_characteristics_tab() {
    auto* characteristics_widget = new QWidget();
    auto* layout = new QVBoxLayout(characteristics_widget);
    
    auto* label = new QLabel("All Characteristics (grouped by trait):");
    layout->addWidget(label);
    
    characteristics_tree_ = new QTreeWidget();
    characteristics_tree_->setHeaderLabels({"Trait / Value", "Confidence", "Evidence", "Updated"});
    characteristics_tree_->setAlternatingRowColors(true);
    characteristics_tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    characteristics_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    characteristics_tree_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    characteristics_tree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    
    populate_characteristics();
    
    layout->addWidget(characteristics_tree_);
    tab_widget_->addTab(characteristics_widget, "Characteristics");
}

void UserProfileDialog::setup_folder_insights_tab() {
    auto* insights_widget = new QWidget();
    auto* layout = new QVBoxLayout(insights_widget);
    
    auto* label = new QLabel("Analyzed Folders:");
    layout->addWidget(label);
    
    folder_insights_tree_ = new QTreeWidget();
    folder_insights_tree_->setHeaderLabels({"Folder Path", "Files", "Pattern", "Last Analyzed"});
    folder_insights_tree_->setAlternatingRowColors(true);
    folder_insights_tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    folder_insights_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    folder_insights_tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    folder_insights_tree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    
    populate_folder_insights();
    
    layout->addWidget(folder_insights_tree_);
    tab_widget_->addTab(insights_widget, "Folder Insights");
}

void UserProfileDialog::populate_characteristics() {
    // Group characteristics by trait name
    std::unordered_map<std::string, std::vector<UserCharacteristic>> grouped_traits;
    for (const auto& characteristic : profile_.characteristics) {
        grouped_traits[characteristic.trait_name].push_back(characteristic);
    }
    
    // Create tree structure
    for (auto& [trait_name, characteristics] : grouped_traits) {
        // Sort by confidence (descending)
        std::sort(characteristics.begin(), characteristics.end(),
                  [](const UserCharacteristic& a, const UserCharacteristic& b) {
                      return a.confidence > b.confidence;
                  });
        
        auto* trait_item = new QTreeWidgetItem(characteristics_tree_);
        trait_item->setText(0, QString::fromStdString(trait_name));
        trait_item->setExpanded(true);
        
        QFont bold_font = trait_item->font(0);
        bold_font.setBold(true);
        trait_item->setFont(0, bold_font);
        
        for (const auto& characteristic : characteristics) {
            add_characteristic_item(trait_item, 
                                  trait_name,
                                  characteristic.value,
                                  characteristic.confidence);
            
            auto* value_item = trait_item->child(trait_item->childCount() - 1);
            value_item->setText(2, QString::fromStdString(characteristic.evidence));
            value_item->setText(3, QString::fromStdString(characteristic.timestamp));
        }
    }
}

void UserProfileDialog::add_characteristic_item(QTreeWidgetItem* parent,
                                               const std::string& trait_name,
                                               const std::string& value,
                                               float confidence) {
    auto* item = new QTreeWidgetItem(parent);
    item->setText(0, QString::fromStdString(value));
    
    int confidence_pct = static_cast<int>(confidence * 100);
    item->setText(1, QString("%1%").arg(confidence_pct));
    
    // Color code by confidence
    QColor color;
    if (confidence >= 0.8f) {
        color = QColor(0, 200, 0);  // Green
    } else if (confidence >= 0.5f) {
        color = QColor(200, 200, 0);  // Yellow
    } else {
        color = QColor(200, 0, 0);  // Red
    }
    item->setForeground(1, color);
}

void UserProfileDialog::populate_folder_insights() {
    for (const auto& insight : profile_.folder_insights) {
        auto* item = new QTreeWidgetItem(folder_insights_tree_);
        item->setText(0, QString::fromStdString(insight.folder_path));
        item->setText(1, QString::number(insight.file_count));
        item->setText(2, QString::fromStdString(insight.usage_pattern));
        item->setText(3, QString::fromStdString(insight.last_analyzed));
        
        // Add details as tooltip
        QString tooltip = QString("Description: %1\n\nDominant Categories: %2")
            .arg(QString::fromStdString(insight.description))
            .arg(QString::fromStdString(insight.dominant_categories));
        item->setToolTip(0, tooltip);
    }
}
