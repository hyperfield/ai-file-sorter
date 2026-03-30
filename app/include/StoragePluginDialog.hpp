#pragma once

#include <QDialog>

class QListWidget;
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
    void install_selected_plugin();
    void uninstall_selected_plugin();

    StoragePluginManager& plugin_manager_;
    QListWidget* plugin_list_{nullptr};
    QLabel* description_label_{nullptr};
    QPushButton* install_button_{nullptr};
    QPushButton* uninstall_button_{nullptr};
};
