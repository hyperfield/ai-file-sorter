#pragma once

#include <QCoreApplication>
#include <QDialog>
#include <atomic>
#include <memory>
#include <string>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class FolderStructurePluginManager;

/**
 * @brief Dialog for installing and removing signed folder-structure plugins.
 */
class FolderStructurePluginDialog : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(FolderStructurePluginDialog)

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
     * @brief Persists an enablement checkbox change for an installed plugin.
     * @param item Changed plugin row.
     * @param column Changed column index.
     */
    void update_plugin_enabled_state(QTreeWidgetItem* item, int column);
    /**
     * @brief Starts a background remote-catalog refresh.
     * @param interactive True when the refresh was triggered explicitly by the user.
     */
    void start_catalog_refresh(bool interactive);
    /**
     * @brief Applies the result of a completed background refresh on the UI thread.
     * @param success True when the refresh succeeded.
     * @param error Optional user-facing error description from the refresh attempt.
     * @param interactive True when the refresh was triggered explicitly by the user.
     */
    void finish_catalog_refresh(bool success, const std::string& error, bool interactive);
    /**
     * @brief Imports and installs a signed plugin archive chosen by the user.
     */
    void import_plugin_archive();
    /**
     * @brief Installs or updates the currently selected remote-catalog plugin.
     */
    void install_selected_plugin();
    /**
     * @brief Activates the commercial license for the currently selected catalog plugin.
     */
    void activate_selected_plugin_license();
    /**
     * @brief Opens the purchase/info page for the currently selected catalog plugin.
     */
    void open_selected_plugin_page();
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
    QLabel* details_label_{nullptr};
    QLabel* description_label_{nullptr};
    QPushButton* check_updates_button_{nullptr};
    QPushButton* import_button_{nullptr};
    QPushButton* install_button_{nullptr};
    QPushButton* activate_button_{nullptr};
    QPushButton* obtain_button_{nullptr};
    QPushButton* uninstall_button_{nullptr};
    std::atomic<bool> refresh_in_progress_{false};
};
