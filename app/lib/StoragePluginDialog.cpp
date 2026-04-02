#include "StoragePluginDialog.hpp"

#include "StoragePluginManager.hpp"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

QString dialog_tr(const char* source)
{
    return QCoreApplication::translate("StoragePluginDialog", source);
}

} // namespace

StoragePluginDialog::StoragePluginDialog(StoragePluginManager& plugin_manager, QWidget* parent)
    : QDialog(parent),
      plugin_manager_(plugin_manager)
{
    setWindowTitle(dialog_tr("Manage Storage Plugins"));
    resize(620, 380);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        dialog_tr("Install optional storage plugins to enable compatibility support for cloud-backed folders."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    plugin_list_ = new QTreeWidget(this);
    plugin_list_->setColumnCount(2);
    plugin_list_->setHeaderLabels({dialog_tr("Plugin"), dialog_tr("Action")});
    plugin_list_->setRootIsDecorated(false);
    plugin_list_->setUniformRowHeights(true);
    plugin_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    plugin_list_->header()->setStretchLastSection(false);
    plugin_list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    plugin_list_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(plugin_list_, 1);

    description_label_ = new QLabel(this);
    description_label_->setWordWrap(true);
    description_label_->setMinimumHeight(72);
    layout->addWidget(description_label_);

    auto* button_row = new QDialogButtonBox(QDialogButtonBox::Close, this);
    check_updates_button_ = button_row->addButton(dialog_tr("Check for updates"), QDialogButtonBox::ActionRole);
    import_button_ = button_row->addButton(dialog_tr("Install from File…"), QDialogButtonBox::ActionRole);
    install_button_ = button_row->addButton(dialog_tr("Install"), QDialogButtonBox::ActionRole);
    uninstall_button_ = button_row->addButton(dialog_tr("Uninstall"), QDialogButtonBox::ActionRole);
    button_row->setCenterButtons(true);
    layout->addWidget(button_row);

    connect(button_row, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(plugin_list_, &QTreeWidget::currentItemChanged, this, [this]() {
        update_selection_state();
    });
    connect(check_updates_button_, &QPushButton::clicked, this, [this]() {
        refresh_catalog(true);
    });
    connect(import_button_, &QPushButton::clicked, this, [this]() {
        import_plugin_archive();
    });
    connect(install_button_, &QPushButton::clicked, this, [this]() {
        install_selected_plugin();
    });
    connect(uninstall_button_, &QPushButton::clicked, this, [this]() {
        uninstall_selected_plugin();
    });

    populate_plugins();
    update_selection_state();
    refresh_catalog(false);
}

void StoragePluginDialog::populate_plugins()
{
    if (!plugin_list_) {
        return;
    }

    const std::string current_plugin_id = selected_plugin_id();
    plugin_list_->clear();
    for (const auto& plugin : plugin_manager_.available_plugins()) {
        const bool installed = plugin_manager_.is_installed(plugin.id);
        const bool supported = plugin_manager_.supports_plugin(plugin.id);
        const bool can_update = plugin_manager_.can_update(plugin.id);
        QString label = QString::fromStdString(plugin.name);
        if (installed) {
            label += dialog_tr(" (Installed)");
        }

        auto* item = new QTreeWidgetItem(plugin_list_);
        item->setText(0, label);
        item->setData(0, Qt::UserRole, QString::fromStdString(plugin.id));

        if (installed && supported && can_update) {
            auto* button = new QPushButton(dialog_tr("Update"), plugin_list_);
            connect(button, &QPushButton::clicked, this, [this, item, plugin_id = plugin.id]() {
                if (plugin_list_) {
                    plugin_list_->setCurrentItem(item);
                }
                update_selected_plugin(plugin_id);
            });

            auto* button_host = new QWidget(plugin_list_);
            auto* row_layout = new QHBoxLayout(button_host);
            row_layout->setContentsMargins(0, 0, 0, 0);
            row_layout->addStretch(1);
            row_layout->addWidget(button);
            row_layout->addStretch(1);
            plugin_list_->setItemWidget(item, 1, button_host);
        }
    }

    if (!current_plugin_id.empty()) {
        for (int row = 0; row < plugin_list_->topLevelItemCount(); ++row) {
            auto* item = plugin_list_->topLevelItem(row);
            if (item && item->data(0, Qt::UserRole).toString().toStdString() == current_plugin_id) {
                plugin_list_->setCurrentItem(item);
                break;
            }
        }
    }

    if (plugin_list_->topLevelItemCount() > 0 && !plugin_list_->currentItem()) {
        plugin_list_->setCurrentItem(plugin_list_->topLevelItem(0));
    }
}

void StoragePluginDialog::update_selection_state()
{
    if (!plugin_list_ || !description_label_ || !check_updates_button_ || !install_button_ ||
        !uninstall_button_) {
        return;
    }

    check_updates_button_->setEnabled(plugin_manager_.can_check_for_updates());

    auto* current = plugin_list_->currentItem();
    if (!current) {
        description_label_->clear();
        install_button_->setEnabled(false);
        uninstall_button_->setEnabled(false);
        return;
    }

    const std::string plugin_id = current->data(0, Qt::UserRole).toString().toStdString();
    const auto plugin = plugin_manager_.find_plugin(plugin_id);
    if (!plugin.has_value()) {
        description_label_->setText(dialog_tr("Unknown plugin."));
        install_button_->setEnabled(false);
        uninstall_button_->setEnabled(false);
        return;
    }

    QString description = QString::fromStdString(plugin->description);
    const bool installed = plugin_manager_.is_installed(plugin_id);
    const bool supported = plugin_manager_.supports_plugin(plugin_id);
    const bool can_update = plugin_manager_.can_update(plugin_id);
    if (!supported) {
        description +=
            QStringLiteral("\n\n") +
            dialog_tr("This plugin entry point is not supported by this version of the app.");
    } else if (installed && can_update) {
        description +=
            QStringLiteral("\n\n") +
            dialog_tr("A newer or refreshed package can be installed from the configured plugin source.");
    }
    description_label_->setText(description);

    install_button_->setEnabled(!installed && supported);
    uninstall_button_->setEnabled(installed);
}

void StoragePluginDialog::refresh_catalog(bool interactive)
{
    std::string error;
    if (!plugin_manager_.refresh_remote_catalog(&error)) {
        if (interactive) {
            QMessageBox::warning(this,
                                 dialog_tr("Update check failed"),
                                 error.empty()
                                     ? dialog_tr("Failed to check for storage plugin updates.")
                                     : QString::fromStdString(error));
        }
        return;
    }

    populate_plugins();
    update_selection_state();
}

void StoragePluginDialog::import_plugin_archive()
{
    const QString archive_path = QFileDialog::getOpenFileName(
        this,
        dialog_tr("Install Storage Plugin"),
        QString(),
        dialog_tr("AI File Sorter plugins (*.aifsplugin *.zip);;All files (*)"));
    if (archive_path.isEmpty()) {
        return;
    }

    std::string installed_plugin_id;
    std::string error;
    if (!plugin_manager_.install_from_archive(archive_path.toStdString(), &installed_plugin_id, &error)) {
        QMessageBox::warning(this,
                             dialog_tr("Install failed"),
                             error.empty()
                                 ? dialog_tr("Failed to install plugin archive.")
                                 : QString::fromStdString(error));
        return;
    }

    populate_plugins();
    if (!installed_plugin_id.empty() && plugin_list_) {
        for (int row = 0; row < plugin_list_->topLevelItemCount(); ++row) {
            auto* item = plugin_list_->topLevelItem(row);
            if (item && item->data(0, Qt::UserRole).toString().toStdString() == installed_plugin_id) {
                plugin_list_->setCurrentItem(item);
                break;
            }
        }
    }
    update_selection_state();
}

void StoragePluginDialog::install_selected_plugin()
{
    const std::string plugin_id = selected_plugin_id();
    if (plugin_id.empty()) {
        return;
    }

    std::string error;
    if (!plugin_manager_.install(plugin_id, &error)) {
        QMessageBox::warning(this,
                             dialog_tr("Install failed"),
                             error.empty() ? dialog_tr("Failed to install plugin.") : QString::fromStdString(error));
        return;
    }

    populate_plugins();
    update_selection_state();
}

void StoragePluginDialog::update_selected_plugin(const std::string& plugin_id)
{
    if (plugin_id.empty()) {
        return;
    }

    std::string error;
    if (!plugin_manager_.update(plugin_id, &error)) {
        QMessageBox::warning(this,
                             dialog_tr("Update failed"),
                             error.empty() ? dialog_tr("Failed to update plugin.") : QString::fromStdString(error));
        return;
    }

    populate_plugins();
    update_selection_state();
}

void StoragePluginDialog::uninstall_selected_plugin()
{
    const std::string plugin_id = selected_plugin_id();
    if (plugin_id.empty()) {
        return;
    }

    std::string error;
    if (!plugin_manager_.uninstall(plugin_id, &error)) {
        QMessageBox::warning(this,
                             dialog_tr("Uninstall failed"),
                             error.empty() ? dialog_tr("Failed to uninstall plugin.") : QString::fromStdString(error));
        return;
    }

    populate_plugins();
    update_selection_state();
}

std::string StoragePluginDialog::selected_plugin_id() const
{
    if (!plugin_list_ || !plugin_list_->currentItem()) {
        return {};
    }
    return plugin_list_->currentItem()->data(0, Qt::UserRole).toString().toStdString();
}
