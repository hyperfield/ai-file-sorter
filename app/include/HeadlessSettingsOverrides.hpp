#pragma once

#include "CategoryLanguage.hpp"
#include "Language.hpp"

#include <optional>
#include <string>
#include <vector>

/**
 * @brief Optional settings overlay supplied by headless integrations.
 *
 * Unset fields leave the persisted app setting unchanged. Set fields override
 * only the current headless run and are not written back to the app config.
 */
struct HeadlessSettingsOverrides {
    /** @brief Override whether category moves create subcategory folders. */
    std::optional<bool> use_subcategories;
    /** @brief Override whether consistency hints should favor stable labels. */
    std::optional<bool> use_consistency_hints;
    /** @brief Override whether category whitelists are active. */
    std::optional<bool> use_whitelist;
    /** @brief Override the active category whitelist name. */
    std::optional<std::string> active_whitelist;
    /** @brief Override the flat allowed category whitelist for the run. */
    std::optional<std::vector<std::string>> allowed_categories;
    /** @brief Override the flat allowed subcategory whitelist for the run. */
    std::optional<std::vector<std::string>> allowed_subcategories;
    /** @brief Override whether files are included in categorization runs. */
    std::optional<bool> categorize_files;
    /** @brief Override whether folders are included in categorization runs. */
    std::optional<bool> categorize_directories;
    /** @brief Override whether subfolders are scanned. */
    std::optional<bool> include_subdirectories;
    /** @brief Override whether picture files are analyzed by content. */
    std::optional<bool> analyze_images_by_content;
    /** @brief Override whether only picture files are processed. */
    std::optional<bool> process_images_only;
    /** @brief Override whether picture dates are appended to category labels. */
    std::optional<bool> add_image_date_to_category;
    /** @brief Override whether picture date/place prefixes are added to suggested filenames. */
    std::optional<bool> add_image_date_place_to_filename;
    /** @brief Override whether picture rename suggestions are generated. */
    std::optional<bool> offer_rename_images;
    /** @brief Override whether picture files are rename-only. */
    std::optional<bool> rename_images_only;
    /** @brief Override whether audio/video metadata is used in suggested filenames. */
    std::optional<bool> add_audio_video_metadata_to_filename;
    /** @brief Override whether document files are analyzed by content. */
    std::optional<bool> analyze_documents_by_content;
    /** @brief Override whether only document files are processed. */
    std::optional<bool> process_documents_only;
    /** @brief Override whether document rename suggestions are generated. */
    std::optional<bool> offer_rename_documents;
    /** @brief Override whether document files are rename-only. */
    std::optional<bool> rename_documents_only;
    /** @brief Override whether document dates are appended to category labels. */
    std::optional<bool> add_document_date_to_category;
    /** @brief Override the app UI/progress language for the run where supported. */
    std::optional<Language> language;
    /** @brief Override the category output language. */
    std::optional<CategoryLanguage> category_language;
};
