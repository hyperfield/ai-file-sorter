#include "FolderStructurePluginDialog.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QByteArray>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <cstddef>
#include <filesystem>
#include <utility>
#include <vector>

#include "FolderStructurePluginManager.hpp"
#include "Utils.hpp"

namespace {

std::string to_utf8(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString plugin_display_name(const FolderStructurePluginInstallError& error) {
    if (!error.plugin_name.empty()) {
        return QString::fromStdString(error.plugin_name);
    }
    if (!error.plugin_id.empty()) {
        return QString::fromStdString(error.plugin_id);
    }
    return FolderStructurePluginDialog::tr("this plugin");
}

}  // namespace

FolderStructurePluginDialog::FolderStructurePluginDialog(std::shared_ptr<FolderStructurePluginManager> plugin_manager,
                                                         QWidget* parent)
    : QDialog(parent), plugin_manager_(std::move(plugin_manager)) {
    setWindowTitle(tr("Manage Folder Structure Plugins"));
    resize(640, 380);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(tr("Install signed folder-structure plugins that add templates and routing guidance. "
                                "Checked plugins are loaded automatically."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    plugin_list_ = new QTreeWidget(this);
    plugin_list_->setColumnCount(4);
    plugin_list_->setHeaderLabels({tr("Enabled"), tr("Plugin"), tr("Version"), tr("Signer")});
    plugin_list_->setRootIsDecorated(false);
    plugin_list_->setUniformRowHeights(true);
    plugin_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    plugin_list_->setSelectionBehavior(QAbstractItemView::SelectRows);
    plugin_list_->setAllColumnsShowFocus(true);
    plugin_list_->header()->setStretchLastSection(false);
    plugin_list_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    plugin_list_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    plugin_list_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    plugin_list_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(plugin_list_, 1);

    description_label_ = new QLabel(this);
    description_label_->setWordWrap(true);
    description_label_->setMinimumHeight(72);
    layout->addWidget(description_label_);

    auto* button_row = new QDialogButtonBox(QDialogButtonBox::Close, this);
    import_button_ = button_row->addButton(tr("Install from File..."), QDialogButtonBox::ActionRole);
    uninstall_button_ = button_row->addButton(tr("Uninstall"), QDialogButtonBox::ActionRole);
    button_row->setCenterButtons(true);
    layout->addWidget(button_row);

    connect(button_row, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(plugin_list_, &QTreeWidget::currentItemChanged, this, [this]() { update_selection_state(); });
    connect(plugin_list_, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int column) { update_plugin_enabled_state(item, column); });
    connect(import_button_, &QPushButton::clicked, this, [this]() { import_plugin_archive(); });
    connect(uninstall_button_, &QPushButton::clicked, this, [this]() { uninstall_selected_plugin(); });

    populate_plugins();
    update_selection_state();
}

void FolderStructurePluginDialog::populate_plugins() {
    if (!plugin_manager_ || !plugin_list_) {
        return;
    }

    const std::string current_plugin_id = selected_plugin_id();
    const QSignalBlocker blocker(plugin_list_);
    plugin_list_->clear();

    const std::vector<FolderStructurePluginManifest> plugins = plugin_manager_->installed_plugins();
    for (const auto& plugin : plugins) {
        auto* item = new QTreeWidgetItem(plugin_list_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, plugin_manager_->is_enabled(plugin.id) ? Qt::Checked : Qt::Unchecked);
        item->setText(1, QString::fromStdString(plugin.name));
        item->setText(2, QString::fromStdString(plugin.version));
        item->setText(3, QString::fromStdString(plugin.verified_signer_key_id));
        item->setData(0, Qt::UserRole, QString::fromStdString(plugin.id));
        item->setData(1, Qt::UserRole, QString::fromStdString(plugin.id));
        item->setToolTip(1, QString::fromStdString(plugin.description));
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

void FolderStructurePluginDialog::update_selection_state() {
    if (!description_label_ || !import_button_ || !uninstall_button_) {
        return;
    }

    import_button_->setEnabled(plugin_manager_ != nullptr);
    auto* current = plugin_list_ ? plugin_list_->currentItem() : nullptr;
    uninstall_button_->setEnabled(plugin_manager_ != nullptr && current != nullptr);
    if (!current) {
        description_label_->setText(
            tr("No verified folder-structure plugins are installed. "
               "Use the Enabled checkbox to choose which installed plugins are loaded."));
        return;
    }

    const QString name = current->text(1);
    const QString version = current->text(2);
    const QString signer = current->text(3);
    const QString status =
        current->checkState(0) == Qt::Checked ? tr("Enabled (loaded automatically)") : tr("Disabled");
    const QString description = current->toolTip(1);
    description_label_->setText(
        tr("%1 %2\nVerified signer: %3\nStatus: %4\n\n%5").arg(name, version, signer, status, description));
}

void FolderStructurePluginDialog::update_plugin_enabled_state(QTreeWidgetItem* item, int column) {
    if (!plugin_manager_ || !item || column != 0) {
        return;
    }

    const std::string plugin_id = item->data(0, Qt::UserRole).toString().toStdString();
    if (plugin_id.empty()) {
        return;
    }

    const bool enabled = item->checkState(0) == Qt::Checked;
    std::string error;
    if (!plugin_manager_->set_enabled(plugin_id, enabled, &error)) {
        const QSignalBlocker blocker(plugin_list_);
        item->setCheckState(0, enabled ? Qt::Unchecked : Qt::Checked);
        QMessageBox::warning(
            this, tr("Update failed"),
            error.empty() ? tr("Failed to update folder-structure plugin state.") : QString::fromStdString(error));
    }
    update_selection_state();
}

void FolderStructurePluginDialog::import_plugin_archive() {
    const QString archive_path =
        QFileDialog::getOpenFileName(this, tr("Install Folder Structure Plugin"), QString(),
                                     tr("AI File Sorter plugins (*.aifsplugin *.zip);;All files (*)"));
    if (archive_path.isEmpty()) {
        return;
    }

    std::string installed_plugin_id;
    FolderStructurePluginInstallError install_error;
    const std::filesystem::path archive = Utils::utf8_to_path(to_utf8(archive_path));
    if (!plugin_manager_ || !plugin_manager_->install_from_archive(archive, &installed_plugin_id, &install_error)) {
        if (plugin_manager_ && install_error.missing_entitlement) {
            if (!activate_missing_entitlement(install_error)) {
                return;
            }
            install_error = FolderStructurePluginInstallError{};
            if (plugin_manager_->install_from_archive(archive, &installed_plugin_id, &install_error)) {
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
                return;
            }
        }
        QMessageBox::warning(this, tr("Install failed"),
                             install_error.message.empty() ? tr("Failed to install folder-structure plugin.")
                                                           : QString::fromStdString(install_error.message));
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

bool FolderStructurePluginDialog::activate_missing_entitlement(const FolderStructurePluginInstallError& install_error) {
    if (!plugin_manager_ || !install_error.missing_entitlement || install_error.product_id.empty()) {
        return false;
    }

    QMessageBox license_required(this);
    license_required.setIcon(QMessageBox::Warning);
    license_required.setWindowTitle(tr("License required"));
    license_required.setText(QString::fromStdString(install_error.message));
    license_required.setInformativeText(
        tr("Activate a license for %1, then AI File Sorter will retry the installation.")
            .arg(plugin_display_name(install_error)));
    auto* activate_button = license_required.addButton(tr("Activate..."), QMessageBox::AcceptRole);
    license_required.addButton(QMessageBox::Cancel);
    license_required.exec();
    if (license_required.clickedButton() != activate_button) {
        return false;
    }

    bool accepted = false;
    const QString license_key =
        QInputDialog::getText(this, tr("Activate Plugin License"),
                              tr("Paste the license key for %1:").arg(plugin_display_name(install_error)),
                              QLineEdit::Normal, QString(), &accepted);
    if (!accepted || license_key.trimmed().isEmpty()) {
        return false;
    }

    std::string activation_error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool activated =
        plugin_manager_->activate_license(install_error.product_id, to_utf8(license_key.trimmed()), &activation_error);
    QApplication::restoreOverrideCursor();

    if (!activated) {
        QMessageBox::warning(this, tr("Activation failed"),
                             activation_error.empty() ? tr("Failed to activate the plugin license.")
                                                      : QString::fromStdString(activation_error));
        return false;
    }
    return true;
}

void FolderStructurePluginDialog::uninstall_selected_plugin() {
    const std::string plugin_id = selected_plugin_id();
    if (plugin_id.empty()) {
        return;
    }

    std::string error;
    if (!plugin_manager_ || !plugin_manager_->uninstall(plugin_id, &error)) {
        QMessageBox::warning(
            this, tr("Uninstall failed"),
            error.empty() ? tr("Failed to uninstall folder-structure plugin.") : QString::fromStdString(error));
        return;
    }

    populate_plugins();
    update_selection_state();
}

std::string FolderStructurePluginDialog::selected_plugin_id() const {
    if (!plugin_list_ || !plugin_list_->currentItem()) {
        return {};
    }
    return plugin_list_->currentItem()->data(0, Qt::UserRole).toString().toStdString();
}
