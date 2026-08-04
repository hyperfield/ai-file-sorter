# Progress dialog translations for analysis output (2026-01-10)

## Summary

The analysis progress dialog now fully follows the selected interface language, including the status tags (e.g., `[SCAN]`, `[VISION]`, `[SORT]`) and the descriptive lines shown while scanning and categorizing. Previously, the dialog title/button were translated, but the streaming progress text was hardcoded in English.

## Motivation

Users choosing a non-English UI still saw English-only progress lines in the analysis window, which made the experience feel inconsistent and harder to parse. The goal is to have all visible analysis feedback respect the UI language, especially for a long-running operation where progress messages are the primary feedback channel.

## What changed (implementation details)

1. **Progress messages now use `tr(...)` before rendering**
   - The log lines generated in `MainApp` (scan, queue, processing, vision analysis, sort updates, warnings, and stop notices) now pass through Qt translation via `tr(...)`.
   - We convert the translated `QString` to UTF-8 before sending it into the progress dialog, keeping the dialog’s output transport (std::string) intact.

2. **Progress dialog stop notice is translated**
   - The stop request message triggered by the Stop button is also localized now.

3. **Translation catalogs include the new progress keys**
   - Added entries for all new progress strings in the in-code translation map (`TranslationManager.cpp`).
   - Added the same keys to the UI `.ts` resources for Dutch/French/German/Italian/Spanish/Turkish.

## Code excerpts

### `app/lib/MainApp.cpp`

```cpp
append_progress(to_utf8(tr("[SCAN] Exploring %1")
                            .arg(QString::fromStdString(directory_path))));
// ^ Translates the progress message based on current UI language
//   while keeping path interpolation in a single localized string.
```

```cpp
const QString type_label = entry.type == FileType::Directory ? tr("Directory") : tr("File");
append_progress(to_utf8(tr("[SORT] %1 (%2)")
                            .arg(QString::fromStdString(entry.file_name), type_label)));
// ^ Ensures the file/directory label is localized in the progress log.
```

### `app/lib/CategorizationProgressDialog.cpp`

```cpp
main_app->report_progress(
    tr("[STOP] Analysis will stop after the current item is processed.")
        .toUtf8()
        .toStdString());
// ^ Localizes the stop notice shown in the progress dialog.
```

## User impact

- The analysis progress dialog now matches the chosen interface language throughout the entire workflow.
- Status tags and descriptive phrases are translated; file and directory labels also follow the UI language.
- No workflow changes are required; this is a pure UI/UX consistency improvement.

## Files touched

- `app/lib/MainApp.cpp` (translate progress message generation)
- `app/lib/CategorizationProgressDialog.cpp` (translate stop notice)
- `app/lib/TranslationManager.cpp` (add translation keys)
- `app/resources/i18n/aifilesorter_*.ts` (Dutch/French/German/Italian/Spanish/Turkish message entries)
