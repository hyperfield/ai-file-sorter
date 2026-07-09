# 2026-01-29: Vendored document backends, recursive cache keying, GPU fallback reporting, and category-column gating

## Covered commits
- `277e8e2` `2026-01-29` `chore(doc-deps): add vendor scripts and add Windows downloader`
- `9f6d6dd` `2026-01-29` `build(doc-deps): link vendored libzip/pugixml/pdfium`
- `a2dc32c` `2026-01-29` `feat(doc-analysis): use vendored pdfium/libzip/pugixml`
- `f08d90e` `2026-01-29` `chore(doc-deps): vendor libzip/pugixml/pdfium prebuilts`
- `2e24f52` `2026-01-29` `fix(cache): scope lookups by directory path`
- `ea71ee0` `2026-01-29` `feat(ui): report GPU fallback in progress log`
- `ca26f43` `2026-01-29` `fix(ui): keep category column for non-rename rows`
- `f436057` `2026-01-29` `docs(headers): add doxygen headers`

## Motivation
Document extraction had to become self-contained rather than depending on loosely assumed host tools, and recursive scanning raised cache-keying problems for duplicate filenames in different folders. At the same time, the UI needed to explain GPU fallback more clearly and stop showing irrelevant category columns for rename-only rows.

## What changed
The grouped commits added vendored PDFium/libzip/pugixml support and vendor scripts, switched document analysis to those embedded backends, scoped cache lookups by directory path, reported GPU fallback in progress output, and gated the category column for rows where it no longer made sense.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `277e8e2`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -62,6 +62,9 @@ AI File Sorter runs entirely on your device, using local AI models such as LLaMa
   - [Image analysis (Visual LLM)](#image-analysis-visual-llm)
     - [Required visual LLM files](#required-visual-llm-files)
     - [Main window options](#main-window-options)
+  - [Document analysis (Text LLM)](#document-analysis-text-llm)
+    - [Supported document formats](#supported-document-formats)
+    - [Main window options (documents)](#main-window-options-documents)
   - [Requirements](#requirements)
   - [Installation](#installation)
     - [Linux](#linux)
@@ -109,6 +112,7 @@ See [CHANGELOG.md](CHANGELOG.md) for the full history.
 - **Multilingual categorization**: Have the LLM assign categories in Dutch, French, German, Italian, Polish, Portuguese, Spanish, or Turkish (model dependent).
 - **Custom local LLMs**: Register your own local GGUF models directly from the **Select LLM** dialog.
 - **Image content analysis (Visual LLM)**: Analyze supported picture files with LLaVA to produce descriptions and optional filename suggestions (rename-only mode supported).
+- **Document content analysis (Text LLM)**: Analyze supported document files to summarize content and suggest filenames; uses the same selected LLM (local or remote).
 - **Sortable review**: Sort the Categorization Review table by file name, category, or subcategory to triage faster.
 - **Qt6 Interface**: Lightweight and responsive UI with refreshed menus and icons.
 - **Interface languages**: English, Dutch, French, German, Italian, Korean, Spanish, and Turkish.
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `9f6d6dd`
```diff
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
--- a/app/CMakeLists.txt
+++ b/app/CMakeLists.txt
@@ -43,6 +43,70 @@ find_package(fmt REQUIRED CONFIG)
 find_package(spdlog REQUIRED CONFIG)
 find_package(Intl REQUIRED) # libintl/gettext
 
+# Vendored document analysis deps
+set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external")
+set(PUGIXML_DIR "${EXTERNAL_DIR}/pugixml")
+set(LIBZIP_DIR "${EXTERNAL_DIR}/libzip")
+set(PDFIUM_DIR "${EXTERNAL_DIR}/pdfium")
+set(DOC_DEPS_INCLUDE_DIRS "")
+set(DOC_DEPS_LIBS "")
+
+# Pugixml (compiled via PugixmlBundle.cpp)
+if(EXISTS "${PUGIXML_DIR}/src/pugixml.hpp")
+    list(APPEND DOC_DEPS_INCLUDE_DIRS "${PUGIXML_DIR}/src")
+    add_compile_definitions(PUGIXML_NO_EXCEPTIONS AI_FILE_SORTER_USE_PUGIXML)
+endif()
+
+# libzip (build from vendored source)
+if(EXISTS "${LIBZIP_DIR}/CMakeLists.txt")
+    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
+    set(ENABLE_BZIP2 OFF CACHE BOOL "" FORCE)
+    set(ENABLE_LZMA OFF CACHE BOOL "" FORCE)
+    set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
+    set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
+    set(ENABLE_GNUTLS OFF CACHE BOOL "" FORCE)
+    set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
+    set(ENABLE_COMMONCRYPTO OFF CACHE BOOL "" FORCE)
+    set(ENABLE_WINDOWS_CRYPTO OFF CACHE BOOL "" FORCE)
+    add_subdirectory("${LIBZIP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libzip")
+    if(TARGET libzip::zip)
+        add_compile_definitions(AI_FILE_SORTER_USE_LIBZIP)
+        list(APPEND DOC_DEPS_INCLUDE_DIRS "${LIBZIP_DIR}/lib" "${CMAKE_CURRENT_BINARY_DIR}/libzip")
+        list(APPEND DOC_DEPS_LIBS libzip::zip)
+    endif()
+endif()
+
+if(WIN32)
+    set(PDFIUM_PLATFORM_DIR "${PDFIUM_DIR}/windows-x64")
+    set(PDFIUM_LIBRARY "${PDFIUM_PLATFORM_DIR}/lib/pdfium.dll.lib")
+    set(PDFIUM_RUNTIME "${PDFIUM_PLATFORM_DIR}/bin/pdfium.dll")
+elseif(APPLE)
+    set(PDFIUM_PLATFORM_DIR "${PDFIUM_DIR}/macos-arm64")
+    set(PDFIUM_LIBRARY "${PDFIUM_PLATFORM_DIR}/lib/libpdfium.dylib")
+    set(PDFIUM_RUNTIME "${PDFIUM_LIBRARY}")
+else()
+    set(PDFIUM_PLATFORM_DIR "${PDFIUM_DIR}/linux-x64")
+    set(PDFIUM_LIBRARY "${PDFIUM_PLATFORM_DIR}/lib/libpdfium.so")
+    set(PDFIUM_RUNTIME "${PDFIUM_LIBRARY}")
+endif()
+
+if(EXISTS "${PDFIUM_LIBRARY}")
+    add_compile_definitions(AI_FILE_SORTER_USE_PDFIUM)
+    list(APPEND DOC_DEPS_INCLUDE_DIRS "${PDFIUM_PLATFORM_DIR}/include")
+    add_library(pdfium SHARED IMPORTED)
+    if(WIN32)
+        set_target_properties(pdfium PROPERTIES
+            IMPORTED_IMPLIB "${PDFIUM_LIBRARY}"
+            IMPORTED_LOCATION "${PDFIUM_RUNTIME}"
+        )
+    else()
+        set_target_properties(pdfium PROPERTIES
+            IMPORTED_LOCATION "${PDFIUM_LIBRARY}"
+        )
+    endif()
+    list(APPEND DOC_DEPS_LIBS pdfium)
+endif()
+
 # Sources
 set(APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
 file(GLOB APP_LIB_SOURCES CONFIGURE_DEPENDS
@@ -61,6 +125,7 @@ target_include_directories(aifilesorter PRIVATE
     "${CMAKE_CURRENT_SOURCE_DIR}/include"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/llama"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/tools/mtmd"
+    ${DOC_DEPS_INCLUDE_DIRS}
 )
```

This second excerpt is included because `9f6d6dd` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
