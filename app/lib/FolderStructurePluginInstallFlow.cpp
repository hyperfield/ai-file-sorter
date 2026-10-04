#include "FolderStructurePluginInstallFlow.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <cstddef>

#include "FolderStructurePluginManager.hpp"

namespace {

QString flow_tr(const char* text) {
    return QCoreApplication::translate("FolderStructurePluginInstallFlow", text);
}

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
    return flow_tr("this plugin");
}

void show_install_failed(QWidget* parent, const FolderStructurePluginInstallError& install_error) {
    QMessageBox::warning(parent, flow_tr("Install failed"),
                         install_error.message.empty() ? flow_tr("Failed to install folder-structure plugin.")
                                                       : QString::fromStdString(install_error.message));
}

}  // namespace

bool activate_folder_structure_plugin_entitlement(QWidget* parent,
                                                  const std::shared_ptr<FolderStructurePluginManager>& plugin_manager,
                                                  const FolderStructurePluginInstallError& install_error) {
    if (!plugin_manager || !install_error.missing_entitlement || install_error.product_id.empty()) {
        return false;
    }

    QMessageBox license_required(parent);
    license_required.setIcon(QMessageBox::Warning);
    license_required.setWindowTitle(flow_tr("License required"));
    license_required.setText(QString::fromStdString(install_error.message));
    license_required.setInformativeText(
        flow_tr("Activate a license for %1, then AI File Sorter will retry the installation.")
            .arg(plugin_display_name(install_error)));
    auto* activate_button = license_required.addButton(flow_tr("Activate..."), QMessageBox::AcceptRole);
    license_required.addButton(QMessageBox::Cancel);
    license_required.exec();
    if (license_required.clickedButton() != activate_button) {
        return false;
    }

    bool accepted = false;
    const QString license_key =
        QInputDialog::getText(parent, flow_tr("Activate Plugin License"),
                              flow_tr("Paste the license key for %1:").arg(plugin_display_name(install_error)),
                              QLineEdit::Normal, QString(), &accepted);
    if (!accepted || license_key.trimmed().isEmpty()) {
        return false;
    }

    std::string activation_error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool activated =
        plugin_manager->activate_license(install_error.product_id, to_utf8(license_key.trimmed()), &activation_error);
    QApplication::restoreOverrideCursor();

    if (!activated) {
        QMessageBox::warning(parent, flow_tr("Activation failed"),
                             activation_error.empty() ? flow_tr("Failed to activate the plugin license.")
                                                      : QString::fromStdString(activation_error));
        return false;
    }
    return true;
}

FolderStructurePluginInstallFlowResult install_folder_structure_plugin_from_catalog(
    QWidget* parent, const std::shared_ptr<FolderStructurePluginManager>& plugin_manager,
    const std::string& plugin_id) {
    if (!plugin_manager || plugin_id.empty()) {
        FolderStructurePluginInstallError install_error;
        show_install_failed(parent, install_error);
        return FolderStructurePluginInstallFlowResult::Failed;
    }

    FolderStructurePluginInstallError install_error;
    if (plugin_manager->install(plugin_id, &install_error)) {
        return FolderStructurePluginInstallFlowResult::Installed;
    }

    if (install_error.missing_entitlement) {
        if (!activate_folder_structure_plugin_entitlement(parent, plugin_manager, install_error)) {
            return FolderStructurePluginInstallFlowResult::Cancelled;
        }
        install_error = FolderStructurePluginInstallError{};
        if (plugin_manager->install(plugin_id, &install_error)) {
            return FolderStructurePluginInstallFlowResult::Installed;
        }
    }

    show_install_failed(parent, install_error);
    return FolderStructurePluginInstallFlowResult::Failed;
}
