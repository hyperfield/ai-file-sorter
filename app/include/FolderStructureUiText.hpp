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
 * @brief Localizes the known first-party plugin name without rewriting catalog data.
 * @param plugin_id Catalog plugin identifier.
 * @param name Name received from the catalog.
 * @return Localized known label; unknown or customized names remain unchanged.
 */
inline QString plugin_name(std::string_view plugin_id, const std::string& name)
{
    if (plugin_id == "johnny_decimal_support" && name == "Johnny.Decimal Support") {
        return QCoreApplication::translate("FolderStructureUiText", "Johnny.Decimal Support");
    }
    return QString::fromStdString(name);
}

/**
 * @brief Localizes a recognized first-party description with a verbatim fallback.
 * @param plugin_id Catalog plugin identifier.
 * @param description Original, unmodified catalog description.
 * @return Localized shipped wording, or the exact external/customized description.
 */
inline QString plugin_description(std::string_view plugin_id, const std::string& description)
{
    constexpr auto kJohnnyDecimalDescription = QT_TRANSLATE_NOOP("FolderStructureUiText",
        "Adds Johnny.Decimal starter folders, archive recognition, validation, index views, next-folder creation, and routing guidance.");
    if (plugin_id == "johnny_decimal_support" && description == kJohnnyDecimalDescription) {
        return QCoreApplication::translate("FolderStructureUiText", kJohnnyDecimalDescription);
    }
    return QString::fromStdString(description);
}
} // namespace FolderStructureUiText
