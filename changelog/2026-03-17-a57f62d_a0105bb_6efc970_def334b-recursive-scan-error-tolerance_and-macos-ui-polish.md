# 2026-03-17: Recursive-scan filesystem error tolerance and macOS/native UI polish

## Covered commits
- `a57f62d` `2026-03-17` `fix(scanner): tolerate filesystem errors during recursive scans`
- `a0105bb` `2026-03-17` `test(scanner): cover recursive traversal and unreadable subtrees`
- `6efc970` `2026-03-17` `fix(build): include Homebrew macOS pkg-config metadata for libmediainfo`
- `def334b` `2026-03-18` `fix(ui): use native macOS disclosure toggles and align media checkbox`
- `8863056` `2026-03-17` `chore(deps): bump llama.cpp submodule`

## Motivation
Recursive scanning is only useful if it survives real filesystems, including unreadable subtrees. In parallel, the macOS UI still needed more native-feeling disclosure controls and alignment around the media checkbox, and macOS builders needed better MediaInfo pkg-config support.

## What changed
The grouped work taught recursive scans to tolerate filesystem errors, added tests for unreadable subtrees, improved Homebrew pkg-config discovery for MediaInfo, used more native macOS disclosure toggles and checkbox alignment, and refreshed the llama.cpp submodule in support of the broader platform pass.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `a57f62d`
```diff
diff --git a/app/include/FileScanner.hpp b/app/include/FileScanner.hpp
--- a/app/include/FileScanner.hpp
+++ b/app/include/FileScanner.hpp
@@ -18,6 +18,16 @@ public:
 
 private:
     struct ScanContext;
+    void scan_non_recursive(const fs::path& scan_path,
+                            const ScanContext& context,
+                            std::vector<FileEntry>& results);
+    void scan_recursive(const fs::path& scan_path,
+                        const ScanContext& context,
+                        std::vector<FileEntry>& results);
+    void log_scan_warning(const ScanContext& context,
+                          const fs::path& path,
+                          const std::error_code& error,
+                          const char* action) const;
     std::optional<FileEntry> build_entry(const fs::directory_entry& entry,
                                          const ScanContext& context);
     bool should_skip_entry(const fs::path& entry_path,
@@ -26,10 +36,11 @@ private:
                            const std::string& full_path) const;
     std::optional<FileType> classify_entry(const fs::directory_entry& entry,
                                            bool bundle,
+                                           bool is_directory,
                                            const ScanContext& context) const;
     bool is_file_hidden(const fs::path &path) const;
     bool is_junk_file(const std::string& name) const;
-    bool is_file_bundle(const fs::path& path) const;
+    bool is_file_bundle(const fs::path& path, bool is_directory) const;
 };
 
 #endif
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `a0105bb`
```diff
diff --git a/TESTS.md b/TESTS.md
--- a/TESTS.md
+++ b/TESTS.md
@@ -335,6 +335,20 @@ Procedure: Scan once for files and once for directories.
 Expected outcome: The bundle appears only in the file scan and not in the directory scan.
 Run: `./build-tests/ai_file_sorter_tests "application bundles are treated as files"`
 
+#### Test case: recursive scans include nested files
+Purpose: Ensure recursive scans still return files from nested subdirectories.
+Setup: Create one file in the root and one file in a nested subdirectory.
+Procedure: Scan with `Files | Recursive`.
+Expected outcome: Both files appear in the results.
+Run: `./build-tests/ai_file_sorter_tests "recursive scans include nested files"`
+
+#### Test case: recursive scans skip unreadable directories and continue
+Purpose: Ensure one inaccessible subdirectory does not abort an otherwise valid recursive scan.
+Setup: Create a readable subtree and a second subtree whose directory permissions are removed (non-Windows only).
+Procedure: Scan with `Files | Recursive`.
+Expected outcome: The readable file is returned, the scan does not throw, and the unreadable subtree is skipped.
+Run: `./build-tests/ai_file_sorter_tests "recursive scans skip unreadable directories and continue"`
+
 ### `tests/unit/test_support_prompt.cpp`
 
 #### Test case: Support prompt thresholds advance based on response
diff --git a/tests/unit/test_file_scanner.cpp b/tests/unit/test_file_scanner.cpp
index 0ba3903..c4ee395 100644
--- a/tests/unit/test_file_scanner.cpp
+++ b/tests/unit/test_file_scanner.cpp
@@ -1,10 +1,13 @@
 #include <catch2/catch_test_macros.hpp>
 #include "FileScanner.hpp"
 #include "TestHelpers.hpp"
+#include <algorithm>
 #include <fstream>
 #include <filesystem>
 #ifdef _WIN32
 #include <windows.h>
+#else
+#include <unistd.h>
 #endif
 
 static void write_file(const std::filesystem::path& path) {
```

This second excerpt is included because `a0105bb` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
