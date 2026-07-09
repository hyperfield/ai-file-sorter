# 2026-02-01: Recursive scan, rename gating, and test/documentation expansion

This entry documents a multi-part change set that primarily improves recursive scanning, deconflicts cached results when multiple subfolders are involved, and tightens the rename-only and review-dialog behavior so the UI always matches the user's chosen rename options. It also introduces a comprehensive test matrix (with explicit CLI reporting) and adds a full test catalog in `TESTS.md`.

## Motivation and user-facing intent

The previous UI exposed two related checkboxes: "Categorize folders" (formerly "Categorize directories") and "Include subdirectories". In practice, these are distinct actions:

- "Categorize folders" determines whether the app should classify *directory entries* as items, similar to files.
- "Include subdirectories" determines whether the app should *walk recursively* and scan files that reside in nested paths, flattening their categorization output to the main directory level.

In earlier iterations, these two concepts were easy to conflate. A user could reasonably expect "Include subdirectories" to mean "scan nested files" while the app treated it as a general scan toggle without clearly preventing conflicts with folder categorization. This ambiguity increased once image/document-only and rename-only combinations were added.

In parallel, the review dialog could still show rename-only checkboxes or suggested filename columns even when renaming was not offered for the current run. That was confusing for users, and it made the caching logic behave in ways that were technically correct but visually misleading.

The changes below address these concerns by:

1) making recursive scanning explicit (in code and UI),
2) enforcing consistent rename gating across analysis, cache use, and the review dialog, and
3) adding tests that exhaustively validate the combinatorial state space.

## Detailed changes (what changed, from what, and why)

### 1) Recursive scanning is now a first-class, explicit option

**What changed:**
- A new `FileScanOptions::Recursive` flag was added, and `FileScanner::get_directory_entries()` now performs a recursive walk when this flag is set.
- `Settings` persists a new `IncludeSubdirectories` flag, which adds the recursive scan option at runtime.
- When recursive scanning is enabled, categorization must use full paths as keys to avoid collisions between identical filenames in different subfolders. This is enforced throughout `MainApp` and `ResultsCoordinator`.
- Cached entries are now loaded recursively from the database when recursive scanning is enabled.

**Why:**
- Recursive scanning is a different operation from categorizing directory entries; treating them as separate settings avoids misinterpretation and retains precise control.
- Full path keys are essential in recursive mode to avoid the classic filename collision problem (two different files named `report.pdf` in different subfolders).

**How:**
- `MainApp::effective_scan_options()` appends the `Recursive` flag only when files are being scanned and the new setting is enabled.
- The results pipeline propagates a `use_full_path_keys` boolean into the cache and "to be categorized" sets.
- The database layer can now query all cached entries under a folder prefix when recursion is enabled.

### 2) Rename-only and "offer rename" logic is now consistent in UI and cache flow

**What changed:**
- The review dialog now receives explicit `allow_image_renames` and `allow_document_renames` flags; it disables the corresponding rename-only checkboxes when renaming is not offered.
- Main analysis logic clears cached rename suggestions when renaming is not allowed for a given file type, preventing the "Suggested filename" column from appearing unless it is actually actionable.
- Image/document analysis still runs when requested, but suggested names are only exposed in the UI when rename options permit it.

**Why:**
- The previous behavior could show rename-only toggles or suggested filename columns even when renaming was not active in the current run, leading to confusion and a misleading review dialog.
- This update ensures the UI matches the user's intent: if renaming is off, no rename controls are shown or enabled.

**How:**
- In `MainApp`, cached entries are filtered so rename-only entries are skipped when renaming is not allowed; otherwise their suggested names are cleared.
- `CategorizationDialog::show_results()` stores the rename-allowed flags and disables the checkbox state when renames are disallowed.

### 3) UI clarity: "Include subdirectories" renamed to "Scan subfolders"

**What changed:**
- The label was renamed to "Scan subfolders" (with updated translations) to emphasize recursion rather than folder categorization.

**Why:**
- The new label clarifies the intent and reduces ambiguity with "Categorize folders".

### 4) Test coverage and documentation expansion

**What changed:**
- Added an exhaustive checkbox-combination matrix test that validates routing of files through analysis/categorization pipelines for all 128 flag combinations, with and without renamed files.
- Added a review dialog gate test to ensure rename-only checkboxes are disabled when renaming is not permitted.
- Added a Catch2 event listener that prints the test case name as each test starts, improving CLI readability for long runs.
- Added Doxygen-style comments to new test helper headers and created a comprehensive `TESTS.md` catalog.

**Why:**
- The state space of checkbox combinations has grown enough that spot checks were no longer sufficient.
- A readable test log is critical when validating long multi-case runs.
- The documentation explicitly records test behavior so new contributors can reason about the suite without spelunking code.

## Code excerpts (with inline commentary)

### A) Recursive scanning and file entry collection

```cpp
// app/lib/FileScanner.cpp
std::vector<FileEntry>
FileScanner::get_directory_entries(const std::string &directory_path,
                                   FileScanOptions options)
{
    // ...
    const bool recursive = has_flag(options, FileScanOptions::Recursive);

    if (!recursive) {
        // Non-recursive: just walk the top-level directory.
        for (const auto &entry : fs::directory_iterator(scan_path)) {
            if (auto entry_info = build_entry(entry, context)) {
                file_paths_and_names.push_back(std::move(*entry_info));
            }
        }
    } else {
        // Recursive: walk all subfolders, skipping hidden entries and bundles.
        for (fs::recursive_directory_iterator it(scan_path), end; it != end; ++it) {
            const auto& entry = *it;
            const fs::path& entry_path = entry.path();
            const std::string full_path = Utils::path_to_utf8(entry_path);
            const std::string file_name = Utils::path_to_utf8(entry_path.filename());
            const bool bundle = is_file_bundle(entry_path);
            if (bundle) {
                it.disable_recursion_pending(); // Do not dive into app bundles.
            }
            if (should_skip_entry(entry_path, file_name, context, full_path)) {
                if (entry.is_directory()) {
                    it.disable_recursion_pending(); // Skip hidden/junk directories.
                }
                continue;
            }
            if (auto type = classify_entry(entry, bundle, context)) {
                file_paths_and_names.push_back(FileEntry{full_path, file_name, *type});
            }
        }
    }
    // ...
}
```

### B) Effective scan options now include recursion when requested

```cpp
// app/lib/MainApp.cpp
FileScanOptions MainApp::effective_scan_options() const
{
    // ...
    FileScanOptions options = file_scan_options;
    if (analyze_images || analyze_documents) {
        options = options | FileScanOptions::Files; // Ensure files are scanned.
    }
    if (settings.get_include_subdirectories() && has_flag(options, FileScanOptions::Files)) {
        options = options | FileScanOptions::Recursive; // Activate recursive walk.
    }
    return options;
}
```

### C) Recursive cache loading and full-path keys to avoid collisions

```cpp
// app/lib/CategorizationService.cpp
std::vector<CategorizedFile> CategorizationService::load_cached_entries(
    const std::string& directory_path) const
{
    // Recursive mode uses a directory-prefix query to pull cached entries
    // from nested paths. Non-recursive mode stays with an exact match.
    if (settings.get_include_subdirectories()) {
        return db_manager.get_categorized_files_recursive(directory_path);
    }
    return db_manager.get_categorized_files(directory_path);
}
```

```cpp
// app/lib/ResultsCoordinator.cpp
for (const auto& entry : actual_files) {
    // Full path keys are required in recursive mode to distinguish duplicates.
    const std::string key = use_full_path_keys ? entry.full_path : entry.file_name;
    if (!cached_files.contains(key)) {
        found_files.push_back(entry);
    }
}
```

### D) Rename gating in the review dialog

```cpp
// app/lib/CategorizationDialog.cpp
void CategorizationDialog::show_results(const std::vector<CategorizedFile>& files,
                                        const std::string& base_dir_override,
                                        bool include_subdirectories,
                                        bool allow_image_renames,
                                        bool allow_document_renames)
{
    categorized_files = files;
    include_subdirectories_ = include_subdirectories;
    allow_image_renames_ = allow_image_renames;
    allow_document_renames_ = allow_document_renames;

    // Hide the Suggested Filename column when there are no active suggestions.
    set_show_rename_column(std::any_of(categorized_files.begin(),
                                       categorized_files.end(),
                                       [](const CategorizedFile& file) {
                                           return !file.suggested_name.empty();
                                       }));

    update_rename_only_checkbox_state(); // Disable rename-only checkboxes if renames are disallowed.
    // ... populate UI and open dialog
}
```

```cpp
// app/lib/CategorizationDialog.cpp
void CategorizationDialog::update_rename_only_checkbox_state()
{
    // The rename-only checkboxes are only enabled if:
    // (1) there are items of that type in the results AND
    // (2) renaming is allowed for that type in this run.
    const bool enable_images_checkbox = has_images && allow_image_renames_;
    rename_images_only_checkbox->setEnabled(enable_images_checkbox);
    rename_images_only_checkbox->setChecked(enable_images_checkbox && all_rename_only);

    const bool enable_documents_checkbox = has_documents && allow_document_renames_;
    rename_documents_only_checkbox->setEnabled(enable_documents_checkbox);
    rename_documents_only_checkbox->setChecked(enable_documents_checkbox && all_doc_rename_only);
}
```

### E) Matrix test that exhaustively validates checkbox combinations

```cpp
// tests/unit/test_checkbox_matrix.cpp
for (int mask = 0; mask < 128; ++mask) {
    Combo combo{
        static_cast<bool>(mask & (1 << 0)), // analyze_images
        static_cast<bool>(mask & (1 << 1)), // analyze_documents
        static_cast<bool>(mask & (1 << 2)), // process_images_only
        static_cast<bool>(mask & (1 << 3)), // process_documents_only
        static_cast<bool>(mask & (1 << 4)), // rename_images_only
        static_cast<bool>(mask & (1 << 5)), // rename_documents_only
        static_cast<bool>(mask & (1 << 6))  // categorize_files
    };

    MainAppTestAccess::split_entries_for_analysis(...);

    // Each entry is assigned to exactly one expected bucket (or none), and
    // we compare the computed expectation to the actual split result.
    CHECK(expected == actual);
}
```

### F) CLI-visible test case banner

```cpp
// tests/unit/test_cli_reporter.cpp
void testCaseStarting(Catch::TestCaseInfo const& info) override {
    // This makes long test runs easier to follow in CI logs.
    std::cout << "[TEST] " << info.name << std::endl;
}
```

## Additional notes and behavioral details

- The `Settings` loader now enforces that `IncludeSubdirectories` and `CategorizeFolders` are mutually exclusive. If recursive scanning is enabled, folder categorization is disabled to avoid ambiguous behavior when the traversal is recursive but folder entries are not explicitly enumerated as items.
- The rename-only changes ensure that the "Suggested filename" column is not visible when rename suggestions are not actionable. This also prevents stale cached suggestions from appearing when the run is configured for categorization only.
- The label update to "Scan subfolders" is intentionally short to fit the top-row checkbox layout and to clearly distinguish it from "Categorize folders".

## Summary

This change set makes recursive scanning a deliberate, explicit capability that safely handles filename collisions via full-path keys and recursive cache queries. It aligns rename-only behavior with the user's actual rename settings in both data processing and UI presentation. Finally, it expands test coverage to cover the full combinatorial space of checkbox states and documents the test suite in a comprehensive, textbook-style reference.
