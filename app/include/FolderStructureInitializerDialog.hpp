#pragma once

#include <QCoreApplication>
#include <QDialog>
#include <cstddef>
#include <filesystem>
#include <vector>

#include "FolderStructurePluginProfile.hpp"
#include "FolderStructureTemplates.hpp"

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QWidget;

/**
 * @brief Dialog for creating starter folder structures at a user-selected location.
 */
class FolderStructureInitializerDialog : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(FolderStructureInitializerDialog)

   public:
    /**
     * @brief Constructs the folder-structure initializer dialog.
     * @param start_directory Initial destination directory shown in the dialog.
     * @param plugin_profiles Verified plugin profiles that can provide extra templates.
     * @param parent Optional parent widget.
     */
    explicit FolderStructureInitializerDialog(const std::filesystem::path& start_directory = {},
                                              const std::vector<FolderStructurePluginProfile>& plugin_profiles = {},
                                              QWidget* parent = nullptr);

    /**
     * @brief Return the destination root entered by the user.
     * @return Destination root as a filesystem path.
     */
    std::filesystem::path destination_root() const;

    /**
     * @brief Return how many template folders were created by the accepted run.
     * @return Count of newly created folders.
     */
    std::size_t created_count() const { return created_count_; }

    /**
     * @brief Return how many template folders already existed during the accepted run.
     * @return Count of already-existing folders.
     */
    std::size_t existing_count() const { return existing_count_; }

   private:
    void populate_templates();
    void update_selection();
    void browse_destination();
    void create_selected_structure();
    void create_next_johnny_decimal_folder();
    void refresh_validation_report();
    bool next_folder_tab_active() const;
    bool validation_tab_active() const;

    const FolderStructureTemplates::Descriptor* selected_template() const;

    QTabWidget* tab_widget_{nullptr};
    QWidget* starter_tab_{nullptr};
    QWidget* next_folder_tab_{nullptr};
    QWidget* validation_tab_{nullptr};
    QListWidget* template_list_{nullptr};
    QLabel* description_label_{nullptr};
    QLineEdit* destination_edit_{nullptr};
    QPushButton* browse_button_{nullptr};
    QListWidget* preview_list_{nullptr};
    QLineEdit* next_area_edit_{nullptr};
    QLineEdit* next_folder_edit_{nullptr};
    QLineEdit* next_preview_edit_{nullptr};
    QLabel* next_preview_status_label_{nullptr};
    QPlainTextEdit* validation_report_edit_{nullptr};
    QPushButton* create_button_{nullptr};
    std::vector<FolderStructurePluginProfile> plugin_profiles_;
    std::vector<FolderStructureTemplates::Descriptor> descriptors_;
    std::size_t created_count_{0};
    std::size_t existing_count_{0};
};
