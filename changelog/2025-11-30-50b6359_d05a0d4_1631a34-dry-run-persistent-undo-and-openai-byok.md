# 2025-11-30: Dry-run preview, persistent undo, and bring-your-own OpenAI key

## Covered commits
- `50b6359` `2025-11-30` `feat: add dry-run preview and persistent undo`
- `d05a0d4` `2025-11-30` `feat(remote-llm): you can use your own OpenAI key now`
- `1631a34` `2025-11-30` `chore(remote-llm): add API key URL`
- `867e0bc` `2025-11-30` `fix(main-window-file-view): auto-expand columns`
- `b053c3e` `2025-11-23` `fix(llm-selection-dialog): remote LLM selection on Linux - fix the OK button inactivity on selection`
- `a743600` `2025-11-23` `fix(llm-selection-dialog): remote LLM selection on Linux - fix the OK button inactivity on selection`
- `a2603d6` `2025-11-24` `fix(startapp-windows): improve for Vulkan DLLs`

## Motivation
Once people started trusting the sorter for larger folders, the product needed a stronger safety story and a cleaner remote-model story. Users wanted a preview mode before any move happened, an undo record that survived dialog closure, and the ability to use their own OpenAI key rather than rely on bundled credentials.

## What changed
The grouped commits added the dry-run review table, persistent undo state, BYOK OpenAI support and settings wiring, automatic file-view column sizing, plus follow-up fixes for Linux remote-LLM selection and Windows Vulkan launcher behavior in the same product phase.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `50b6359`
```diff
diff --git a/app/include/CategorizationDialog.hpp b/app/include/CategorizationDialog.hpp
--- a/app/include/CategorizationDialog.hpp
+++ b/app/include/CategorizationDialog.hpp
@@ -7,6 +7,7 @@
 #include <QStandardItemModel>
 
 #include <memory>
+#include <optional>
 #include <tuple>
 #include <vector>
 #include <spdlog/logger.h>
@@ -24,6 +25,7 @@ class CategorizationDialog : public QDialog
 public:
     CategorizationDialog(DatabaseManager* db_manager,
                          bool show_subcategory_col,
+                         const std::string& undo_dir,
                          QWidget* parent = nullptr);
 
     void set_show_subcategory_column(bool enabled);
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `d05a0d4`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -61,8 +61,8 @@ File content–based sorting for certain file types is also in development.
     - [macOS](#macos)
     - [Windows](#windows)
   - [Uninstallation](#uninstallation)
+  - [Using your OpenAI API key](#using-your-openai-api-key)
   - [Testing](#testing)
-  - [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption)
   - [How to Use](#how-to-use)
   - [Sorting a Remote Directory (e.g., NAS)](#sorting-a-remote-directory-eg-nas)
   - [Contributing](#contributing)
@@ -86,7 +86,7 @@ See [CHANGELOG.md](CHANGELOG.md) for the full history.
 
 ## Features
 
-- **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (LLaMa, Mistral) or a remote LLM (ChatGPT), depending on your preference.
+- **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (LLaMa, Mistral) or ChatGPT with your own OpenAI API key (choose any ChatGPT model your key allows).
 - **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
   **Robust Categorization Algorithm**: Consistency across categories is supported by taxonomy and heuristics.
   **Customizable Sorting Rules**: Automatically assign categories and subcategories for granular organization.
```

This second excerpt is included because `d05a0d4` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
