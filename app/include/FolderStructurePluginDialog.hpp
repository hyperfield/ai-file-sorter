#pragma once

#include <QDialog>

#include <memory>
#include <string>

class QLabel;
class QPushButton;
class QTreeWidget;

class FolderStructurePluginManager;

/**
 * @brief Dialog for installing and removing signed folder-structure plugins.
 */
class FolderStructurePluginDialog : public QDialog {
public:
    /**
     * @brief Constructs the folder-structure plugin management dialog.
     * @param plugin_manager Shared manager used to query, install, and uninstall plugins.
     * @param parent Optional parent widget.
     */
    explicit FolderStructurePluginDialog(std::shared_ptr<FolderStructurePluginManager> plugin_manager,
                                         QWidget* parent = nullptr);

private:
    /**
     * @brief Rebuilds the installed plugin list from verified package data.
     */
    void populate_plugins();
    /**
     * @brief Updates details and button enabled state for the current selection.
     */
    void update_selection_state();
    /**
     * @brief Imports and installs a signed plugin archive chosen by the user.
     */
    void import_plugin_archive();
    /**
     * @brief Uninstalls the currently selected folder-structure plugin.
     */
    void uninstall_selected_plugin();
    /**
     * @brief Returns the id of the currently selected plugin.
     * @return Empty string when no plugin is selected.
     */
    std::string selected_plugin_id() const;

    std::shared_ptr<FolderStructurePluginManager> plugin_manager_;
    QTreeWidget* plugin_list_{nullptr};
    QLabel* description_label_{nullptr};
    QPushButton* import_button_{nullptr};
    QPushButton* uninstall_button_{nullptr};
};
