#pragma once

#include "CategoryLanguage.hpp"
#include "FolderStructureTemplates.hpp"
#include "MenuMnemonicController.hpp"

#include <QCoreApplication>
#include <QByteArray>
#include <QString>
#include <string>
#include <string_view>

/** @brief Localizes display metadata without modifying signed or persisted data. */
namespace FolderStructureUiText {

/**
 * @brief Returns a localized built-in template name, preserving external names.
 * @param descriptor Original template descriptor.
 * @return Interface-language label, with the original text as fallback.
 */
inline QString template_name(const FolderStructureTemplates::Descriptor& descriptor)
{
    if (!descriptor.plugin_profile_id.empty()) return QString::fromStdString(descriptor.name);
    return QCoreApplication::translate("FolderStructureTemplates", descriptor.name.c_str());
}

/**
 * @brief Returns a localized built-in description without changing folder names.
 * @param descriptor Original template descriptor.
 * @return Interface-language description, preserving third-party descriptions.
 */
inline QString template_description(const FolderStructureTemplates::Descriptor& descriptor)
{
    if (!descriptor.plugin_profile_id.empty()) return QString::fromStdString(descriptor.description);
    return QCoreApplication::translate("FolderStructureTemplates", descriptor.description.c_str());
}

/**
 * @brief Uses the existing localized language-menu labels for a folder language.
 * @param language Stable language identifier stored in the selector's item data.
 * @return Localized name without mnemonic markers, or the canonical fallback.
 */
inline QString language_name(CategoryLanguage language)
{
    const QString canonical = categoryLanguageToString(language);
    const QByteArray menu_source = (QStringLiteral("&") + canonical).toUtf8();
    const QString menu_label = QCoreApplication::translate("UiTranslator", menu_source.constData());
    if (menu_label != QString::fromUtf8(menu_source)) {
        return MenuMnemonicController::strip_mnemonic_markers(menu_label);
    }
    const QByteArray source = canonical.toUtf8();
    return QCoreApplication::translate("UiTranslator", source.constData());
}

/**
 * @brief Returns the current localized display name for Numbered Folder Tools.
 * @return The current plugin label in the interface language.
 */
inline QString numbered_folder_tools_name()
{
    return QCoreApplication::translate("FolderStructureUiText", "Numbered Folder Tools");
}

/**
 * @brief Maps current and legacy first-party names to the current display name.
 * @param plugin_id Plugin identifier from a catalog or signed manifest.
 * @param name Original plugin name.
 * @return Localized known label; unknown or customized names remain unchanged.
 */
inline QString plugin_name(std::string_view plugin_id, const std::string& name)
{
    if (plugin_id == "johnny_decimal_support" &&
        (name == "Johnny.Decimal Support" || name == "Numbered Folder Tools")) {
        return numbered_folder_tools_name();
    }
    return QString::fromStdString(name);
}

/**
 * @brief Localizes a recognized first-party description with a verbatim fallback.
 * @param plugin_id Plugin identifier from a catalog or signed manifest.
 * @param description Original, unmodified plugin description.
 * @return Localized shipped wording, or the exact external/customized description.
 */
inline QString plugin_description(std::string_view plugin_id, const std::string& description)
{
    constexpr auto kNumberedFolderToolsDescription = QT_TRANSLATE_NOOP("FolderStructureUiText",
        "Create numbered folders, browse the archive index, and check your folder structure. Compatible with Johnny.Decimal.");
    if (plugin_id == "johnny_decimal_support" &&
        (description == kNumberedFolderToolsDescription ||
         description == "Adds Johnny.Decimal starter folders, recognition, and routing guidance." ||
         description == "Adds Johnny.Decimal starter folders and routing guidance." ||
         description == "Adds Johnny.Decimal starter folders, archive recognition, validation, index views, next-folder creation, and routing guidance.")) {
        return QCoreApplication::translate("FolderStructureUiText", kNumberedFolderToolsDescription);
    }
    return QString::fromStdString(description);
}
} // namespace FolderStructureUiText
