#pragma once

#include <QDialog>

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
    explicit StoragePluginDialog(StoragePluginManager& plugin_manager, QWidget* parent = nullptr);

private:
    void populate_plugins();
    void update_selection_state();
    void refresh_catalog(bool interactive);
    void update_selected_plugin(const std::string& plugin_id);
    void import_plugin_archive();
    void install_selected_plugin();
    void uninstall_selected_plugin();
    std::string selected_plugin_id() const;

    StoragePluginManager& plugin_manager_;
    QTreeWidget* plugin_list_{nullptr};
    QLabel* description_label_{nullptr};
    QPushButton* check_updates_button_{nullptr};
    QPushButton* import_button_{nullptr};
    QPushButton* install_button_{nullptr};
    QPushButton* uninstall_button_{nullptr};
};
