#pragma once

#include <QCoreApplication>
#include <QDialog>
#include <cstddef>
#include <filesystem>

#include "FolderStructureTemplates.hpp"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/**
 * @brief Dialog for creating a built-in starter folder structure at a user-selected location.
 */
class FolderStructureInitializerDialog : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(FolderStructureInitializerDialog)

   public:
    /**
     * @brief Constructs the folder-structure initializer dialog.
     * @param start_directory Initial destination directory shown in the dialog.
     * @param parent Optional parent widget.
     */
    explicit FolderStructureInitializerDialog(const std::filesystem::path& start_directory = {},
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

    const FolderStructureTemplates::Descriptor* selected_template() const;

    QListWidget* template_list_{nullptr};
    QLabel* description_label_{nullptr};
    QLineEdit* destination_edit_{nullptr};
    QPushButton* browse_button_{nullptr};
    QListWidget* preview_list_{nullptr};
    QPushButton* create_button_{nullptr};
    std::size_t created_count_{0};
    std::size_t existing_count_{0};
};
