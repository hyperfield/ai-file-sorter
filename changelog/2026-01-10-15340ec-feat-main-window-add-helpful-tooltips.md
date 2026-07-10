# 2026-01-10: feat(main-window): add helpful tooltips

## Covered commits
- `15340ec` `2026-01-10` `feat(main-window): add helpful tooltips`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/TranslationManager.cpp`
- `M` `app/lib/UiTranslator.cpp`
- `M` `app/resources/i18n/aifilesorter_de.ts`
- `M` `app/resources/i18n/aifilesorter_es.ts`
- `M` `app/resources/i18n/aifilesorter_fr.ts`
- `M` `app/resources/i18n/aifilesorter_it.ts`
- `M` `app/resources/i18n/aifilesorter_tr.ts`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/lib/TranslationManager.cpp`, `app/lib/UiTranslator.cpp`, `app/resources/i18n/aifilesorter_de.ts`, `app/resources/i18n/aifilesorter_es.ts`, `app/resources/i18n/aifilesorter_fr.ts`, `app/resources/i18n/aifilesorter_it.ts`, `app/resources/i18n/aifilesorter_tr.ts`. It changed the project from not having the capability described by `feat(main-window): add helpful tooltips` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `15340ec`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/TranslationManager.cpp b/app/lib/TranslationManager.cpp
--- a/app/lib/TranslationManager.cpp
+++ b/app/lib/TranslationManager.cpp
@@ -12,10 +12,16 @@ static const QHash<QString, QString> kFrenchTranslations = {
     {QStringLiteral("Analyze folder"), QStringLiteral("Analyser le dossier")},
     {QStringLiteral("Stop analyzing"), QStringLiteral("Arrêter l'analyse")},
     {QStringLiteral("Use subcategories"), QStringLiteral("Utiliser les sous-catégories")},
+    {QStringLiteral("Create subcategory folders within each category."), QStringLiteral("Créer des sous-dossiers dans chaque catégorie.")},
     {QStringLiteral("Categorization type"), QStringLiteral("Type de catégorisation")},
+    {QStringLiteral("Choose how strict the category labels should be."), QStringLiteral("Choisir le niveau de précision des libellés de catégorie.")},
     {QStringLiteral("More refined"), QStringLiteral("Plus précis")},
+    {QStringLiteral("Favor detailed labels even if similar items vary."), QStringLiteral("Privilégie des libellés détaillés même si les éléments similaires varient.")},
     {QStringLiteral("More consistent"), QStringLiteral("Plus cohérent")},
+    {QStringLiteral("Favor consistent labels across similar items."), QStringLiteral("Privilégie des libellés cohérents pour les éléments similaires.")},
     {QStringLiteral("Use a whitelist"), QStringLiteral("Utiliser une liste blanche")},
+    {QStringLiteral("Restrict categories and subcategories to the selected whitelist."), QStringLiteral("Limiter les catégories et sous-catégories à la liste blanche sélectionnée.")},
+    {QStringLiteral("Select the whitelist used for this run."), QStringLiteral("Sélectionner la liste blanche utilisée pour cette analyse.")},
     {QStringLiteral("Recategorize folder?"), QStringLiteral("Recatégoriser le dossier ?")},
     {QStringLiteral("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?"),
      QStringLiteral("Ce dossier a été catégorisé en mode %1. Voulez-vous le recatégoriser maintenant en mode %2 ?")},
@@ -23,7 +29,9 @@ static const QHash<QString, QString> kFrenchTranslations = {
     {QStringLiteral("Keep existing"), QStringLiteral("Conserver l'existant")},
     {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("Impossible de réinitialiser la catégorisation en cache pour ce dossier.")},
     {QStringLiteral("Categorize files"), QStringLiteral("Catégoriser les fichiers")},
+    {QStringLiteral("Include files in the categorization pass."), QStringLiteral("Inclure les fichiers dans la catégorisation.")},
     {QStringLiteral("Categorize directories"), QStringLiteral("Catégoriser les dossiers")},
+    {QStringLiteral("Include directories in the categorization pass."), QStringLiteral("Inclure les dossiers dans la catégorisation.")},
     {QStringLiteral("Ready"), QStringLiteral("Prêt")},
     {QStringLiteral("Set folder to %1"), QStringLiteral("Dossier défini sur %1")},
     {QStringLiteral("Loaded folder %1"), QStringLiteral("Dossier chargé %1")},
```

The excerpt is taken from the commit diff for `feat(main-window): add helpful tooltips`. The most relevant surfaces are `app/lib/TranslationManager.cpp`, `app/lib/UiTranslator.cpp`, `app/resources/i18n/aifilesorter_de.ts`, `app/resources/i18n/aifilesorter_es.ts`, `app/resources/i18n/aifilesorter_fr.ts`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
