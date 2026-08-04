# 2025-11-30: Update version and more

## Covered commits
- `7cea734` `2025-11-30` `Update version and more`

## Motivation
This commit synchronized version metadata so release identifiers, packaged artifacts, and documentation would describe the same build. Version drift is small in diff size but high impact for packaging and support.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/CMakeLists.txt`
- `M` `app/build_windows.ps1`
- `M` `app/include/app_version.hpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `A` `app/lib/TranslationManager.cpp.bak`
- `M` `app/main.cpp`
- `A` `app/resources/windows/version.rc.in`
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit adjusted version-bearing files in `app/CMakeLists.txt`, `app/build_windows.ps1`, `app/include/app_version.hpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/TranslationManager.cpp.bak`, `app/main.cpp`, `app/resources/windows/version.rc.in`, `app/startapp_windows.cpp`. It moved the repository from the previous release identifier to the newer one required for the next build or distribution step.

Before this commit, the repository reflected the state immediately preceding `7cea734`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -47,9 +47,14 @@ file(GLOB APP_LIB_SOURCES
 )
 set(APP_SOURCES ${APP_MAIN_SOURCE} ${APP_LIB_SOURCES})
 
-# Executable (GUI on Windows)
+# Executable (console opt-in on Windows)
 if(WIN32)
-    add_executable(aifilesorter WIN32 ${APP_SOURCES})
+    option(AI_FILE_SORTER_CONSOLE "Build main executable as console (attach stdout/stderr)" OFF)
+    if(AI_FILE_SORTER_CONSOLE)
+        add_executable(aifilesorter ${APP_SOURCES})
+    else()
+        add_executable(aifilesorter WIN32 ${APP_SOURCES})
+    endif()
 else()
     add_executable(aifilesorter ${APP_SOURCES})
 endif()
@@ -171,13 +176,15 @@ if(WIN32)
 endif()
 
 if(WIN32)
-    add_executable(StartAiFileSorter WIN32
+    # Build the starter as a console subsystem so stdout/stderr stay attached for CLI debugging.
+    set(STARTER_TARGET StartAiFileSorter)
+    add_executable(${STARTER_TARGET}
         "${CMAKE_CURRENT_SOURCE_DIR}/startapp_windows.cpp"
     )
-    target_link_libraries(StartAiFileSorter PRIVATE
+    target_link_libraries(${STARTER_TARGET} PRIVATE
         Qt6::Widgets Qt6::Gui Qt6::Core
     )
-    target_compile_definitions(StartAiFileSorter PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
+    target_compile_definitions(${STARTER_TARGET} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
 endif()
 
 # On Windows, ensure Unicode and lean Windows headers in deps that need it
```

The excerpt is taken from the commit diff for `Update version and more`. The most relevant surfaces are `app/CMakeLists.txt`, `app/build_windows.ps1`, `app/include/app_version.hpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/TranslationManager.cpp.bak`, and 3 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
