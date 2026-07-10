# 2026-03-22: Runtime translation migration to ts/qm catalogs and settings UI refresh

## Covered commits
- `f24426c` `2026-03-22` `feat(i18n): migrate runtime translations to ts/qm catalogs`
- `a558cae` `2026-03-22` `feat(ui): unify disclosure toggles and refresh settings menu icons`
- `192e64e` `2026-03-22` `chore(ui): remove llm recommendation badge`
- `b2e1d6a` `2026-03-22` `Revert "chore(ui): remove llm recommendation badge"`

## Motivation
The in-code translation tables were becoming increasingly difficult to scale and maintain. Migrating to Qt `ts`/`qm` catalogs was justified to formalize the translation workflow and reduce drift. In the same UI-refresh window, the settings surface was cleaned up by unifying disclosure toggles and reconsidering the LLM recommendation badge.

## What changed
These commits moved runtime translations to Qt catalog files, refreshed settings/disclosure UI behavior, briefly removed the LLM recommendation badge, and then reverted that badge removal so the release landed on the preferred UI state without dropping the rest of the cleanup.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `f24426c`
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -21,6 +21,26 @@ endif()
 
 # Qt6
 find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
+find_package(Qt6LinguistTools CONFIG QUIET)
+if(NOT Qt6LinguistTools_FOUND)
+    set(_aifs_qttools_hints "")
+    if(DEFINED ENV{HOMEBREW_PREFIX} AND NOT "$ENV{HOMEBREW_PREFIX}" STREQUAL "")
+        list(APPEND _aifs_qttools_hints "$ENV{HOMEBREW_PREFIX}/opt/qttools")
+    endif()
+    list(APPEND _aifs_qttools_hints /opt/homebrew/opt/qttools /usr/local/opt/qttools)
+    if(DEFINED Qt6_DIR AND NOT "${Qt6_DIR}" STREQUAL "")
+        get_filename_component(_aifs_qt6_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
+        list(APPEND _aifs_qttools_hints "${_aifs_qt6_prefix}")
+    endif()
+    list(REMOVE_DUPLICATES _aifs_qttools_hints)
+    foreach(_aifs_qttools_hint IN LISTS _aifs_qttools_hints)
+        if(EXISTS "${_aifs_qttools_hint}/lib/cmake/Qt6LinguistTools/Qt6LinguistToolsConfig.cmake")
+            list(APPEND CMAKE_PREFIX_PATH "${_aifs_qttools_hint}")
+        endif()
+    endforeach()
+    list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
+    find_package(Qt6LinguistTools CONFIG REQUIRED)
+endif()
 
 # Third-party deps (resolved best via vcpkg on Windows)
 find_package(CURL REQUIRED)
@@ -327,7 +347,20 @@ set(APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
 file(GLOB APP_LIB_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp"
 )
+file(GLOB APP_HEADER_SOURCES CONFIGURE_DEPENDS
+    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
+)
 set(APP_SOURCES ${APP_MAIN_SOURCE} ${APP_LIB_SOURCES})
+set(AIFS_TRANSLATION_TS_FILES
+    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_fr.ts"
+    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_de.ts"
+    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_it.ts"
+    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_es.ts"
+    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_nl.ts"
+    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_tr.ts"
+    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_ko.ts"
+)
+set(AIFS_TRANSLATION_QM_DIR "${CMAKE_CURRENT_BINARY_DIR}/i18n")
 
 # Executable (GUI subsystem on Windows)
 if(WIN32)
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `a558cae`
```diff
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -477,12 +477,8 @@ void sync_disclosure_button(QToolButton* button, bool expanded)
     if (!button) {
         return;
     }
-#if defined(Q_OS_MACOS)
     Q_UNUSED(expanded);
     button->update();
-#else
-    button->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
-#endif
 }
 
 } // namespace
diff --git a/app/lib/MainAppUiBuilder.cpp b/app/lib/MainAppUiBuilder.cpp
index 25f477f..36223ee 100644
--- a/app/lib/MainAppUiBuilder.cpp
+++ b/app/lib/MainAppUiBuilder.cpp
@@ -21,6 +21,7 @@
 #include <QHeaderView>
 #include <QHBoxLayout>
 #include <QIcon>
+#include <QLinearGradient>
 #include <QComboBox>
 #include <QFontMetrics>
 #include <QKeySequence>
```

This second excerpt is included because `a558cae` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
