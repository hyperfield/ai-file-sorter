#include "FolderStructurePluginDialog.hpp"

#include "FolderStructurePluginManager.hpp"
#include "Utils.hpp"

#include <QAbstractItemView>
#include <QByteArray>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <cstddef>
#include <utility>
#include <vector>

namespace {

QString dialog_tr(const char* source)
{
    return QCoreApplication::translate("FolderStructurePluginDialog", source);
}

std::string to_utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

} // namespace

FolderStructurePluginDialog::FolderStructurePluginDialog(
    std::shared_ptr<FolderStructurePluginManager> plugin_manager,
    QWidget* parent)
    : QDialog(parent),
      plugin_manager_(std::move(plugin_manager))
{
    setWindowTitle(dialog_tr("Manage Folder Structure Plugins"));
    resize(640, 380);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        dialog_tr("Install signed folder-structure plugins that add templates and routing guidance."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    plugin_list_ = new QTreeWidget(this);
    plugin_list_->setColumnCount(3);
    plugin_list_->setHeaderLabels({dialog_tr("Plugin"), dialog_tr("Version"), dialog_tr("Signer")});
    plugin_list_->setRootIsDecorated(false);
    plugin_list_->setUniformRowHeights(true);
    plugin_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    plugin_list_->header()->setStretchLastSection(false);
    plugin_list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    plugin_list_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    plugin_list_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(plugin_list_, 1);

    description_label_ = new QLabel(this);
    description_label_->setWordWrap(true);
    description_label_->setMinimumHeight(72);
    layout->addWidget(description_label_);

    auto* button_row = new QDialogButtonBox(QDialogButtonBox::Close, this);
    import_button_ = button_row->addButton(dialog_tr("Install from File..."), QDialogButtonBox::ActionRole);
    uninstall_button_ = button_row->addButton(dialog_tr("Uninstall"), QDialogButtonBox::ActionRole);
    button_row->setCenterButtons(true);
    layout->addWidget(button_row);

    connect(button_row, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(plugin_list_, &QTreeWidget::currentItemChanged, this, [this]() {
        update_selection_state();
    });
    connect(import_button_, &QPushButton::clicked, this, [this]() {
        import_plugin_archive();
    });
    connect(uninstall_button_, &QPushButton::clicked, this, [this]() {
        uninstall_selected_plugin();
    });

    populate_plugins();
    update_selection_state();
}

void FolderStructurePluginDialog::populate_plugins()
{
    if (!plugin_manager_ || !plugin_list_) {
        return;
    }

    const std::string current_plugin_id = selected_plugin_id();
    plugin_list_->clear();

    const std::vector<FolderStructurePluginManifest> plugins = plugin_manager_->installed_plugins();
    for (const auto& plugin : plugins) {
        auto* item = new QTreeWidgetItem(plugin_list_);
        item->setText(0, QString::fromStdString(plugin.name));
        item->setText(1, QString::fromStdString(plugin.version));
        item->setText(2, QString::fromStdString(plugin.verified_signer_key_id));
        item->setData(0, Qt::UserRole, QString::fromStdString(plugin.id));
        item->setToolTip(0, QString::fromStdString(plugin.description));
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

void FolderStructurePluginDialog::update_selection_state()
{
    if (!description_label_ || !import_button_ || !uninstall_button_) {
        return;
    }

    import_button_->setEnabled(plugin_manager_ != nullptr);
    auto* current = plugin_list_ ? plugin_list_->currentItem() : nullptr;
    uninstall_button_->setEnabled(plugin_manager_ != nullptr && current != nullptr);
    if (!current) {
        description_label_->setText(dialog_tr("No verified folder-structure plugins are installed."));
        return;
    }

    const QString name = current->text(0);
    const QString version = current->text(1);
    const QString signer = current->text(2);
    const QString description = current->toolTip(0);
    description_label_->setText(
        dialog_tr("%1 %2\nVerified signer: %3\n\n%4")
            .arg(name, version, signer, description));
}

void FolderStructurePluginDialog::import_plugin_archive()
{
    const QString archive_path = QFileDialog::getOpenFileName(
        this,
        dialog_tr("Install Folder Structure Plugin"),
        QString(),
        dialog_tr("AI File Sorter plugins (*.aifsplugin *.zip);;All files (*)"));
    if (archive_path.isEmpty()) {
        return;
    }

    std::string installed_plugin_id;
    std::string error;
    if (!plugin_manager_ ||
        !plugin_manager_->install_from_archive(Utils::utf8_to_path(to_utf8(archive_path)),
                                               &installed_plugin_id,
                                               &error)) {
        QMessageBox::warning(this,
                             dialog_tr("Install failed"),
                             error.empty()
                                 ? dialog_tr("Failed to install folder-structure plugin.")
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

void FolderStructurePluginDialog::uninstall_selected_plugin()
{
    const std::string plugin_id = selected_plugin_id();
    if (plugin_id.empty()) {
        return;
    }

    std::string error;
    if (!plugin_manager_ || !plugin_manager_->uninstall(plugin_id, &error)) {
        QMessageBox::warning(this,
                             dialog_tr("Uninstall failed"),
                             error.empty()
                                 ? dialog_tr("Failed to uninstall folder-structure plugin.")
                                 : QString::fromStdString(error));
        return;
    }

    populate_plugins();
    update_selection_state();
}

std::string FolderStructurePluginDialog::selected_plugin_id() const
{
    if (!plugin_list_ || !plugin_list_->currentItem()) {
        return {};
    }
    return plugin_list_->currentItem()->data(0, Qt::UserRole).toString().toStdString();
}
