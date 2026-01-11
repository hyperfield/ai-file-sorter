# 2026-01-09 - Image-only processing option for visual analysis

This changelog documents the new **image-only processing** option introduced alongside visual LLM analysis. The goal is to let users run image analysis and optional image renames **without** processing any other file types, and to make the UI state reflect that constraint clearly.

## Motivation and user impact

When image analysis is enabled, some users want to focus only on supported image files and skip the rest of the folder (documents, archives, source code, etc.). The previous flow always scanned and queued non-image entries whenever categorization options were enabled, which could be slow or undesired when the intent is "images only."

This change adds a dedicated checkbox under the image analysis section:

- **Process image files only (ignore any other files)**  
  When enabled, only supported image files are queued, analyzed, and eligible for rename/categorization. The standard categorization controls are disabled to prevent mixed-mode ambiguity.

## UI wiring and translations

The checkbox is inserted as the first item under the image analysis umbrella and gets localized like the other main window controls.

```cpp
// app/lib/MainAppUiBuilder.cpp
app.process_images_only_checkbox = new QCheckBox(central);
image_rename_layout->addWidget(app.process_images_only_checkbox);
```

```cpp
// app/lib/UiTranslator.cpp
if (auto* checkbox = raw_ptr(deps_.primary.process_images_only_checkbox)) {
    // Keep the label explicit about behavior.
    checkbox->setText(tr("Process image files only (ignore any other files)"));
}
```

Translations were added for French, German, Italian, Spanish, and Turkish in `app/lib/TranslationManager.cpp` and the `.ts` resources.

## Persisted setting and state sync

The setting is persisted in the same `Settings` layer as the other image analysis toggles:

```cpp
// app/lib/Settings.cpp
process_images_only = load_bool("ProcessImagesOnly", false);
set_bool_setting(config, settings_section, "ProcessImagesOnly", process_images_only);
```

The main window restores and saves the checkbox state alongside the existing image options.

## Behavior: disable categorization controls

When the checkbox is active **and** image analysis is enabled, the regular categorization controls are disabled to avoid mixed behavior.

```cpp
// app/lib/MainApp.cpp
const bool analyze_images = analyze_images_checkbox && analyze_images_checkbox->isChecked();
const bool images_only_active = analyze_images && process_images_only_checkbox->isChecked();
const bool enable_categorization = !images_only_active;

use_subcategories_checkbox->setEnabled(enable_categorization);
categorize_files_checkbox->setEnabled(enable_categorization);
categorize_directories_checkbox->setEnabled(enable_categorization);
categorization_style_heading->setEnabled(enable_categorization);
categorization_style_refined_radio->setEnabled(enable_categorization);
categorization_style_consistent_radio->setEnabled(enable_categorization);
use_whitelist_checkbox->setEnabled(enable_categorization);
```

```cpp
// app/lib/MainApp.cpp
if (whitelist_selector) {
    // Only enable when categorization is active and the whitelist toggle is on.
    const bool whitelist_enabled = enable_categorization &&
                                   use_whitelist_checkbox &&
                                   use_whitelist_checkbox->isChecked();
    whitelist_selector->setEnabled(whitelist_enabled);
}
```

## Behavior: skip non-image files end-to-end

The analysis flow now skips non-image entries in two places:

1. **Cached results** are pruned if they are not image files, so the cached list cannot re-introduce non-image work items.
2. **Fresh scan results** are filtered so only supported image files remain in the queue.

```cpp
// app/lib/MainApp.cpp
const bool analyze_images = settings.get_analyze_images_by_content();
const bool process_images_only = analyze_images && settings.get_process_images_only();

if (process_images_only) {
    already_categorized_files.erase(
        std::remove_if(already_categorized_files.begin(),
                       already_categorized_files.end(),
                       [](const CategorizedFile& entry) {
                           // Ignore directories entirely in image-only mode.
                           if (entry.type != FileType::File) {
                               return true;
                           }
                           const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                                  Utils::utf8_to_path(entry.file_name);
                           // Keep only supported image file types.
                           return !LlavaImageAnalyzer::is_supported_image(full_path);
                       }),
        already_categorized_files.end());
}
```

```cpp
// app/lib/MainApp.cpp
files_to_categorize = results_coordinator.find_files_to_categorize(...);
if (process_images_only) {
    files_to_categorize.erase(
        std::remove_if(files_to_categorize.begin(),
                       files_to_categorize.end(),
                       [](const FileEntry& entry) {
                           // Only image files are allowed when the option is enabled.
                           if (entry.type != FileType::File) {
                               return true;
                           }
                           return !LlavaImageAnalyzer::is_supported_image(entry.full_path);
                       }),
        files_to_categorize.end());
}
```

This keeps the analysis queue and downstream processing strictly limited to images.

## Tests and coverage

Unit tests were updated to:

- validate that the new checkbox is disabled until image analysis is enabled,
- ensure the UI remains consistent with the new control, and
- persist the setting through save/load.

Files touched:

- `tests/unit/test_main_app_image_options.cpp`
- `tests/unit/test_settings_image_options.cpp`
- `tests/unit/test_ui_translator.cpp`

