# 2026-01-10 - Rename-applied cache, review visibility, picture icons, review text updates

This changelog captures the changes implemented during this session. The goals were to:

- avoid reprocessing picture files that were already renamed in prior runs,
- make review rows clearer based on whether rename-only is active,
- keep categorization possible for already-renamed files without re-running image analysis,
- add a picture icon in the Type column, and
- rename the review dialog title and the first column label, with translations.

## Cache: persist "rename applied" so renamed pictures do not re-run

Motivation: once an image rename is executed, the app should not offer it again or re-run
visual analysis on the same file. We need a stable cache flag that survives restarts and
folder re-analysis.

### Schema changes and sticky upsert

The cache table now stores `rename_applied`, and the upsert keeps it sticky once it
becomes `1`.

```sql
CREATE TABLE IF NOT EXISTS file_categorization (
    ...
    rename_only INTEGER DEFAULT 0,
    rename_applied INTEGER DEFAULT 0, -- keep a permanent "renamed" record
    ...
);
```

```sql
INSERT INTO file_categorization (...)
ON CONFLICT(file_name, file_type, dir_path)
DO UPDATE SET
    ...
    rename_applied = CASE
        WHEN excluded.rename_applied = 1 THEN 1 -- once renamed, stay renamed
        ELSE rename_applied
    END;
```

This means:

- a rename-only run can mark `rename_applied = 1` after it succeeds,
- later runs keep the "already renamed" state even if other fields are updated,
- we can safely skip visual LLM work for already-renamed pictures.

## Analysis flow: skip visual LLM for already-renamed pictures

Motivation: if a picture was already renamed, the app should not re-run image analysis
or re-offer a rename. When categorization is enabled, it should still categorize
based on filename/path; when rename-only is enabled, it should skip entirely.

The analysis entry splitter now routes already-renamed pictures away from the image
analysis queue:

```cpp
if (analyze_images && is_image_entry) {
    const bool already_renamed = renamed_files.contains(entry.file_name);
    if (already_renamed) {
        if (rename_images_only) {
            continue; // rename-only: skip the entry entirely
        }
        other_entries.push_back(entry); // categorize from filename/path only
    } else {
        image_entries.push_back(entry); // run visual analysis
    }
}
```

This ensures:

- rename-only runs do not re-process already-renamed files,
- categorization runs still allow folder moves without re-running the visual model,
- suggested names are not regenerated for already-renamed items.

## Review dialog: hide already-renamed rows when rename-only is ON

Motivation: in rename-only mode, the review should focus only on items the app will
actually rename. Already-renamed files with assigned categories can still be useful
when categorization is enabled, so those remain visible in that mode.

### Rename-locked tracking

Rows now carry a `rename_locked` flag that is true for either:

- `rename_applied = true`, or
- legacy cases where `suggested_name == file_name`.

```cpp
const bool rename_locked = file.rename_applied ||
                           (!file.suggested_name.empty() &&
                            to_lower_copy_str(file.suggested_name) ==
                            to_lower_copy_str(file.file_name));
file_item->setData(rename_locked, kRenameLockedRole); // used for visibility filters
```

### Visibility rules

When rename-only is enabled, rows that are already renamed and also have a category
(and subcategory, if enabled) are hidden from the table:

```cpp
const bool hide_inactive_rows = rename_images_only_checkbox &&
                                rename_images_only_checkbox->isChecked();
for (int row = 0; row < model->rowCount(); ++row) {
    const bool hide_row = hide_inactive_rows &&
                          row_is_already_renamed_with_category(row);
    table_view->setRowHidden(row, hide_row); // hide only in rename-only mode
}
```

Practical outcome:

- rename-only ON: show only rows that can still be renamed,
- rename-only OFF: show all rows, including already-renamed items that still need
  category folder moves.

## Review dialog: picture icon in the Type column

Motivation: the Type column currently shows only a generic file icon or directory icon.
Pictures benefit from a dedicated visual indicator so they are easier to spot.

We now tag pictures with a dedicated type code and map that to a themed image icon,
falling back to the normal file icon if the theme does not include an image glyph:

```cpp
if (code == QStringLiteral("I")) {
    QIcon icon = QIcon::fromTheme(QStringLiteral("image-x-generic"));
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("image"));
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("image-x-generic-symbolic"));
    }
    return icon.isNull() ? style->standardIcon(QStyle::SP_FileIcon) : icon;
}
```

## Review dialog text: rename title and first column

The dialog title and the first column label were updated for clarity:

- "Review Categorization" -> "Review and Confirm"
- "Move" -> "Process"

These strings were updated in both the in-app translation map and the .ts translation files
for French, German, Italian, Spanish, and Turkish.

## Main window tooltips

Motivation: new users need quick, on-hover explanations of what each checkbox and
radio option does, without having to consult external documentation.

Tooltips were added to the main window checkboxes and categorization style radios,
including the image analysis options. Examples include:

```cpp
checkbox->setToolTip(tr("Create subcategory folders within each category."));
refined_radio->setToolTip(tr("Favor detailed labels even if similar items vary."));
checkbox->setToolTip(tr("Run the visual LLM on supported picture files."));
```

These tooltip strings are localized in the static translation map and in the
resource `.ts` files for French, German, Italian, Spanish, and Turkish.

## Tests

New/updated tests cover:

- hiding already-renamed rows when rename-only is enabled,
- routing already-renamed pictures away from the visual analysis path,
- defaulting rename-applied to false for new cache rows.

See:

- `tests/unit/test_categorization_dialog.cpp`
- `tests/unit/test_main_app_image_options.cpp`
- `tests/unit/test_database_manager_rename_only.cpp`
