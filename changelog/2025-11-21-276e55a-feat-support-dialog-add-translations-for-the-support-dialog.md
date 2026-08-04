# 2025-11-21: feat(support-dialog): add translations for the support dialog

## Covered commits
- `276e55a` `2025-11-21` `feat(support-dialog): add translations for the support dialog`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/TranslationManager.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/lib/TranslationManager.cpp`. It changed the project from not having the capability described by `feat(support-dialog): add translations for the support dialog` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `276e55a`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/TranslationManager.cpp b/app/lib/TranslationManager.cpp
--- a/app/lib/TranslationManager.cpp
+++ b/app/lib/TranslationManager.cpp
@@ -69,6 +69,16 @@ static const QHash<QString, QString> kFrenchTranslations = {
     {QStringLiteral("About &Qt"), QStringLiteral("À propos de &Qt")},
     {QStringLiteral("About &AGPL"), QStringLiteral("À propos de l'&AGPL")},
     {QStringLiteral("&Support Project"), QStringLiteral("&Soutenir le projet")},
+    {QStringLiteral("Support AI File Sorter"), QStringLiteral("Soutenir AI File Sorter")},
+    {QStringLiteral("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you."),
+     QStringLiteral("Merci d'utiliser AI File Sorter ! Vous avez déjà catégorisé %1 fichiers. Moi, l'auteur, j'espère vraiment que cette application vous a été utile.")},
+    {QStringLiteral("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
+                    "If the app saves you time or brings value, please consider supporting it so it can keep improving."),
+     QStringLiteral("AI File Sorter demande des centaines d'heures de développement, de nouvelles fonctionnalités, de réponses au support et des coûts permanents comme les serveurs ou l'infrastructure des modèles distants. "
+                    "Si l'application vous fait gagner du temps ou vous apporte de la valeur, merci d'envisager un soutien pour qu'elle puisse continuer à s'améliorer.")},
+    {QStringLiteral("Support"), QStringLiteral("Soutenir")},
+    {QStringLiteral("I'm not yet sure"), QStringLiteral("Je ne suis pas encore sûr")},
+    {QStringLiteral("I cannot donate"), QStringLiteral("Je ne peux pas faire de don")},
     {QStringLiteral("About the AGPL License"), QStringLiteral("À propos de la licence AGPL")},
     {QStringLiteral("AI File Sorter is distributed under the GNU Affero General Public License v3.0."
                     "<br><br>"
@@ -161,6 +171,16 @@ static const QHash<QString, QString> kGermanTranslations = {
     {QStringLiteral("About &Qt"), QStringLiteral("Über &Qt")},
     {QStringLiteral("About &AGPL"), QStringLiteral("Über &AGPL")},
     {QStringLiteral("&Support Project"), QStringLiteral("Projekt unterstützen")},
+    {QStringLiteral("Support AI File Sorter"), QStringLiteral("Unterstütze AI File Sorter")},
+    {QStringLiteral("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you."),
+     QStringLiteral("Vielen Dank, dass du AI File Sorter verwendest! Du hast bisher %1 Dateien kategorisiert. Ich, der Autor, hoffe wirklich, dass dir die App geholfen hat.")},
+    {QStringLiteral("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
+                    "If the app saves you time or brings value, please consider supporting it so it can keep improving."),
+     QStringLiteral("AI File Sorter erfordert hunderte Stunden Entwicklung, Funktionsarbeit, Support-Antworten und laufende Kosten wie Server und Remote-Model-Infrastruktur. "
+                    "Wenn dir die App Zeit spart oder Nutzen bringt, erwäge bitte sie zu unterstützen, damit sie sich weiterentwickeln kann.")},
+    {QStringLiteral("Support"), QStringLiteral("Unterstützen")},
+    {QStringLiteral("I'm not yet sure"), QStringLiteral("Ich bin mir noch nicht sicher")},
+    {QStringLiteral("I cannot donate"), QStringLiteral("Ich kann nicht spenden")},
     {QStringLiteral("About the AGPL License"), QStringLiteral("Über die AGPL-Lizenz")},
     {QStringLiteral("AI File Sorter is distributed under the GNU Affero General Public License v3.0."
                     "<br><br>"
```

The excerpt is taken from the commit diff for `feat(support-dialog): add translations for the support dialog`. The most relevant surfaces are `app/lib/TranslationManager.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
