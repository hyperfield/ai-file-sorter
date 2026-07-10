# 2026-01-24: docs(changelog): log Korean UI + build/test fixes

## Covered commits
- `9b9b571` `2026-01-24` `docs(changelog): log Korean UI + build/test fixes`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `changelog/2026-01-24-korean-ui-language-tests-build-fix.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `changelog/2026-01-24-korean-ui-language-tests-build-fix.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `9b9b571`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/changelog/2026-01-24-korean-ui-language-tests-build-fix.md b/changelog/2026-01-24-korean-ui-language-tests-build-fix.md
--- /dev/null
+++ b/changelog/2026-01-24-korean-ui-language-tests-build-fix.md
@@ -0,0 +1,180 @@
+# 2026-01-24 - Korean UI language, translation test coverage, mtmd CMake fix, language menu ordering
+
+## Motivation
+
+- **Korean UI support**: Users requested Korean as a selectable interface language. The app already supports multiple UI locales, so extending the existing translation mechanism is the most consistent path.
+- **Test coverage for UI languages**: We added a new language, but the translation tests only validated English/French. Expanding the test to cover *all* UI languages reduces regressions in translation tables and ensures the menu selection logic remains correct.
+- **CMake mtmd configure error**: The test build failed because the separately-added `mtmd` subdirectory didn’t inherit `LLAMA_INSTALL_VERSION`. That variable is needed for `set_target_properties(... VERSION ...)`. We fixed this at the app-level CMake without modifying the llama.cpp submodule.
+- **Language menu ordering**: After adding Korean, the Interface Language menu wasn’t alphabetically sorted; this is a UX polish item that avoids confusion when scanning the list.
+
+## What changed (high level)
+
+1. Added **Korean** to the UI language enum and conversion helpers.
+2. Added **Korean translations** (full translation table) and included Korean in the available language list.
+3. Updated the **language menu** to insert Korean alphabetically.
+4. Extended **UI translation tests** to validate *all* UI languages.
+5. Fixed **mtmd build configuration** by passing the llama.cpp install version into the mtmd subdirectory.
+
+## Detailed changes and rationale
+
+### 1) Language enum + string conversion
+
+**Why**: The UI language selector is driven by the `Language` enum and the `languageToString`/`languageFromString` helpers. We need Korean in all three places to make it selectable and persistent in settings.
+
+**Excerpt** (`app/include/Language.hpp`):
+
+```cpp
+// Added Korean to the enum so it can be selected and persisted.
+enum class Language {
+    English,
+    French,
+    German,
+    Italian,
+    Spanish,
+    Turkish,
+    Korean,
+    Dutch
+};
+
+inline QString languageToString(Language language)
+{
+    switch (language) {
+    // ... existing cases ...
+    case Language::Korean:
+        return QStringLiteral("Korean");
+    // ...
+    }
+}
+
+inline Language languageFromString(const QString& value)
+{
+    const QString lowered = value.toLower();
+    // ... existing cases ...
+    if (lowered == QStringLiteral("korean") || lowered == QStringLiteral("ko")) {
+        return Language::Korean;
+    }
+    return Language::English;
+}
+```
+
+### 2) System locale default
+
+**Why**: We map the OS locale to a default UI language in `Settings`. Adding Korean lets the app default to Korean on Korean systems.
+
+**Excerpt** (`app/lib/Settings.cpp`):
+
+```cpp
+switch (QLocale::system().language()) {
+    // ... existing cases ...
+    case QLocale::Korean: return Language::Korean;
+    // ...
+}
+```
+
+### 3) Translation manager updates (Korean map + menu labels)
+
+**Why**: The translation system is static and dictionary-based, so Korean needs its own translation map and must be listed as an available UI language. We also added `&Korean` to the other language maps so the “Interface language” submenu shows the translated label consistently across locales.
+
+**Excerpt** (`app/lib/TranslationManager.cpp`):
+
+```cpp
+// New Korean translation table (sample)
+static const QHash<QString, QString> kKoreanTranslations = {
+    {QStringLiteral("Analyze folder"), QStringLiteral("폴더 분석")},
+    {QStringLiteral("Folder:"), QStringLiteral("폴더:")},
+    {QStringLiteral("Interface &language"), QStringLiteral("인터페이스 &언어")},
+    {QStringLiteral("&Korean"), QStringLiteral("&한국어")},
+    // ... full table continues ...
+};
+
+const QHash<QString, QString>* translations_for(Language lang)
+{
+    switch (lang) {
+    // ... existing cases ...
+    case Language::Korean: return &kKoreanTranslations;
+    default: return nullptr;
+    }
```

The excerpt is taken from the commit diff for `docs(changelog): log Korean UI + build/test fixes`. The most relevant surfaces are `changelog/2026-01-24-korean-ui-language-tests-build-fix.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
