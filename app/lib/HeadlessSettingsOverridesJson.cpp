#include "HeadlessSettingsOverridesJson.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QByteArray>
#include <QString>
#include <QStringView>

#include <optional>
#include <utility>
#include <vector>

namespace {

std::optional<bool> optional_bool_from_json(const QJsonObject& object, QStringView key)
{
    const QJsonValue value = object.value(key);
    if (!value.isBool()) {
        return std::nullopt;
    }
    return value.toBool();
}

std::optional<std::vector<std::string>> optional_string_array_from_json(const QJsonObject& object,
                                                                        QStringView key)
{
    const QJsonValue value = object.value(key);
    if (!value.isArray()) {
        return std::nullopt;
    }

    std::vector<std::string> values;
    const QJsonArray array = value.toArray();
    values.reserve(static_cast<std::size_t>(array.size()));
    for (const QJsonValue& item : array) {
        if (!item.isString()) {
            continue;
        }
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            values.push_back(text.toStdString());
        }
    }
    return values;
}

std::optional<std::string> optional_string_from_json(const QJsonObject& object, QStringView key)
{
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return std::nullopt;
    }
    const QByteArray utf8 = value.toString().trimmed().toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

template <typename Setter>
void set_optional_bool(const QJsonObject& object, QStringView key, Setter setter)
{
    if (const auto value = optional_bool_from_json(object, key)) {
        setter(*value);
    }
}

std::optional<SortingMode> optional_sorting_mode_from_json(const QJsonObject& object, QStringView key)
{
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return std::nullopt;
    }
    const QString mode = value.toString().trimmed().toLower();
    if (mode == QStringLiteral("existingfoldertree") ||
        mode == QStringLiteral("existing-folder-tree") ||
        mode == QStringLiteral("existing_folder_tree") ||
        mode == QStringLiteral("foldertree") ||
        mode == QStringLiteral("folder-tree")) {
        return SortingMode::ExistingFolderTree;
    }
    if (mode == QStringLiteral("generatedcategories") ||
        mode == QStringLiteral("generated-categories") ||
        mode == QStringLiteral("generated_categories") ||
        mode == QStringLiteral("categories")) {
        return SortingMode::GeneratedCategories;
    }
    return std::nullopt;
}

} // namespace

HeadlessSettingsOverrides HeadlessSettingsOverridesJson::from_json(const QJsonObject& object)
{
    HeadlessSettingsOverrides overrides;
    set_optional_bool(object, QStringLiteral("useSubcategories"), [&](bool value) {
        overrides.use_subcategories = value;
    });
    if (const auto mode = optional_sorting_mode_from_json(object, QStringLiteral("sortingMode"))) {
        overrides.sorting_mode = *mode;
    }
    if (const auto destination = optional_string_from_json(object, QStringLiteral("destinationFolder"))) {
        overrides.destination_folder = *destination;
    }
    set_optional_bool(object, QStringLiteral("suggestNewFolders"), [&](bool value) {
        overrides.suggest_new_folders = value;
    });
    set_optional_bool(object, QStringLiteral("useConsistencyHints"), [&](bool value) {
        overrides.use_consistency_hints = value;
    });
    set_optional_bool(object, QStringLiteral("useWhitelist"), [&](bool value) {
        overrides.use_whitelist = value;
    });
    const QJsonValue active_whitelist = object.value(QStringLiteral("activeWhitelist"));
    if (active_whitelist.isString()) {
        overrides.active_whitelist = active_whitelist.toString().toStdString();
    }
    if (auto values = optional_string_array_from_json(object, QStringLiteral("allowedCategories"))) {
        overrides.allowed_categories = std::move(*values);
    }
    if (auto values = optional_string_array_from_json(object, QStringLiteral("allowedSubcategories"))) {
        overrides.allowed_subcategories = std::move(*values);
    }
    set_optional_bool(object, QStringLiteral("categorizeFiles"), [&](bool value) {
        overrides.categorize_files = value;
    });
    set_optional_bool(object, QStringLiteral("categorizeDirectories"), [&](bool value) {
        overrides.categorize_directories = value;
    });
    set_optional_bool(object, QStringLiteral("includeSubdirectories"), [&](bool value) {
        overrides.include_subdirectories = value;
    });
    set_optional_bool(object, QStringLiteral("analyzeImagesByContent"), [&](bool value) {
        overrides.analyze_images_by_content = value;
    });
    set_optional_bool(object, QStringLiteral("processImagesOnly"), [&](bool value) {
        overrides.process_images_only = value;
    });
    set_optional_bool(object, QStringLiteral("addImageDateToCategory"), [&](bool value) {
        overrides.add_image_date_to_category = value;
    });
    set_optional_bool(object, QStringLiteral("addImageDatePlaceToFilename"), [&](bool value) {
        overrides.add_image_date_place_to_filename = value;
    });
    set_optional_bool(object, QStringLiteral("offerRenameImages"), [&](bool value) {
        overrides.offer_rename_images = value;
    });
    set_optional_bool(object, QStringLiteral("renameImagesOnly"), [&](bool value) {
        overrides.rename_images_only = value;
    });
    set_optional_bool(object, QStringLiteral("addAudioVideoMetadataToFilename"), [&](bool value) {
        overrides.add_audio_video_metadata_to_filename = value;
    });
    set_optional_bool(object, QStringLiteral("analyzeDocumentsByContent"), [&](bool value) {
        overrides.analyze_documents_by_content = value;
    });
    set_optional_bool(object, QStringLiteral("processDocumentsOnly"), [&](bool value) {
        overrides.process_documents_only = value;
    });
    set_optional_bool(object, QStringLiteral("offerRenameDocuments"), [&](bool value) {
        overrides.offer_rename_documents = value;
    });
    set_optional_bool(object, QStringLiteral("renameDocumentsOnly"), [&](bool value) {
        overrides.rename_documents_only = value;
    });
    set_optional_bool(object, QStringLiteral("addDocumentDateToCategory"), [&](bool value) {
        overrides.add_document_date_to_category = value;
    });

    const QJsonValue language = object.value(QStringLiteral("language"));
    if (language.isString()) {
        overrides.language = languageFromString(language.toString());
    }
    const QJsonValue category_language = object.value(QStringLiteral("categoryLanguage"));
    if (category_language.isString()) {
        overrides.category_language = categoryLanguageFromString(category_language.toString());
    }
    return overrides;
}
