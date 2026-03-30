#include "StoragePluginDialog.hpp"

#include "StoragePluginManager.hpp"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
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
    resize(520, 360);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        dialog_tr("Install optional storage plugins to enable compatibility support for cloud-backed folders."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    plugin_list_ = new QListWidget(this);
    layout->addWidget(plugin_list_, 1);

    description_label_ = new QLabel(this);
    description_label_->setWordWrap(true);
    description_label_->setMinimumHeight(72);
    layout->addWidget(description_label_);

    auto* button_row = new QDialogButtonBox(QDialogButtonBox::Close, this);
    install_button_ = button_row->addButton(dialog_tr("Install"), QDialogButtonBox::ActionRole);
    uninstall_button_ = button_row->addButton(dialog_tr("Uninstall"), QDialogButtonBox::ActionRole);
    layout->addWidget(button_row);

    connect(button_row, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(plugin_list_, &QListWidget::currentRowChanged, this, [this]() {
        update_selection_state();
    });
    connect(install_button_, &QPushButton::clicked, this, [this]() {
        install_selected_plugin();
    });
    connect(uninstall_button_, &QPushButton::clicked, this, [this]() {
        uninstall_selected_plugin();
    });

    populate_plugins();
    update_selection_state();
}

void StoragePluginDialog::populate_plugins()
{
    if (!plugin_list_) {
        return;
    }

    plugin_list_->clear();
    for (const auto& plugin : plugin_manager_.available_plugins()) {
        const bool installed = plugin_manager_.is_installed(plugin.id);
        auto* item = new QListWidgetItem(
            QString::fromStdString(plugin.name) +
                (installed ? dialog_tr(" (Installed)") : QString()),
            plugin_list_);
        item->setData(Qt::UserRole, QString::fromStdString(plugin.id));
    }

    if (plugin_list_->count() > 0 && plugin_list_->currentRow() < 0) {
        plugin_list_->setCurrentRow(0);
    }
}

void StoragePluginDialog::update_selection_state()
{
    if (!plugin_list_ || !description_label_ || !install_button_ || !uninstall_button_) {
        return;
    }

    auto* current = plugin_list_->currentItem();
    if (!current) {
        description_label_->clear();
        install_button_->setEnabled(false);
        uninstall_button_->setEnabled(false);
        return;
    }

    const std::string plugin_id = current->data(Qt::UserRole).toString().toStdString();
    const auto plugin = plugin_manager_.find_plugin(plugin_id);
    if (!plugin.has_value()) {
        description_label_->setText(dialog_tr("Unknown plugin."));
        install_button_->setEnabled(false);
        uninstall_button_->setEnabled(false);
        return;
    }

    description_label_->setText(QString::fromStdString(plugin->description));
    const bool installed = plugin_manager_.is_installed(plugin_id);
    install_button_->setEnabled(!installed);
    uninstall_button_->setEnabled(installed);
}

void StoragePluginDialog::install_selected_plugin()
{
    if (!plugin_list_ || !plugin_list_->currentItem()) {
        return;
    }

    const std::string plugin_id =
        plugin_list_->currentItem()->data(Qt::UserRole).toString().toStdString();
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

void StoragePluginDialog::uninstall_selected_plugin()
{
    if (!plugin_list_ || !plugin_list_->currentItem()) {
        return;
    }

    const std::string plugin_id =
        plugin_list_->currentItem()->data(Qt::UserRole).toString().toStdString();
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
