# 2026-03-23: Linux build fixes, translation-test tool resolution, and packaging documentation follow-up

## Covered commits
- `3c1c660` `2026-03-23` `fix(compile): on linux`
- `2a10f49` `2026-03-23` `fix(makefile): fix compile on Linux`
- `9b7e1ad` `2026-03-23` `fix(tests): resolve Qt 6 lrelease explicitly in translation test script`
- `3b26ece` `2026-03-23` `docs(readme): add Debian packaging note`
- `a8b1c9d` `2026-03-23` `chore(docs): update`

## Motivation
The release immediately exposed build-system and test-tooling rough edges. Linux compilation needed one more pass, the translation test script needed to resolve Qt `lrelease` more explicitly across environments, and the packaging docs had to explain the Debian package/dependency story that was now supported by the build scripts.

## What changed
This grouped follow-up chapter covers the Linux compile fixes, the explicit Qt `lrelease` resolution in the translation test script, and the README/documentation updates that clarified Debian packaging and related build expectations.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `3c1c660`
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -420,6 +420,18 @@ endif
 ifndef LRELEASE
 LRELEASE := $(shell command -v lrelease6 2>/dev/null)
 ifeq ($(strip $(LRELEASE)),)
+LRELEASE := $(shell \
+	if command -v qmake6 >/dev/null 2>&1; then \
+		qt_host_bins="$$(qmake6 -query QT_HOST_BINS 2>/dev/null || qmake6 -query QT_INSTALL_BINS 2>/dev/null)"; \
+		if [ -n "$$qt_host_bins" ] && [ -x "$$qt_host_bins/lrelease" ]; then \
+			printf '%s\n' "$$qt_host_bins/lrelease"; \
+		fi; \
+	fi)
+endif
+ifeq ($(strip $(LRELEASE)),)
+LRELEASE := $(wildcard /usr/lib/qt6/bin/lrelease)
+endif
+ifeq ($(strip $(LRELEASE)),)
 LRELEASE := $(shell command -v lrelease 2>/dev/null)
 endif
 ifeq ($(strip $(LRELEASE)),)
diff --git a/tests/run_translation_tests.sh b/tests/run_translation_tests.sh
index 528f2fb..7070635 100755
--- a/tests/run_translation_tests.sh
+++ b/tests/run_translation_tests.sh
@@ -99,7 +99,20 @@ else
     fi
 fi
 
-LRELEASE="$(command -v lrelease6 2>/dev/null || command -v lrelease 2>/dev/null || true)"
+LRELEASE=""
+if command -v lrelease6 >/dev/null 2>&1; then
+    LRELEASE="$(command -v lrelease6)"
+elif command -v qmake6 >/dev/null 2>&1; then
+    qt_host_bins="$(qmake6 -query QT_HOST_BINS 2>/dev/null || qmake6 -query QT_INSTALL_BINS 2>/dev/null || true)"
+    if [[ -n "$qt_host_bins" && -x "$qt_host_bins/lrelease" ]]; then
+        LRELEASE="$qt_host_bins/lrelease"
+    fi
+elif [[ -x /usr/lib/qt6/bin/lrelease ]]; then
+    LRELEASE="/usr/lib/qt6/bin/lrelease"
+elif command -v lrelease >/dev/null 2>&1; then
+    LRELEASE="$(command -v lrelease)"
+fi
+
 if [[ -z "$LRELEASE" ]]; then
     echo "Could not find lrelease or lrelease6" >&2
     exit 1
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `2a10f49`
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -389,6 +389,18 @@ QM_FILES := $(patsubst $(TS_DIR)/%.ts,$(QM_DIR)/%.qm,$(TS_FILES))
 TRANSLATIONS_QRC := $(OBJ_DIR)/translations.qrc
 TRANSLATIONS_QRC_CPP := $(OBJ_DIR)/qrc_translations.cpp
 TRANSLATIONS_QRC_OBJ := $(OBJ_DIR)/qrc_translations.o
+QMAKE6 := $(shell \
+	if command -v qmake6 >/dev/null 2>&1; then \
+		command -v qmake6; \
+	elif [ -x /usr/lib/qt6/bin/qmake6 ]; then \
+		printf '%s\n' /usr/lib/qt6/bin/qmake6; \
+	fi)
+QTPATHS6 := $(shell \
+	if command -v qtpaths6 >/dev/null 2>&1; then \
+		command -v qtpaths6; \
+	elif [ -x /usr/lib/qt6/bin/qtpaths6 ]; then \
+		printf '%s\n' /usr/lib/qt6/bin/qtpaths6; \
+	fi)
 
 ifndef RCC
 RCC := $(shell command -v qt6-rcc 2>/dev/null)
@@ -418,29 +430,51 @@ endif
 endif
 
 ifndef LRELEASE
-LRELEASE := $(shell command -v lrelease6 2>/dev/null)
-ifeq ($(strip $(LRELEASE)),)
 LRELEASE := $(shell \
-	if command -v qmake6 >/dev/null 2>&1; then \
-		qt_host_bins="$$(qmake6 -query QT_HOST_BINS 2>/dev/null || qmake6 -query QT_INSTALL_BINS 2>/dev/null)"; \
-		if [ -n "$$qt_host_bins" ] && [ -x "$$qt_host_bins/lrelease" ]; then \
-			printf '%s\n' "$$qt_host_bins/lrelease"; \
+	for candidate in lrelease6 lrelease-qt6; do \
+		if command -v "$$candidate" >/dev/null 2>&1; then \
+			command -v "$$candidate"; \
+			exit 0; \
+		fi; \
+	done; \
+	if [ -n "$(QTPATHS6)" ]; then \
+		for qt_host_dir in "$$($(QTPATHS6) --query QT_HOST_LIBEXECS 2>/dev/null)" "$$($(QTPATHS6) --query QT_HOST_BINS 2>/dev/null)"; do \
+			if [ -n "$$qt_host_dir" ] && [ -x "$$qt_host_dir/lrelease" ]; then \
+				printf '%s\n' "$$qt_host_dir/lrelease"; \
+				exit 0; \
+			fi; \
+		done; \
+	fi; \
+	if [ -n "$(QMAKE6)" ]; then \
+		for qt_host_dir in "$$($(QMAKE6) -query QT_HOST_LIBEXECS 2>/dev/null)" "$$($(QMAKE6) -query QT_HOST_BINS 2>/dev/null)" "$$($(QMAKE6) -query QT_INSTALL_BINS 2>/dev/null)"; do \
+			if [ -n "$$qt_host_dir" ] && [ -x "$$qt_host_dir/lrelease" ]; then \
+				printf '%s\n' "$$qt_host_dir/lrelease"; \
+				exit 0; \
+			fi; \
+		done; \
+	fi; \
+	for candidate in \
+		/usr/lib/qt6/libexec/lrelease \
+		/usr/lib/qt6/bin/lrelease \
+		/opt/homebrew/bin/lrelease \
+		/opt/homebrew/opt/qt/share/qt/libexec/lrelease \
+		/opt/homebrew/opt/qtbase/share/qt/libexec/lrelease \
+		/usr/local/opt/qtbase/share/qt/libexec/lrelease; do \
+		if [ -x "$$candidate" ]; then \
+			printf '%s\n' "$$candidate"; \
+			exit 0; \
+		fi; \
+	done; \
+	if command -v lrelease >/dev/null 2>&1; then \
+		candidate="$$(command -v lrelease)"; \
+		if "$$candidate" -version 2>&1 | grep -Eq 'Qt[^0-9]*6|version 6'; then \
+			printf '%s\n' "$$candidate"; \
 		fi; \
 	fi)
 endif
 ifeq ($(strip $(LRELEASE)),)
-LRELEASE := $(wildcard /usr/lib/qt6/bin/lrelease)
-endif
-ifeq ($(strip $(LRELEASE)),)
-LRELEASE := $(shell command -v lrelease 2>/dev/null)
-endif
```

This second excerpt is included because `2a10f49` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
