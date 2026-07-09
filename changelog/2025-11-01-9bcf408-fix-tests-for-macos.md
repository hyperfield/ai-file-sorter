# 2025-11-01: fix(tests): for macOS

## Covered commits
- `9bcf408` `2025-11-01` `fix(tests): for macOS`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `tests/run_translation_tests.sh`

## What changed from what, why, and how
The commit corrected behavior in `tests/run_translation_tests.sh`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(tests): for macOS`.

Before this commit, the repository reflected the state immediately preceding `9bcf408`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/tests/run_translation_tests.sh b/tests/run_translation_tests.sh
--- a/tests/run_translation_tests.sh
+++ b/tests/run_translation_tests.sh
@@ -37,19 +37,60 @@ CPP
 
 OUTPUT="$BUILD_DIR/translation_manager_test"
 
-QT_INCLUDES=(
-    -I"$ROOT_DIR/app/include"
-    -I/usr/include/x86_64-linux-gnu/qt6
-    -I/usr/include/x86_64-linux-gnu/qt6/QtCore
-    -I/usr/include/x86_64-linux-gnu/qt6/QtGui
-    -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets
-)
-
-QT_LIBS=(
-    -lQt6Core
-    -lQt6Gui
-    -lQt6Widgets
-)
-
-g++ -std=c++20 -fPIC "${QT_INCLUDES[@]}" "$SRC" "$ROOT_DIR/app/lib/TranslationManager.cpp" -o "$OUTPUT" "${QT_LIBS[@]}"
+pkg_includes="$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets 2>/dev/null || true)"
+pkg_libs="$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets 2>/dev/null || true)"
+
+if [[ -n "$pkg_includes" && -n "$pkg_libs" ]]; then
+    QT_FLAGS="$pkg_includes -I$ROOT_DIR/app/include"
+    QT_LIB_FLAGS="$pkg_libs"
+else
+    qt_headers=""
+    qt_libs=""
+    if command -v qmake6 >/dev/null 2>&1; then
+        qt_headers="$(qmake6 -query QT_INSTALL_HEADERS 2>/dev/null || true)"
+        qt_libs="$(qmake6 -query QT_INSTALL_LIBS 2>/dev/null || true)"
+    elif command -v qmake >/dev/null 2>&1; then
+        qt_headers="$(qmake -query QT_INSTALL_HEADERS 2>/dev/null || true)"
+        qt_libs="$(qmake -query QT_INSTALL_LIBS 2>/dev/null || true)"
+    elif command -v qtpaths6 >/dev/null 2>&1; then
+        prefix="$(qtpaths6 --install-prefix 2>/dev/null || true)"
+        if [[ -n "$prefix" ]]; then
+            qt_headers="$prefix/include"
+            qt_libs="$prefix/lib"
+        fi
+    elif command -v brew >/dev/null 2>&1; then
+        prefix="$(brew --prefix qt 2>/dev/null || brew --prefix qt6 2>/dev/null || true)"
+        if [[ -n "$prefix" ]]; then
+            qt_headers="$prefix/include"
+            qt_libs="$prefix/lib"
+        fi
+    fi
+
+    if [[ -n "$qt_headers" ]]; then
+        QT_FLAGS="-I$ROOT_DIR/app/include -I$qt_headers -I$qt_headers/QtCore -I$qt_headers/QtGui -I$qt_headers/QtWidgets"
+        if [[ -n "$qt_libs" ]]; then
+            for fw in QtCore QtGui QtWidgets; do
+                fw_headers="$qt_libs/$fw.framework/Headers"
+                if [[ -d "$fw_headers" ]]; then
+                    QT_FLAGS="$QT_FLAGS -I$fw_headers"
+                fi
+            done
+        fi
+    else
+        QT_FLAGS="-I$ROOT_DIR/app/include -I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets -I/opt/homebrew/include -I/opt/homebrew/include/QtCore -I/opt/homebrew/include/QtGui -I/opt/homebrew/include/QtWidgets"
+    fi
+
+    if [[ -n "$qt_libs" ]]; then
+        if [[ -d "$qt_libs/QtCore.framework" ]]; then
+            QT_LIB_FLAGS="-F$qt_libs -framework QtCore -framework QtGui -framework QtWidgets"
+        else
+            QT_LIB_FLAGS="-L$qt_libs -lQt6Core -lQt6Gui -lQt6Widgets"
+        fi
+    else
+        QT_LIB_FLAGS="-L/opt/homebrew/lib -lQt6Core -lQt6Gui -lQt6Widgets"
+    fi
+fi
+
+# shellcheck disable=SC2086
+g++ -std=c++20 -fPIC $QT_FLAGS "$SRC" "$ROOT_DIR/app/lib/TranslationManager.cpp" -o "$OUTPUT" $QT_LIB_FLAGS
 QT_QPA_PLATFORM=offscreen "$OUTPUT"
```

The excerpt is taken from the commit diff for `fix(tests): for macOS`. The most relevant surfaces are `tests/run_translation_tests.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
