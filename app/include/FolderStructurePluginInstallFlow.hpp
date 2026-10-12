#pragma once

#include <memory>
#include <string>

class QWidget;

class FolderStructurePluginManager;
struct FolderStructurePluginInstallError;

/**
 * @brief Result of a user-facing folder-structure plugin install flow.
 */
enum class FolderStructurePluginInstallFlowResult {
    Installed,
    Cancelled,
    Failed,
};

/**
 * @brief Prompts for and activates a missing folder-structure plugin entitlement.
 * @param parent Parent widget for modal UI.
 * @param plugin_manager Plugin manager that performs activation.
 * @param install_error Missing-entitlement install error.
 * @return True when activation completed.
 */
bool activate_folder_structure_plugin_entitlement(QWidget* parent,
                                                  const std::shared_ptr<FolderStructurePluginManager>& plugin_manager,
                                                  const FolderStructurePluginInstallError& install_error);

/**
 * @brief Prompts for a license key and activates a missing folder-structure plugin entitlement.
 * @param parent Parent widget for modal UI.
 * @param plugin_manager Plugin manager that performs activation.
 * @param install_error Missing-entitlement metadata for the selected plugin.
 * @return True when activation completed or a valid entitlement already exists.
 */
bool prompt_for_folder_structure_plugin_entitlement(QWidget* parent,
                                                    const std::shared_ptr<FolderStructurePluginManager>& plugin_manager,
                                                    const FolderStructurePluginInstallError& install_error);

/**
 * @brief Installs a catalog plugin, prompting for activation and retrying when entitlement is missing.
 * @param parent Parent widget for modal UI.
 * @param plugin_manager Plugin manager that performs catalog installation.
 * @param plugin_id Plugin id to install.
 * @return Install flow result.
 */
FolderStructurePluginInstallFlowResult install_folder_structure_plugin_from_catalog(
    QWidget* parent, const std::shared_ptr<FolderStructurePluginManager>& plugin_manager, const std::string& plugin_id);
