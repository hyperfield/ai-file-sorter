#pragma once

#include <QDialog>

#include <atomic>
#include <memory>
#include <string>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

class StoragePluginManager;

/**
 * @brief Simple plugin installer dialog for optional storage-provider packages.
 */
class StoragePluginDialog : public QDialog {
public:
    /**
     * @brief Constructs the storage-plugin dialog.
     * @param plugin_manager Shared plugin manager used to query, install, update, and uninstall plugins.
     * @param parent Parent widget for the dialog.
     */
    explicit StoragePluginDialog(std::shared_ptr<StoragePluginManager> plugin_manager,
                                 QWidget* parent = nullptr);

private:
    /**
     * @brief Rebuilds the plugin list from the current manager state.
     */
    void populate_plugins();
    /**
     * @brief Updates button enabled state and the description panel for the current selection.
     */
    void update_selection_state();
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
     * @brief Updates the currently selected plugin from the configured source.
     * @param plugin_id Plugin identifier to update.
     */
    void update_selected_plugin(const std::string& plugin_id);
    /**
     * @brief Imports and installs a plugin archive chosen by the user.
     */
    void import_plugin_archive();
    /**
     * @brief Installs the currently selected plugin.
     */
    void install_selected_plugin();
    /**
     * @brief Uninstalls the currently selected plugin.
     */
    void uninstall_selected_plugin();
    /**
     * @brief Returns the id of the currently selected plugin.
     * @return Empty string when no plugin is selected.
     */
    std::string selected_plugin_id() const;

    std::shared_ptr<StoragePluginManager> plugin_manager_;
    QTreeWidget* plugin_list_{nullptr};
    QLabel* description_label_{nullptr};
    QPushButton* check_updates_button_{nullptr};
    QPushButton* import_button_{nullptr};
    QPushButton* install_button_{nullptr};
    QPushButton* uninstall_button_{nullptr};
    std::atomic<bool> refresh_in_progress_{false};
};
