#include "FolderStructurePluginInstallFlow.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <cstddef>

#include "FolderStructurePluginManager.hpp"
#include "PluginBuildConfig.hpp"

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

bool entitlement_is_available(const std::shared_ptr<FolderStructurePluginManager>& plugin_manager,
                              const FolderStructurePluginInstallError& install_error) {
    return plugin_manager &&
           plugin_manager->has_plugin_entitlement(install_error.product_id, install_error.plugin_id);
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
    if (!PluginBuildConfig::commercial_plugin_actions_enabled()) {
        return false;
    }
    if (entitlement_is_available(plugin_manager, install_error)) {
        return true;
    }

    QMessageBox license_required(parent);
    license_required.setIcon(QMessageBox::Warning);
    license_required.setWindowTitle(flow_tr("License needed"));
    license_required.setText(QString::fromStdString(install_error.message));
    license_required.setInformativeText(
        flow_tr("Activate your license now and AI File Sorter will continue the installation automatically."));
    QPushButton* read_more_button = nullptr;
    if (!install_error.purchase_url.empty()) {
        read_more_button = license_required.addButton(flow_tr("Read More..."), QMessageBox::ActionRole);
    }
    auto* activate_button = license_required.addButton(flow_tr("Activate License..."), QMessageBox::AcceptRole);
    license_required.addButton(QMessageBox::Cancel);
    bool entitlement_detected = false;
    QTimer entitlement_refresh_timer(&license_required);
    entitlement_refresh_timer.setInterval(1000);
    QObject::connect(&entitlement_refresh_timer, &QTimer::timeout, &license_required, [&]() {
        if (entitlement_is_available(plugin_manager, install_error)) {
            entitlement_detected = true;
            license_required.accept();
        }
    });
    entitlement_refresh_timer.start();
    license_required.exec();
    if (entitlement_detected || entitlement_is_available(plugin_manager, install_error)) {
        return true;
    }
    if (read_more_button && license_required.clickedButton() == read_more_button) {
        QDesktopServices::openUrl(QUrl(QString::fromStdString(install_error.purchase_url)));
        return false;
    }
    if (license_required.clickedButton() != activate_button) {
        return false;
    }

    entitlement_refresh_timer.stop();
    return prompt_for_folder_structure_plugin_entitlement(parent, plugin_manager, install_error);
}

bool prompt_for_folder_structure_plugin_entitlement(QWidget* parent,
                                                    const std::shared_ptr<FolderStructurePluginManager>& plugin_manager,
                                                    const FolderStructurePluginInstallError& install_error) {
    if (!plugin_manager || !install_error.missing_entitlement || install_error.product_id.empty()) {
        return false;
    }
    if (!PluginBuildConfig::commercial_plugin_actions_enabled()) {
        return false;
    }
    if (entitlement_is_available(plugin_manager, install_error)) {
        return true;
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

    if (install_error.missing_entitlement && PluginBuildConfig::commercial_plugin_actions_enabled()) {
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
