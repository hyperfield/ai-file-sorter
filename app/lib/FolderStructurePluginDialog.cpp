#include "FolderStructurePluginDialog.hpp"

#include <QAbstractItemView>
#include <QByteArray>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "FolderStructurePluginInstallFlow.hpp"
#include "FolderStructurePluginManager.hpp"
#include "PluginBuildConfig.hpp"
#include "Utils.hpp"

namespace {

constexpr int kPluginIdRole = Qt::UserRole;
constexpr int kInstalledRole = Qt::UserRole + 1;
constexpr int kPurchaseUrlRole = Qt::UserRole + 2;

std::string to_utf8(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::unordered_map<std::string, FolderStructurePluginManifest> installed_manifest_map(
    const std::vector<FolderStructurePluginManifest>& manifests) {
    std::unordered_map<std::string, FolderStructurePluginManifest> by_id;
    by_id.reserve(manifests.size());
    for (const auto& manifest : manifests) {
        by_id[manifest.id] = manifest;
    }
    return by_id;
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
    check_updates_button_ = button_row->addButton(tr("Check for Updates"), QDialogButtonBox::ActionRole);
    import_button_ = button_row->addButton(tr("Install from File..."), QDialogButtonBox::ActionRole);
    install_button_ = button_row->addButton(tr("Install"), QDialogButtonBox::ActionRole);
    obtain_button_ = button_row->addButton(tr("Obtain..."), QDialogButtonBox::ActionRole);
    uninstall_button_ = button_row->addButton(tr("Uninstall"), QDialogButtonBox::ActionRole);
    check_updates_button_->setVisible(PluginBuildConfig::remote_catalogs_enabled());
    install_button_->setVisible(PluginBuildConfig::remote_catalogs_enabled());
    obtain_button_->setVisible(PluginBuildConfig::commercial_plugin_actions_enabled());
    button_row->setCenterButtons(true);
    layout->addWidget(button_row);

    connect(button_row, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(plugin_list_, &QTreeWidget::currentItemChanged, this, [this]() { update_selection_state(); });
    connect(plugin_list_, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int column) { update_plugin_enabled_state(item, column); });
    connect(check_updates_button_, &QPushButton::clicked, this, [this]() { start_catalog_refresh(true); });
    connect(import_button_, &QPushButton::clicked, this, [this]() { import_plugin_archive(); });
    connect(install_button_, &QPushButton::clicked, this, [this]() { install_selected_plugin(); });
    connect(obtain_button_, &QPushButton::clicked, this, [this]() { open_selected_plugin_page(); });
    connect(uninstall_button_, &QPushButton::clicked, this, [this]() { uninstall_selected_plugin(); });

    populate_plugins();
    update_selection_state();
    if (PluginBuildConfig::remote_catalogs_enabled()) {
        start_catalog_refresh(false);
    }
}

void FolderStructurePluginDialog::populate_plugins() {
    if (!plugin_manager_ || !plugin_list_) {
        return;
    }

    const std::string current_plugin_id = selected_plugin_id();
    const QSignalBlocker blocker(plugin_list_);
    plugin_list_->clear();

    const auto installed_by_id = installed_manifest_map(plugin_manager_->installed_plugins());
    const std::vector<FolderStructurePluginCatalogEntry> plugins = plugin_manager_->available_plugins();
    for (const auto& plugin : plugins) {
        const auto installed = installed_by_id.find(plugin.id);
        const bool is_installed = installed != installed_by_id.end();

        auto* item = new QTreeWidgetItem(plugin_list_);
        if (is_installed) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, plugin_manager_->is_enabled(plugin.id) ? Qt::Checked : Qt::Unchecked);
        } else {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setText(0, tr("Available"));
        }
        item->setText(1, QString::fromStdString(plugin.name));
        item->setText(2, QString::fromStdString(plugin.version));
        item->setText(3,
                      is_installed ? QString::fromStdString(installed->second.verified_signer_key_id) : tr("Catalog"));
        item->setData(0, kPluginIdRole, QString::fromStdString(plugin.id));
        item->setData(1, kPluginIdRole, QString::fromStdString(plugin.id));
        item->setData(0, kInstalledRole, is_installed);
        item->setData(0, kPurchaseUrlRole, QString::fromStdString(plugin.purchase_url));
        item->setToolTip(1, QString::fromStdString(plugin.description));
    }

    if (!current_plugin_id.empty()) {
        for (int row = 0; row < plugin_list_->topLevelItemCount(); ++row) {
            auto* item = plugin_list_->topLevelItem(row);
            if (item && item->data(0, kPluginIdRole).toString().toStdString() == current_plugin_id) {
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
    if (!description_label_ || !check_updates_button_ || !import_button_ || !install_button_ || !obtain_button_ ||
        !uninstall_button_) {
        return;
    }

    const bool refresh_active = refresh_in_progress_.load();
    if (plugin_list_) {
        plugin_list_->setEnabled(!refresh_active);
    }
    check_updates_button_->setEnabled(plugin_manager_ != nullptr && !refresh_active &&
                                      plugin_manager_->remote_catalog_configured());
    import_button_->setEnabled(plugin_manager_ != nullptr && !refresh_active);
    install_button_->setEnabled(false);
    obtain_button_->setEnabled(false);
    uninstall_button_->setEnabled(false);

    auto* current = plugin_list_ ? plugin_list_->currentItem() : nullptr;
    if (!current) {
        description_label_->setText(
            tr("No verified folder-structure plugins are installed or listed. Check for updates or install a signed "
               "plugin package from file."));
        return;
    }

    const std::string plugin_id = current->data(0, kPluginIdRole).toString().toStdString();
    const auto plugin = plugin_manager_ ? plugin_manager_->find_plugin(plugin_id) : std::nullopt;
    if (!plugin.has_value()) {
        description_label_->setText(tr("Unknown folder-structure plugin."));
        return;
    }

    const bool installed = plugin_manager_ && plugin_manager_->is_installed(plugin_id);
    const bool can_update = plugin_manager_ && plugin_manager_->can_update(plugin_id);
    const QString purchase_url = current->data(0, kPurchaseUrlRole).toString();

    QString status =
        installed ? (current->checkState(0) == Qt::Checked ? tr("Enabled (loaded automatically)") : tr("Disabled"))
                  : tr("Available from catalog");
    if (plugin->license_required) {
        status += tr("; license required");
    }

    QString description = QString::fromStdString(plugin->description);
    if (!installed && !plugin->has_remote_package()) {
        description += QStringLiteral("\n\n") +
                       tr("This plugin is listed by the server catalog, but the downloadable package is not available "
                          "from the catalog yet.");
    }

    description_label_->setText(tr("%1 %2\nSource: %3\nStatus: %4\n\n%5")
                                    .arg(QString::fromStdString(plugin->name), QString::fromStdString(plugin->version),
                                         current->text(3), status, description));

    install_button_->setText(installed ? tr("Update") : tr("Install"));
    install_button_->setEnabled(plugin_manager_ != nullptr && !refresh_active && plugin->has_remote_package() &&
                                (!installed || can_update));
    obtain_button_->setEnabled(PluginBuildConfig::commercial_plugin_actions_enabled() && !refresh_active &&
                               !purchase_url.isEmpty());
    uninstall_button_->setEnabled(plugin_manager_ != nullptr && !refresh_active && installed);
}

void FolderStructurePluginDialog::update_plugin_enabled_state(QTreeWidgetItem* item, int column) {
    if (!plugin_manager_ || !item || column != 0 || !item->data(0, kInstalledRole).toBool()) {
        return;
    }

    const std::string plugin_id = item->data(0, kPluginIdRole).toString().toStdString();
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

void FolderStructurePluginDialog::start_catalog_refresh(bool interactive) {
    if (!PluginBuildConfig::remote_catalogs_enabled()) {
        return;
    }

    if (!plugin_manager_ || refresh_in_progress_.exchange(true)) {
        return;
    }

    update_selection_state();

    auto manager = plugin_manager_;
    QPointer<FolderStructurePluginDialog> self(this);
    std::thread([manager, self, interactive]() {
        std::string error;
        const bool success = manager->refresh_remote_catalog(&error);
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self, success, error = std::move(error), interactive]() mutable {
                if (!self) {
                    return;
                }
                self->finish_catalog_refresh(success, error, interactive);
            },
            Qt::QueuedConnection);
    }).detach();
}

void FolderStructurePluginDialog::finish_catalog_refresh(bool success, const std::string& error, bool interactive) {
    refresh_in_progress_ = false;
    if (!success) {
        update_selection_state();
        if (interactive) {
            QMessageBox::warning(this, tr("Update check failed"),
                                 error.empty() ? tr("Failed to check for folder-structure plugin updates.")
                                               : QString::fromStdString(error));
        }
        return;
    }

    populate_plugins();
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
        if (plugin_manager_ && install_error.missing_entitlement &&
            PluginBuildConfig::commercial_plugin_actions_enabled()) {
            if (!activate_folder_structure_plugin_entitlement(this, plugin_manager_, install_error)) {
                return;
            }
            install_error = FolderStructurePluginInstallError{};
            if (plugin_manager_->install_from_archive(archive, &installed_plugin_id, &install_error)) {
                populate_plugins();
                if (!installed_plugin_id.empty() && plugin_list_) {
                    for (int row = 0; row < plugin_list_->topLevelItemCount(); ++row) {
                        auto* item = plugin_list_->topLevelItem(row);
                        if (item && item->data(0, kPluginIdRole).toString().toStdString() == installed_plugin_id) {
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
            if (item && item->data(0, kPluginIdRole).toString().toStdString() == installed_plugin_id) {
                plugin_list_->setCurrentItem(item);
                break;
            }
        }
    }
    update_selection_state();
}

void FolderStructurePluginDialog::install_selected_plugin() {
    const std::string plugin_id = selected_plugin_id();
    if (plugin_id.empty()) {
        return;
    }

    const auto result = install_folder_structure_plugin_from_catalog(this, plugin_manager_, plugin_id);
    if (result != FolderStructurePluginInstallFlowResult::Installed) {
        return;
    }

    populate_plugins();
    update_selection_state();
}

void FolderStructurePluginDialog::open_selected_plugin_page() {
    if (!plugin_list_ || !plugin_list_->currentItem()) {
        return;
    }
    const QString purchase_url = plugin_list_->currentItem()->data(0, kPurchaseUrlRole).toString();
    if (purchase_url.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl(purchase_url));
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
    return plugin_list_->currentItem()->data(0, kPluginIdRole).toString().toStdString();
}
