# 2026-02-01: chore(ui): clarify recursive scan label

## Covered commits
- `d2ccba6` `2026-02-01` `chore(ui): clarify recursive scan label`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/TranslationManager.cpp`
- `M` `app/lib/TranslationManager.cpp.bak`
- `M` `app/lib/UiTranslator.cpp`
- `M` `app/resources/i18n/aifilesorter_de.ts`
- `M` `app/resources/i18n/aifilesorter_es.ts`
- `M` `app/resources/i18n/aifilesorter_fr.ts`
- `M` `app/resources/i18n/aifilesorter_it.ts`
- `M` `app/resources/i18n/aifilesorter_nl.ts`
- `M` `app/resources/i18n/aifilesorter_tr.ts`
- `M` `tests/unit/test_ui_translator.cpp`

## What changed from what, why, and how
The commit updated test-related files in `app/lib/TranslationManager.cpp`, `app/lib/TranslationManager.cpp.bak`, `app/lib/UiTranslator.cpp`, `app/resources/i18n/aifilesorter_de.ts`, `app/resources/i18n/aifilesorter_es.ts`, `app/resources/i18n/aifilesorter_fr.ts`, `app/resources/i18n/aifilesorter_it.ts`, `app/resources/i18n/aifilesorter_nl.ts`, and 2 more file(s). It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `d2ccba6`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/TranslationManager.cpp b/app/lib/TranslationManager.cpp
--- a/app/lib/TranslationManager.cpp
+++ b/app/lib/TranslationManager.cpp
@@ -30,8 +30,11 @@ static const QHash<QString, QString> kFrenchTranslations = {
     {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("Impossible de réinitialiser la catégorisation en cache pour ce dossier.")},
     {QStringLiteral("Categorize files"), QStringLiteral("Catégoriser les fichiers")},
     {QStringLiteral("Include files in the categorization pass."), QStringLiteral("Inclure les fichiers dans la catégorisation.")},
-    {QStringLiteral("Categorize directories"), QStringLiteral("Catégoriser les dossiers")},
+    {QStringLiteral("Categorize folders"), QStringLiteral("Catégoriser les dossiers")},
     {QStringLiteral("Include directories in the categorization pass."), QStringLiteral("Inclure les dossiers dans la catégorisation.")},
+    {QStringLiteral("Scan subfolders"), QStringLiteral("Scanner les sous-dossiers")},
+    {QStringLiteral("Scan files inside subfolders and treat them as part of the main folder."),
+     QStringLiteral("Analyser les fichiers dans les sous-dossiers et les traiter comme s'ils étaient dans le dossier principal.")},
     {QStringLiteral("Ready"), QStringLiteral("Prêt")},
     {QStringLiteral("Set folder to %1"), QStringLiteral("Dossier défini sur %1")},
     {QStringLiteral("Loaded folder %1"), QStringLiteral("Dossier chargé %1")},
@@ -213,8 +216,11 @@ static const QHash<QString, QString> kGermanTranslations = {
     {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("Zurücksetzen der zwischengespeicherten Kategorisierung für diesen Ordner fehlgeschlagen.")},
     {QStringLiteral("Categorize files"), QStringLiteral("Dateien kategorisieren")},
     {QStringLiteral("Include files in the categorization pass."), QStringLiteral("Dateien in die Kategorisierung einbeziehen.")},
-    {QStringLiteral("Categorize directories"), QStringLiteral("Ordner kategorisieren")},
+    {QStringLiteral("Categorize folders"), QStringLiteral("Ordner kategorisieren")},
     {QStringLiteral("Include directories in the categorization pass."), QStringLiteral("Ordner in die Kategorisierung einbeziehen.")},
+    {QStringLiteral("Scan subfolders"), QStringLiteral("Unterordner scannen")},
+    {QStringLiteral("Scan files inside subfolders and treat them as part of the main folder."),
+     QStringLiteral("Dateien in Unterordnern scannen und so behandeln, als wären sie im Hauptordner.")},
     {QStringLiteral("Ready"), QStringLiteral("Bereit")},
     {QStringLiteral("Set folder to %1"), QStringLiteral("Ordner auf %1 gesetzt")},
     {QStringLiteral("Loaded folder %1"), QStringLiteral("Ordner %1 geladen")},
```

The excerpt is taken from the commit diff for `chore(ui): clarify recursive scan label`. The most relevant surfaces are `app/lib/TranslationManager.cpp`, `app/lib/TranslationManager.cpp.bak`, `app/lib/UiTranslator.cpp`, `app/resources/i18n/aifilesorter_de.ts`, `app/resources/i18n/aifilesorter_es.ts`, and 5 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
