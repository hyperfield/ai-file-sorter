# 2025-12-23: Arch Linux Makefile compatibility and Fedora documentation follow-up

## Covered commits
- `2601288` `2025-12-23` `fix(make): fix compile under Arch Linux, improve Makefile overall`
- `e95617c` `2025-12-23` `chore(version): update`
- `8e65ce9` `2025-12-31` `chore(readme): update for Fedora Linux`

## Motivation
This chapter exists because the project was being built on more distributions than the original setup had assumed. The immediate problem was getting the Makefile into a shape that compiled cleanly on Arch Linux, then making sure the versioned docs reflected the expanded distro support instead of lagging behind the code.

## What changed
The fix commit reworked Makefile assumptions for Arch while the follow-up version and README updates made sure downstream build instructions stayed synchronized with the actual supported Linux environment set.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `2601288`
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -117,6 +117,7 @@ build-windows/
 build-tests/
 build/
 dist/
+app/lib/ggml/
 
 # End of https://www.toptal.com/developers/gitignore/api/c++,visualstudiocode
 
diff --git a/app/Makefile b/app/Makefile
index dc3893b..25fac6c 100644
--- a/app/Makefile
+++ b/app/Makefile
@@ -15,10 +15,23 @@ ifeq ($(UNAME), Linux)
 	LD_CONF_FILE := /etc/ld.so.conf.d/aifilesorter.conf
 
     # --- Qt6 setup ---
+    PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)
+    QT_PACKAGES := Qt6Widgets Qt6Gui Qt6Core
+    QT_CXXFLAGS :=
+    QT_LDFLAGS :=
+ifneq ($(strip $(PKG_CONFIG)),)
+    QT_CXXFLAGS := $(shell $(PKG_CONFIG) --cflags $(QT_PACKAGES) 2>/dev/null)
+    QT_LDFLAGS := $(shell $(PKG_CONFIG) --libs $(QT_PACKAGES) 2>/dev/null)
+endif
+ifeq ($(strip $(QT_CXXFLAGS)),)
     QT_INCLUDE_BASE ?= /usr/include/x86_64-linux-gnu/qt6
     QT_LIB_BASE     ?= /usr/lib/x86_64-linux-gnu
     CXXFLAGS += -I$(QT_INCLUDE_BASE) -I$(QT_INCLUDE_BASE)/QtCore -I$(QT_INCLUDE_BASE)/QtGui -I$(QT_INCLUDE_BASE)/QtWidgets
     LDFLAGS  += -L$(QT_LIB_BASE) -lQt6Widgets -lQt6Gui -lQt6Core
+else
+    CXXFLAGS += $(QT_CXXFLAGS)
+    LDFLAGS  += $(QT_LDFLAGS)
+endif
 
     # --- Other dependencies ---
 	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lggml-base -lX11 -pthread
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `e95617c`
```diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -1,5 +1,9 @@
 ﻿# Changelog
 
+## [1.4.5] - 2025-12-05
+- Added support for Gemini (a remote LLM) - with your own Gemini API key.
+- Fixed compile under Arch Linux.
+
 ## [1.4.0] - 2025-12-05
 - Added dry run / preview-only mode with From/To table, no moves performed until you uncheck.
 - Persistent Undo: the latest sort saves a plan file; use Edit -> "Undo last run" even after closing dialogs.
diff --git a/README.md b/README.md
index dc9dfa0..fb7774f 100644
--- a/README.md
+++ b/README.md
@@ -1,7 +1,8 @@
 <!-- markdownlint-disable MD046 -->
 # AI File Sorter
 
-[![Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter)](#)
+[![Code Version](https://img.shields.io/badge/Code-1.4.5-blue)](#)
+[![Release Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter?label=Release)](#)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 [![SourceForge Downloads](https://img.shields.io/sourceforge/dw/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
 [![Codacy Badge](https://app.codacy.com/project/badge/Grade/2c646c836a9844be964fbf681649c3cd)](https://app.codacy.com/gh/hyperfield/ai-file-sorter/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
```

This second excerpt is included because `e95617c` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
