#include "TranslationManager.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <algorithm>

namespace {

static const QHash<QString, QString> kFrenchTranslations = {
    {QStringLiteral("Analyze folder"), QStringLiteral("Analyser le dossier")},
    {QStringLiteral("Stop analyzing"), QStringLiteral("Arrêter l'analyse")},
    {QStringLiteral("Use subcategories"), QStringLiteral("Utiliser les sous-catégories")},
    {QStringLiteral("Categorization type"), QStringLiteral("Type de catégorisation")},
    {QStringLiteral("More refined"), QStringLiteral("Plus précis")},
    {QStringLiteral("More consistent"), QStringLiteral("Plus cohérent")},
    {QStringLiteral("Use a whitelist"), QStringLiteral("Utiliser une liste blanche")},
    {QStringLiteral("Recategorize folder?"), QStringLiteral("Recatégoriser le dossier ?")},
    {QStringLiteral("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?"),
     QStringLiteral("Ce dossier a été catégorisé en mode %1. Voulez-vous le recatégoriser maintenant en mode %2 ?")},
    {QStringLiteral("Recategorize"), QStringLiteral("Recatégoriser")},
    {QStringLiteral("Keep existing"), QStringLiteral("Conserver l'existant")},
    {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("Impossible de réinitialiser la catégorisation en cache pour ce dossier.")},
    {QStringLiteral("Categorize files"), QStringLiteral("Catégoriser les fichiers")},
    {QStringLiteral("Categorize directories"), QStringLiteral("Catégoriser les dossiers")},
    {QStringLiteral("Ready"), QStringLiteral("Prêt")},
    {QStringLiteral("Set folder to %1"), QStringLiteral("Dossier défini sur %1")},
    {QStringLiteral("Loaded folder %1"), QStringLiteral("Dossier chargé %1")},
    {QStringLiteral("Analysis cancelled"), QStringLiteral("Analyse annulée")},
    {QStringLiteral("Folder selected: %1"), QStringLiteral("Dossier sélectionné : %1")},
    {QStringLiteral("Analyzing…"), QStringLiteral("Analyse en cours…")},
    {QStringLiteral("Cancelling analysis…"), QStringLiteral("Annulation de l'analyse…")},
    {QStringLiteral("Folder:"), QStringLiteral("Dossier :")},
    {QStringLiteral("Browse…"), QStringLiteral("Parcourir…")},
    {QStringLiteral("File"), QStringLiteral("Fichier")},
    {QStringLiteral("Type"), QStringLiteral("Type")},
    {QStringLiteral("Category"), QStringLiteral("Catégorie")},
    {QStringLiteral("Subcategory"), QStringLiteral("Sous-catégorie")},
    {QStringLiteral("Status"), QStringLiteral("Statut")},
    {QStringLiteral("Select Directory"), QStringLiteral("Sélectionner un dossier")},
    {QStringLiteral("Directory"), QStringLiteral("Dossier")},
    {QStringLiteral("&File"), QStringLiteral("&Fichier")},
    {QStringLiteral("&Quit"), QStringLiteral("&Quitter")},
    {QStringLiteral("&Edit"), QStringLiteral("&Édition")},
    {QStringLiteral("&Copy"), QStringLiteral("Co&pier")},
    {QStringLiteral("Cu&t"), QStringLiteral("Co&uper")},
    {QStringLiteral("&Paste"), QStringLiteral("&Coller")},
    {QStringLiteral("&Delete"), QStringLiteral("&Supprimer")},
    {QStringLiteral("&View"), QStringLiteral("&Affichage")},
    {QStringLiteral("File &Explorer"), QStringLiteral("Explorateur de fichiers")},
    {QStringLiteral("File Explorer"), QStringLiteral("Explorateur de fichiers")},
    {QStringLiteral("&Settings"), QStringLiteral("&Paramètres")},
    {QStringLiteral("Select &LLM…"), QStringLiteral("Sélectionner le &LLM…")},
    {QStringLiteral("Manage category whitelists…"), QStringLiteral("Gérer les listes blanches de catégories…")},
    {QStringLiteral("&Development"), QStringLiteral("&Développement")},
    {QStringLiteral("Log prompts and responses to stdout"), QStringLiteral("Journaliser les invites et réponses dans stdout")},
    {QStringLiteral("Run &consistency pass"), QStringLiteral("Lancer le &signalement de cohérence")},
    {QStringLiteral("Interface &language"), QStringLiteral("&Langue de l'interface")},
    {QStringLiteral("&English"), QStringLiteral("&Anglais")},
    {QStringLiteral("&French"), QStringLiteral("&Français")},
    {QStringLiteral("&German"), QStringLiteral("&Allemand")},
    {QStringLiteral("&Italian"), QStringLiteral("&Italien")},
    {QStringLiteral("&Spanish"), QStringLiteral("&Espagnol")},
    {QStringLiteral("&Turkish"), QStringLiteral("&Turc")},
    {QStringLiteral("&Help"), QStringLiteral("&Aide")},
    {QStringLiteral("&About"), QStringLiteral("À propos")},
    {QStringLiteral("&About AI File Sorter"), QStringLiteral("À propos d'AI File Sorter")},
    {QStringLiteral("About &Qt"), QStringLiteral("À propos de &Qt")},
    {QStringLiteral("About &AGPL"), QStringLiteral("À propos de l'&AGPL")},
    {QStringLiteral("&Support Project"), QStringLiteral("&Soutenir le projet")},
    {QStringLiteral("Undo last run"), QStringLiteral("Annuler la derniere execution")},
    {QStringLiteral("Plan file:"), QStringLiteral("Fichier du plan :")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Simulation (aperçu uniquement, ne deplace pas les fichiers)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Aperçu de la simulation")},
    {QStringLiteral("From"), QStringLiteral("De")},
    {QStringLiteral("To"), QStringLiteral("Vers")},
    {QStringLiteral("Planned destination"), QStringLiteral("Destination prevue")},
    {QStringLiteral("Preview"), QStringLiteral("Aperçu")},
    {QStringLiteral("Undo last run"), QStringLiteral("Annuler la dernière exécution")},
    {QStringLiteral("Plan file:"), QStringLiteral("Fichier du plan :")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Simulation (aperçu uniquement, ne déplace pas les fichiers)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Aperçu de la simulation")},
    {QStringLiteral("From"), QStringLiteral("De")},
    {QStringLiteral("To"), QStringLiteral("Vers")},
    {QStringLiteral("Planned destination"), QStringLiteral("Destination prévue")},
    {QStringLiteral("Preview"), QStringLiteral("Aperçu")},
    {QStringLiteral("Support AI File Sorter"), QStringLiteral("Soutenir AI File Sorter")},
    {QStringLiteral("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you."),
     QStringLiteral("Merci d'utiliser AI File Sorter ! Vous avez déjà catégorisé %1 fichiers. Moi, l'auteur, j'espère vraiment que cette application vous a été utile.")},
    {QStringLiteral("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
                    "If the app saves you time or brings value, please consider supporting it so it can keep improving."),
     QStringLiteral("AI File Sorter demande des centaines d'heures de développement, de nouvelles fonctionnalités, de réponses au support et des coûts permanents comme les serveurs ou l'infrastructure des modèles distants. "
                    "Si l'application vous fait gagner du temps ou vous apporte de la valeur, merci d'envisager un soutien pour qu'elle puisse continuer à s'améliorer.")},
    {QStringLiteral("Support"), QStringLiteral("Soutenir")},
    {QStringLiteral("I'm not yet sure"), QStringLiteral("Je ne suis pas encore sûr")},
    {QStringLiteral("I cannot donate"), QStringLiteral("Je ne peux pas faire de don")},
    {QStringLiteral("About the AGPL License"), QStringLiteral("À propos de la licence AGPL")},
    {QStringLiteral("AI File Sorter is distributed under the GNU Affero General Public License v3.0."
                    "<br><br>"
                    "You can access the full source code at "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "A full copy of the license is provided with this application and available online at "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>."),
     QStringLiteral("AI File Sorter est distribué sous la licence GNU Affero General Public License v3.0."
                    "<br><br>"
                    "Vous pouvez accéder au code source complet sur "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "Une copie complète de la licence est fournie avec cette application et disponible en ligne sur "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>.")},
    {QStringLiteral("Review Categorization"), QStringLiteral("Vérifier la catégorisation")},
    {QStringLiteral("Select all"), QStringLiteral("Tout sélectionner")},
    {QStringLiteral("Move"), QStringLiteral("Déplacer")},
    {QStringLiteral("Suggested filename"), QStringLiteral("Nom de fichier suggéré")},
    {QStringLiteral("Confirm and Process"), QStringLiteral("Confirmer et traiter")},
    {QStringLiteral("Continue Later"), QStringLiteral("Continuer plus tard")},
    {QStringLiteral("Close"), QStringLiteral("Fermer")},
    {QStringLiteral("Not selected"), QStringLiteral("Non sélectionné")},
    {QStringLiteral("Moved"), QStringLiteral("Déplacé")},
    {QStringLiteral("Skipped"), QStringLiteral("Ignoré")},
    {QStringLiteral("Analyzing Files"), QStringLiteral("Analyse des fichiers")},
    {QStringLiteral("Stop Analysis"), QStringLiteral("Arrêter l'analyse")},
    {QStringLiteral("Analyze picture files by content (can be slow)"), QStringLiteral("Analyser les fichiers image par contenu (peut être lent)")},
    {QStringLiteral("Process image files only (ignore any other files)"), QStringLiteral("Traiter uniquement les fichiers image (ignorer tous les autres fichiers)")},
    {QStringLiteral("Offer to rename image files"), QStringLiteral("Proposer de renommer les fichiers image")},
    {QStringLiteral("Do not categorize image files (only rename)"), QStringLiteral("Ne pas catégoriser les fichiers image (renommer uniquement)")},
    {QStringLiteral("Download required"), QStringLiteral("Téléchargement requis")},
    {QStringLiteral("Download required."), QStringLiteral("Téléchargement requis.")},
    {QStringLiteral("Image analysis requires visual LLM files. Download them now?"), QStringLiteral("L'analyse d'images nécessite les fichiers LLM visuels. Les télécharger maintenant ?")},
    {QStringLiteral("Image analysis models (LLaVA)"), QStringLiteral("Modèles d'analyse d'images (LLaVA)")},
    {QStringLiteral("Download the visual LLM files required for image analysis."), QStringLiteral("Téléchargez les fichiers LLM visuels nécessaires à l'analyse d'images.")},
    {QStringLiteral("LLaVA 1.6 Mistral 7B (text model)"), QStringLiteral("LLaVA 1.6 Mistral 7B (modèle texte)")},
    {QStringLiteral("LLaVA mmproj (vision encoder)"), QStringLiteral("LLaVA mmproj (encodeur de vision)")},
    {QStringLiteral("Remote URL"), QStringLiteral("URL distante")},
    {QStringLiteral("Local path"), QStringLiteral("Chemin local")},
    {QStringLiteral("File size"), QStringLiteral("Taille du fichier")},
    {QStringLiteral("File size: unknown"), QStringLiteral("Taille du fichier : inconnue")},
    {QStringLiteral("Model ready."), QStringLiteral("Modèle prêt.")},
    {QStringLiteral("Partial download detected. You can resume."), QStringLiteral("Téléchargement partiel détecté. Vous pouvez reprendre.")},
    {QStringLiteral("Resume download"), QStringLiteral("Reprendre le téléchargement")},
    {QStringLiteral("Download"), QStringLiteral("Télécharger")},
    {QStringLiteral("Downloading…"), QStringLiteral("Téléchargement…")},
    {QStringLiteral("Missing download URL environment variable (%1)."), QStringLiteral("Variable d'environnement de l'URL de téléchargement manquante (%1).")},
    {QStringLiteral("Missing download URL environment variable."), QStringLiteral("Variable d'environnement de l'URL de téléchargement manquante.")},
    {QStringLiteral("Download complete."), QStringLiteral("Téléchargement terminé.")},
    {QStringLiteral("Download cancelled."), QStringLiteral("Téléchargement annulé.")},
    {QStringLiteral("Download error: %1"), QStringLiteral("Erreur de téléchargement : %1")}
};

static const QHash<QString, QString> kGermanTranslations = {
    {QStringLiteral("Analyze folder"), QStringLiteral("Ordner analysieren")},
    {QStringLiteral("Stop analyzing"), QStringLiteral("Analyse stoppen")},
    {QStringLiteral("Use subcategories"), QStringLiteral("Unterkategorien verwenden")},
    {QStringLiteral("Categorization type"), QStringLiteral("Kategorisierungstyp")},
    {QStringLiteral("More refined"), QStringLiteral("Ausführlicher")},
    {QStringLiteral("More consistent"), QStringLiteral("Einheitlicher")},
    {QStringLiteral("Use a whitelist"), QStringLiteral("Whitelist verwenden")},
    {QStringLiteral("Recategorize folder?"), QStringLiteral("Ordner neu kategorisieren?")},
    {QStringLiteral("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?"),
     QStringLiteral("Dieser Ordner wurde im Modus %1 kategorisiert. Möchten Sie ihn jetzt im Modus %2 neu kategorisieren?")},
    {QStringLiteral("Recategorize"), QStringLiteral("Neu kategorisieren")},
    {QStringLiteral("Keep existing"), QStringLiteral("Beibehalten")},
    {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("Zurücksetzen der zwischengespeicherten Kategorisierung für diesen Ordner fehlgeschlagen.")},
    {QStringLiteral("Categorize files"), QStringLiteral("Dateien kategorisieren")},
    {QStringLiteral("Categorize directories"), QStringLiteral("Ordner kategorisieren")},
    {QStringLiteral("Ready"), QStringLiteral("Bereit")},
    {QStringLiteral("Set folder to %1"), QStringLiteral("Ordner auf %1 gesetzt")},
    {QStringLiteral("Loaded folder %1"), QStringLiteral("Ordner %1 geladen")},
    {QStringLiteral("Analysis cancelled"), QStringLiteral("Analyse abgebrochen")},
    {QStringLiteral("Folder selected: %1"), QStringLiteral("Ordner ausgewählt: %1")},
    {QStringLiteral("Analyzing…"), QStringLiteral("Analysiere…")},
    {QStringLiteral("Cancelling analysis…"), QStringLiteral("Analyse wird abgebrochen…")},
    {QStringLiteral("Folder:"), QStringLiteral("Ordner:")},
    {QStringLiteral("Browse…"), QStringLiteral("Durchsuchen…")},
    {QStringLiteral("File"), QStringLiteral("Datei")},
    {QStringLiteral("Type"), QStringLiteral("Typ")},
    {QStringLiteral("Category"), QStringLiteral("Kategorie")},
    {QStringLiteral("Subcategory"), QStringLiteral("Unterkategorie")},
    {QStringLiteral("Status"), QStringLiteral("Status")},
    {QStringLiteral("Select Directory"), QStringLiteral("Ordner auswählen")},
    {QStringLiteral("Directory"), QStringLiteral("Ordner")},
    {QStringLiteral("&File"), QStringLiteral("&Datei")},
    {QStringLiteral("&Quit"), QStringLiteral("&Beenden")},
    {QStringLiteral("&Edit"), QStringLiteral("&Bearbeiten")},
    {QStringLiteral("&Copy"), QStringLiteral("&Kopieren")},
    {QStringLiteral("Cu&t"), QStringLiteral("A&usschneiden")},
    {QStringLiteral("&Paste"), QStringLiteral("&Einfügen")},
    {QStringLiteral("&Delete"), QStringLiteral("&Löschen")},
    {QStringLiteral("&View"), QStringLiteral("&Ansicht")},
    {QStringLiteral("File &Explorer"), QStringLiteral("Datei-Explorer")},
    {QStringLiteral("File Explorer"), QStringLiteral("Datei-Explorer")},
    {QStringLiteral("&Settings"), QStringLiteral("&Einstellungen")},
    {QStringLiteral("Select &LLM…"), QStringLiteral("&LLM auswählen…")},
    {QStringLiteral("Manage category whitelists…"), QStringLiteral("Kategorie-Whitelists verwalten…")},
    {QStringLiteral("&Development"), QStringLiteral("&Entwicklung")},
    {QStringLiteral("Log prompts and responses to stdout"), QStringLiteral("Eingaben und Antworten in stdout protokollieren")},
    {QStringLiteral("Run &consistency pass"), QStringLiteral("Konsistenzdurchlauf ausführen")},
    {QStringLiteral("Interface &language"), QStringLiteral("&Oberflächensprache")},
    {QStringLiteral("&English"), QStringLiteral("&Englisch")},
    {QStringLiteral("&French"), QStringLiteral("&Französisch")},
    {QStringLiteral("&German"), QStringLiteral("&Deutsch")},
    {QStringLiteral("&Italian"), QStringLiteral("&Italienisch")},
    {QStringLiteral("&Spanish"), QStringLiteral("&Spanisch")},
    {QStringLiteral("&Turkish"), QStringLiteral("&Türkisch")},
    {QStringLiteral("&Help"), QStringLiteral("&Hilfe")},
    {QStringLiteral("&About"), QStringLiteral("&Über")},
    {QStringLiteral("&About AI File Sorter"), QStringLiteral("Über AI File Sorter")},
    {QStringLiteral("About &Qt"), QStringLiteral("Über &Qt")},
    {QStringLiteral("About &AGPL"), QStringLiteral("Über &AGPL")},
    {QStringLiteral("&Support Project"), QStringLiteral("Projekt unterstützen")},
    {QStringLiteral("Undo last run"), QStringLiteral("Letzten Durchlauf rückgängig machen")},
    {QStringLiteral("Plan file:"), QStringLiteral("Plan-Datei:")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Probelauf (nur Vorschau, keine Dateien verschieben)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Vorschau Probelauf")},
    {QStringLiteral("From"), QStringLiteral("Von")},
    {QStringLiteral("To"), QStringLiteral("Nach")},
    {QStringLiteral("Planned destination"), QStringLiteral("Geplantes Ziel")},
    {QStringLiteral("Preview"), QStringLiteral("Vorschau")},
    {QStringLiteral("Undo last run"), QStringLiteral("Letzten Durchlauf rückgängig machen")},
    {QStringLiteral("Plan file:"), QStringLiteral("Plan-Datei:")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Probelauf (nur Vorschau, keine Dateien verschieben)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Vorschau Probelauf")},
    {QStringLiteral("From"), QStringLiteral("Von")},
    {QStringLiteral("To"), QStringLiteral("Nach")},
    {QStringLiteral("Planned destination"), QStringLiteral("Geplantes Ziel")},
    {QStringLiteral("Preview"), QStringLiteral("Vorschau")},
    {QStringLiteral("Support AI File Sorter"), QStringLiteral("Unterstütze AI File Sorter")},
    {QStringLiteral("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you."),
     QStringLiteral("Vielen Dank, dass du AI File Sorter verwendest! Du hast bisher %1 Dateien kategorisiert. Ich, der Autor, hoffe wirklich, dass dir die App geholfen hat.")},
    {QStringLiteral("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
                    "If the app saves you time or brings value, please consider supporting it so it can keep improving."),
     QStringLiteral("AI File Sorter erfordert hunderte Stunden Entwicklung, Funktionsarbeit, Support-Antworten und laufende Kosten wie Server und Remote-Model-Infrastruktur. "
                    "Wenn dir die App Zeit spart oder Nutzen bringt, erwäge bitte sie zu unterstützen, damit sie sich weiterentwickeln kann.")},
    {QStringLiteral("Support"), QStringLiteral("Unterstützen")},
    {QStringLiteral("I'm not yet sure"), QStringLiteral("Ich bin mir noch nicht sicher")},
    {QStringLiteral("I cannot donate"), QStringLiteral("Ich kann nicht spenden")},
    {QStringLiteral("About the AGPL License"), QStringLiteral("Über die AGPL-Lizenz")},
    {QStringLiteral("AI File Sorter is distributed under the GNU Affero General Public License v3.0."
                    "<br><br>"
                    "You can access the full source code at "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "A full copy of the license is provided with this application and available online at "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>."),
     QStringLiteral("AI File Sorter wird unter der GNU Affero General Public License v3.0 vertrieben."
                    "<br><br>"
                    "Den vollständigen Quellcode finden Sie unter "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "Eine vollständige Lizenzkopie liegt bei und ist online verfügbar unter "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>.")},
    {QStringLiteral("Review Categorization"), QStringLiteral("Kategorisierung überprüfen")},
    {QStringLiteral("Select all"), QStringLiteral("Alle auswählen")},
    {QStringLiteral("Move"), QStringLiteral("Verschieben")},
    {QStringLiteral("Suggested filename"), QStringLiteral("Vorgeschlagener Dateiname")},
    {QStringLiteral("Confirm and Process"), QStringLiteral("Bestätigen und verarbeiten")},
    {QStringLiteral("Continue Later"), QStringLiteral("Später fortsetzen")},
    {QStringLiteral("Close"), QStringLiteral("Schließen")},
    {QStringLiteral("Not selected"), QStringLiteral("Nicht ausgewählt")},
    {QStringLiteral("Moved"), QStringLiteral("Verschoben")},
    {QStringLiteral("Skipped"), QStringLiteral("Übersprungen")},
    {QStringLiteral("Analyzing Files"), QStringLiteral("Dateien analysieren")},
    {QStringLiteral("Stop Analysis"), QStringLiteral("Analyse stoppen")},
    {QStringLiteral("Analyze picture files by content (can be slow)"), QStringLiteral("Bilddateien nach Inhalt analysieren (kann langsam sein)")},
    {QStringLiteral("Process image files only (ignore any other files)"), QStringLiteral("Nur Bilddateien verarbeiten (alle anderen Dateien ignorieren)")},
    {QStringLiteral("Offer to rename image files"), QStringLiteral("Umbenennen von Bilddateien anbieten")},
    {QStringLiteral("Do not categorize image files (only rename)"), QStringLiteral("Bilddateien nicht kategorisieren (nur umbenennen)")},
    {QStringLiteral("Download required"), QStringLiteral("Download erforderlich")},
    {QStringLiteral("Download required."), QStringLiteral("Download erforderlich.")},
    {QStringLiteral("Image analysis requires visual LLM files. Download them now?"), QStringLiteral("Die Bildanalyse erfordert visuelle LLM-Dateien. Jetzt herunterladen?")},
    {QStringLiteral("Image analysis models (LLaVA)"), QStringLiteral("Bildanalyse-Modelle (LLaVA)")},
    {QStringLiteral("Download the visual LLM files required for image analysis."), QStringLiteral("Laden Sie die für die Bildanalyse erforderlichen visuellen LLM-Dateien herunter.")},
    {QStringLiteral("LLaVA 1.6 Mistral 7B (text model)"), QStringLiteral("LLaVA 1.6 Mistral 7B (Textmodell)")},
    {QStringLiteral("LLaVA mmproj (vision encoder)"), QStringLiteral("LLaVA mmproj (Vision-Encoder)")},
    {QStringLiteral("Remote URL"), QStringLiteral("Remote-URL")},
    {QStringLiteral("Local path"), QStringLiteral("Lokaler Pfad")},
    {QStringLiteral("File size"), QStringLiteral("Dateigröße")},
    {QStringLiteral("File size: unknown"), QStringLiteral("Dateigröße: unbekannt")},
    {QStringLiteral("Model ready."), QStringLiteral("Modell bereit.")},
    {QStringLiteral("Partial download detected. You can resume."), QStringLiteral("Teilweiser Download erkannt. Sie können fortsetzen.")},
    {QStringLiteral("Resume download"), QStringLiteral("Download fortsetzen")},
    {QStringLiteral("Download"), QStringLiteral("Download")},
    {QStringLiteral("Downloading…"), QStringLiteral("Download läuft…")},
    {QStringLiteral("Missing download URL environment variable (%1)."), QStringLiteral("Umgebungsvariable für die Download-URL fehlt (%1).")},
    {QStringLiteral("Missing download URL environment variable."), QStringLiteral("Umgebungsvariable für die Download-URL fehlt.")},
    {QStringLiteral("Download complete."), QStringLiteral("Download abgeschlossen.")},
    {QStringLiteral("Download cancelled."), QStringLiteral("Download abgebrochen.")},
    {QStringLiteral("Download error: %1"), QStringLiteral("Download-Fehler: %1")}
};

static const QHash<QString, QString> kItalianTranslations = {
    {QStringLiteral("Analyze folder"), QStringLiteral("Analizza cartella")},
    {QStringLiteral("Stop analyzing"), QStringLiteral("Interrompi analisi")},
    {QStringLiteral("Use subcategories"), QStringLiteral("Usa sottocategorie")},
    {QStringLiteral("Categorization type"), QStringLiteral("Tipo di categorizzazione")},
    {QStringLiteral("More refined"), QStringLiteral("Più dettagliata")},
    {QStringLiteral("More consistent"), QStringLiteral("Più coerente")},
    {QStringLiteral("Use a whitelist"), QStringLiteral("Usa whitelist")},
    {QStringLiteral("Recategorize folder?"), QStringLiteral("Ricategorizzare la cartella?")},
    {QStringLiteral("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?"),
     QStringLiteral("Questa cartella è stata categorizzata in modalità %1. Vuoi ricategorizzarla ora in modalità %2?")},
    {QStringLiteral("Recategorize"), QStringLiteral("Ricategorizza")},
    {QStringLiteral("Keep existing"), QStringLiteral("Mantieni")},
    {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("Impossibile ripristinare la categorizzazione memorizzata per questa cartella.")},
    {QStringLiteral("Categorize files"), QStringLiteral("Categoriza i file")},
    {QStringLiteral("Categorize directories"), QStringLiteral("Categoriza le cartelle")},
    {QStringLiteral("Ready"), QStringLiteral("Pronto")},
    {QStringLiteral("Set folder to %1"), QStringLiteral("Cartella impostata su %1")},
    {QStringLiteral("Loaded folder %1"), QStringLiteral("Cartella %1 caricata")},
    {QStringLiteral("Analysis cancelled"), QStringLiteral("Analisi annullata")},
    {QStringLiteral("Folder selected: %1"), QStringLiteral("Cartella selezionata: %1")},
    {QStringLiteral("Analyzing…"), QStringLiteral("Analisi in corso…")},
    {QStringLiteral("Cancelling analysis…"), QStringLiteral("Annullamento analisi…")},
    {QStringLiteral("Folder:"), QStringLiteral("Cartella:")},
    {QStringLiteral("Browse…"), QStringLiteral("Sfoglia…")},
    {QStringLiteral("File"), QStringLiteral("File")},
    {QStringLiteral("Type"), QStringLiteral("Tipo")},
    {QStringLiteral("Category"), QStringLiteral("Categoria")},
    {QStringLiteral("Subcategory"), QStringLiteral("Sottocategoria")},
    {QStringLiteral("Status"), QStringLiteral("Stato")},
    {QStringLiteral("Select Directory"), QStringLiteral("Seleziona cartella")},
    {QStringLiteral("Directory"), QStringLiteral("Cartella")},
    {QStringLiteral("&File"), QStringLiteral("&File")},
    {QStringLiteral("&Quit"), QStringLiteral("&Esci")},
    {QStringLiteral("&Edit"), QStringLiteral("&Modifica")},
    {QStringLiteral("&Copy"), QStringLiteral("&Copia")},
    {QStringLiteral("Cu&t"), QStringLiteral("Tag&lia")},
    {QStringLiteral("&Paste"), QStringLiteral("&Incolla")},
    {QStringLiteral("&Delete"), QStringLiteral("&Elimina")},
    {QStringLiteral("&View"), QStringLiteral("&Visualizza")},
    {QStringLiteral("File &Explorer"), QStringLiteral("Esplora file")},
    {QStringLiteral("File Explorer"), QStringLiteral("Esplora file")},
    {QStringLiteral("&Settings"), QStringLiteral("&Impostazioni")},
    {QStringLiteral("Select &LLM…"), QStringLiteral("Seleziona &LLM…")},
    {QStringLiteral("Manage category whitelists…"), QStringLiteral("Gestisci whitelist categorie…")},
    {QStringLiteral("&Development"), QStringLiteral("&Sviluppo")},
    {QStringLiteral("Log prompts and responses to stdout"), QStringLiteral("Registra prompt e risposte su stdout")},
    {QStringLiteral("Run &consistency pass"), QStringLiteral("Esegui controllo di &coerenza")},
    {QStringLiteral("Interface &language"), QStringLiteral("Lingua dell'&interfaccia")},
    {QStringLiteral("&English"), QStringLiteral("&Inglese")},
    {QStringLiteral("&French"), QStringLiteral("&Francese")},
    {QStringLiteral("&German"), QStringLiteral("&Tedesco")},
    {QStringLiteral("&Italian"), QStringLiteral("&Italiano")},
    {QStringLiteral("&Spanish"), QStringLiteral("&Spagnolo")},
    {QStringLiteral("&Turkish"), QStringLiteral("&Turco")},
    {QStringLiteral("&Help"), QStringLiteral("&Aiuto")},
    {QStringLiteral("&About"), QStringLiteral("Informazioni")},
    {QStringLiteral("&About AI File Sorter"), QStringLiteral("Informazioni su AI File Sorter")},
    {QStringLiteral("About &Qt"), QStringLiteral("Informazioni su &Qt")},
    {QStringLiteral("About &AGPL"), QStringLiteral("Informazioni su &AGPL")},
    {QStringLiteral("&Support Project"), QStringLiteral("Supporta il progetto")},
    {QStringLiteral("Undo last run"), QStringLiteral("Annulla l'ultima esecuzione")},
    {QStringLiteral("Plan file:"), QStringLiteral("File del piano:")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Prova (solo anteprima, non spostare i file)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Anteprima prova")},
    {QStringLiteral("From"), QStringLiteral("Da")},
    {QStringLiteral("To"), QStringLiteral("A")},
    {QStringLiteral("Planned destination"), QStringLiteral("Destinazione prevista")},
    {QStringLiteral("Preview"), QStringLiteral("Anteprima")},
    {QStringLiteral("Undo last run"), QStringLiteral("Annulla l'ultima esecuzione")},
    {QStringLiteral("Plan file:"), QStringLiteral("File del piano:")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Prova (solo anteprima, non spostare i file)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Anteprima prova")},
    {QStringLiteral("From"), QStringLiteral("Da")},
    {QStringLiteral("To"), QStringLiteral("A")},
    {QStringLiteral("Planned destination"), QStringLiteral("Destinazione prevista")},
    {QStringLiteral("Preview"), QStringLiteral("Anteprima")},
    {QStringLiteral("Support AI File Sorter"), QStringLiteral("Sostieni AI File Sorter")},
    {QStringLiteral("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you."),
     QStringLiteral("Grazie per usare AI File Sorter! Hai già categorizzato %1 file. Io, l'autore, spero davvero che questa app ti sia stata utile.")},
    {QStringLiteral("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
                    "If the app saves you time or brings value, please consider supporting it so it can keep improving."),
     QStringLiteral("AI File Sorter richiede centinaia di ore di sviluppo, nuove funzionalità, risposte al supporto e costi continui come server e infrastrutture per modelli remoti. "
                    "Se l'app ti fa risparmiare tempo o ti offre valore, considera di sostenerla affinché possa continuare a migliorare.")},
    {QStringLiteral("Support"), QStringLiteral("Sostieni")},
    {QStringLiteral("I'm not yet sure"), QStringLiteral("Non sono ancora sicuro")},
    {QStringLiteral("I cannot donate"), QStringLiteral("Non posso donare")},
    {QStringLiteral("About the AGPL License"), QStringLiteral("Informazioni sulla licenza AGPL")},
    {QStringLiteral("AI File Sorter is distributed under the GNU Affero General Public License v3.0."
                    "<br><br>"
                    "You can access the full source code at "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "A full copy of the license is provided with this application and available online at "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>."),
     QStringLiteral("AI File Sorter è distribuito sotto la GNU Affero General Public License v3.0."
                    "<br><br>"
                    "Puoi accedere al codice sorgente completo su "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "Una copia completa della licenza è fornita con l'applicazione ed è disponibile online su "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>.")},
    {QStringLiteral("Review Categorization"), QStringLiteral("Rivedi categorizzazione")},
    {QStringLiteral("Select all"), QStringLiteral("Seleziona tutto")},
    {QStringLiteral("Move"), QStringLiteral("Sposta")},
    {QStringLiteral("Suggested filename"), QStringLiteral("Nome file suggerito")},
    {QStringLiteral("Confirm and Process"), QStringLiteral("Conferma e elabora")},
    {QStringLiteral("Continue Later"), QStringLiteral("Continua più tardi")},
    {QStringLiteral("Close"), QStringLiteral("Chiudi")},
    {QStringLiteral("Not selected"), QStringLiteral("Non selezionato")},
    {QStringLiteral("Moved"), QStringLiteral("Spostato")},
    {QStringLiteral("Skipped"), QStringLiteral("Saltato")},
    {QStringLiteral("Analyzing Files"), QStringLiteral("Analisi dei file")},
    {QStringLiteral("Stop Analysis"), QStringLiteral("Interrompi analisi")},
    {QStringLiteral("Analyze picture files by content (can be slow)"), QStringLiteral("Analizza i file immagine in base al contenuto (può essere lento)")},
    {QStringLiteral("Process image files only (ignore any other files)"), QStringLiteral("Elabora solo i file immagine (ignora tutti gli altri file)")},
    {QStringLiteral("Offer to rename image files"), QStringLiteral("Offri di rinominare i file immagine")},
    {QStringLiteral("Do not categorize image files (only rename)"), QStringLiteral("Non categorizzare i file immagine (solo rinomina)")},
    {QStringLiteral("Download required"), QStringLiteral("Download richiesto")},
    {QStringLiteral("Download required."), QStringLiteral("Download richiesto.")},
    {QStringLiteral("Image analysis requires visual LLM files. Download them now?"), QStringLiteral("L'analisi delle immagini richiede i file LLM visivi. Scaricarli ora?")},
    {QStringLiteral("Image analysis models (LLaVA)"), QStringLiteral("Modelli di analisi immagini (LLaVA)")},
    {QStringLiteral("Download the visual LLM files required for image analysis."), QStringLiteral("Scarica i file LLM visivi necessari per l'analisi delle immagini.")},
    {QStringLiteral("LLaVA 1.6 Mistral 7B (text model)"), QStringLiteral("LLaVA 1.6 Mistral 7B (modello di testo)")},
    {QStringLiteral("LLaVA mmproj (vision encoder)"), QStringLiteral("LLaVA mmproj (codificatore visivo)")},
    {QStringLiteral("Remote URL"), QStringLiteral("URL remoto")},
    {QStringLiteral("Local path"), QStringLiteral("Percorso locale")},
    {QStringLiteral("File size"), QStringLiteral("Dimensione file")},
    {QStringLiteral("File size: unknown"), QStringLiteral("Dimensione file: sconosciuta")},
    {QStringLiteral("Model ready."), QStringLiteral("Modello pronto.")},
    {QStringLiteral("Partial download detected. You can resume."), QStringLiteral("Download parziale rilevato. Puoi riprendere.")},
    {QStringLiteral("Resume download"), QStringLiteral("Riprendi download")},
    {QStringLiteral("Download"), QStringLiteral("Scarica")},
    {QStringLiteral("Downloading…"), QStringLiteral("Download in corso…")},
    {QStringLiteral("Missing download URL environment variable (%1)."), QStringLiteral("Variabile d'ambiente dell'URL di download mancante (%1).")},
    {QStringLiteral("Missing download URL environment variable."), QStringLiteral("Variabile d'ambiente dell'URL di download mancante.")},
    {QStringLiteral("Download complete."), QStringLiteral("Download completato.")},
    {QStringLiteral("Download cancelled."), QStringLiteral("Download annullato.")},
    {QStringLiteral("Download error: %1"), QStringLiteral("Errore di download: %1")}
};

static const QHash<QString, QString> kSpanishTranslations = {
    {QStringLiteral("Analyze folder"), QStringLiteral("Analizar carpeta")},
    {QStringLiteral("Stop analyzing"), QStringLiteral("Detener análisis")},
    {QStringLiteral("Use subcategories"), QStringLiteral("Usar subcategorías")},
    {QStringLiteral("Categorization type"), QStringLiteral("Tipo de categorización")},
    {QStringLiteral("More refined"), QStringLiteral("Más detallada")},
    {QStringLiteral("More consistent"), QStringLiteral("Más coherente")},
    {QStringLiteral("Use a whitelist"), QStringLiteral("Usar lista blanca")},
    {QStringLiteral("Recategorize folder?"), QStringLiteral("¿Recategorizar la carpeta?")},
    {QStringLiteral("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?"),
     QStringLiteral("Esta carpeta se categorizó con el modo %1. ¿Quieres recategorizarla ahora con el modo %2?")},
    {QStringLiteral("Recategorize"), QStringLiteral("Recategorizar")},
    {QStringLiteral("Keep existing"), QStringLiteral("Mantener existente")},
    {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("No se pudo restablecer la categorización en caché para esta carpeta.")},
    {QStringLiteral("Categorize files"), QStringLiteral("Categorizar archivos")},
    {QStringLiteral("Categorize directories"), QStringLiteral("Categorizar directorios")},
    {QStringLiteral("Ready"), QStringLiteral("Listo")},
    {QStringLiteral("Set folder to %1"), QStringLiteral("Carpeta establecida en %1")},
    {QStringLiteral("Loaded folder %1"), QStringLiteral("Carpeta %1 cargada")},
    {QStringLiteral("Analysis cancelled"), QStringLiteral("Análisis cancelado")},
    {QStringLiteral("Folder selected: %1"), QStringLiteral("Carpeta seleccionada: %1")},
    {QStringLiteral("Analyzing…"), QStringLiteral("Analizando…")},
    {QStringLiteral("Cancelling analysis…"), QStringLiteral("Cancelando análisis…")},
    {QStringLiteral("Folder:"), QStringLiteral("Carpeta:")},
    {QStringLiteral("Browse…"), QStringLiteral("Explorar…")},
    {QStringLiteral("File"), QStringLiteral("Archivo")},
    {QStringLiteral("Type"), QStringLiteral("Tipo")},
    {QStringLiteral("Category"), QStringLiteral("Categoría")},
    {QStringLiteral("Subcategory"), QStringLiteral("Subcategoría")},
    {QStringLiteral("Status"), QStringLiteral("Estado")},
    {QStringLiteral("Select Directory"), QStringLiteral("Seleccionar carpeta")},
    {QStringLiteral("Directory"), QStringLiteral("Carpeta")},
    {QStringLiteral("&File"), QStringLiteral("&Archivo")},
    {QStringLiteral("&Quit"), QStringLiteral("&Salir")},
    {QStringLiteral("&Edit"), QStringLiteral("&Editar")},
    {QStringLiteral("&Copy"), QStringLiteral("&Copiar")},
    {QStringLiteral("Cu&t"), QStringLiteral("Cor&tar")},
    {QStringLiteral("&Paste"), QStringLiteral("&Pegar")},
    {QStringLiteral("&Delete"), QStringLiteral("&Eliminar")},
    {QStringLiteral("&View"), QStringLiteral("&Ver")},
    {QStringLiteral("File &Explorer"), QStringLiteral("Explorador de archivos")},
    {QStringLiteral("File Explorer"), QStringLiteral("Explorador de archivos")},
    {QStringLiteral("&Settings"), QStringLiteral("&Configuración")},
    {QStringLiteral("Select &LLM…"), QStringLiteral("Seleccionar &LLM…")},
    {QStringLiteral("Manage category whitelists…"), QStringLiteral("Gestionar listas blancas de categorías…")},
    {QStringLiteral("&Development"), QStringLiteral("&Desarrollo")},
    {QStringLiteral("Log prompts and responses to stdout"), QStringLiteral("Registrar prompts y respuestas en stdout")},
    {QStringLiteral("Run &consistency pass"), QStringLiteral("Ejecutar pase de &consistencia")},
    {QStringLiteral("Interface &language"), QStringLiteral("&Idioma de la interfaz")},
    {QStringLiteral("&English"), QStringLiteral("&Inglés")},
    {QStringLiteral("&French"), QStringLiteral("&Francés")},
    {QStringLiteral("&German"), QStringLiteral("&Alemán")},
    {QStringLiteral("&Italian"), QStringLiteral("&Italiano")},
    {QStringLiteral("&Spanish"), QStringLiteral("&Español")},
    {QStringLiteral("&Turkish"), QStringLiteral("&Turco")},
    {QStringLiteral("&Help"), QStringLiteral("&Ayuda")},
    {QStringLiteral("&About"), QStringLiteral("&Acerca de")},
    {QStringLiteral("&About AI File Sorter"), QStringLiteral("Acerca de AI File Sorter")},
    {QStringLiteral("About &Qt"), QStringLiteral("Acerca de &Qt")},
    {QStringLiteral("About &AGPL"), QStringLiteral("Acerca de &AGPL")},
    {QStringLiteral("&Support Project"), QStringLiteral("Apoyar el proyecto")},
    {QStringLiteral("Undo last run"), QStringLiteral("Deshacer la ultima ejecucion")},
    {QStringLiteral("Plan file:"), QStringLiteral("Archivo de plan:")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Prueba (solo vista previa, no mover archivos)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Vista previa de la prueba")},
    {QStringLiteral("From"), QStringLiteral("De")},
    {QStringLiteral("To"), QStringLiteral("A")},
    {QStringLiteral("Planned destination"), QStringLiteral("Destino previsto")},
    {QStringLiteral("Preview"), QStringLiteral("Vista previa")},
    {QStringLiteral("Undo last run"), QStringLiteral("Deshacer la última ejecución")},
    {QStringLiteral("Plan file:"), QStringLiteral("Archivo de plan:")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Prueba (solo vista previa, no mover archivos)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Vista previa de la prueba")},
    {QStringLiteral("From"), QStringLiteral("De")},
    {QStringLiteral("To"), QStringLiteral("A")},
    {QStringLiteral("Planned destination"), QStringLiteral("Destino previsto")},
    {QStringLiteral("Preview"), QStringLiteral("Vista previa")},
    {QStringLiteral("Support AI File Sorter"), QStringLiteral("Apoya AI File Sorter")},
    {QStringLiteral("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you."),
     QStringLiteral("Gracias por usar AI File Sorter. Has categorizado %1 archivos hasta ahora. Yo, el autor, realmente espero que esta aplicación te haya sido útil.")},
    {QStringLiteral("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
                    "If the app saves you time or brings value, please consider supporting it so it can keep improving."),
     QStringLiteral("AI File Sorter requiere cientos de horas de desarrollo, trabajo en nuevas funciones, respuestas de soporte y costos continuos como servidores e infraestructura de modelos remotos. "
                    "Si la aplicación te ahorra tiempo o te aporta valor, considera apoyarla para que pueda seguir mejorando.")},
    {QStringLiteral("Support"), QStringLiteral("Apoyar")},
    {QStringLiteral("I'm not yet sure"), QStringLiteral("Aún no estoy seguro")},
    {QStringLiteral("I cannot donate"), QStringLiteral("No puedo donar")},
    {QStringLiteral("About the AGPL License"), QStringLiteral("Acerca de la licencia AGPL")},
    {QStringLiteral("AI File Sorter is distributed under the GNU Affero General Public License v3.0."
                    "<br><br>"
                    "You can access the full source code at "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "A full copy of the license is provided with this application and available online at "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>."),
     QStringLiteral("AI File Sorter se distribuye bajo la GNU Affero General Public License v3.0."
                    "<br><br>"
                    "Puedes acceder al código fuente completo en "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "Se incluye una copia completa de la licencia y está disponible en línea en "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>.")},
    {QStringLiteral("Review Categorization"), QStringLiteral("Revisar categorización")},
    {QStringLiteral("Select all"), QStringLiteral("Seleccionar todo")},
    {QStringLiteral("Move"), QStringLiteral("Mover")},
    {QStringLiteral("Suggested filename"), QStringLiteral("Nombre de archivo sugerido")},
    {QStringLiteral("Confirm and Process"), QStringLiteral("Confirmar y procesar")},
    {QStringLiteral("Continue Later"), QStringLiteral("Continuar más tarde")},
    {QStringLiteral("Close"), QStringLiteral("Cerrar")},
    {QStringLiteral("Not selected"), QStringLiteral("No seleccionado")},
    {QStringLiteral("Moved"), QStringLiteral("Movido")},
    {QStringLiteral("Skipped"), QStringLiteral("Omitido")},
    {QStringLiteral("Analyzing Files"), QStringLiteral("Analizando archivos")},
    {QStringLiteral("Stop Analysis"), QStringLiteral("Detener análisis")},
    {QStringLiteral("Analyze picture files by content (can be slow)"), QStringLiteral("Analizar archivos de imagen por contenido (puede ser lento)")},
    {QStringLiteral("Process image files only (ignore any other files)"), QStringLiteral("Procesar solo archivos de imagen (ignorar cualquier otro archivo)")},
    {QStringLiteral("Offer to rename image files"), QStringLiteral("Ofrecer renombrar archivos de imagen")},
    {QStringLiteral("Do not categorize image files (only rename)"), QStringLiteral("No categorizar archivos de imagen (solo renombrar)")},
    {QStringLiteral("Download required"), QStringLiteral("Descarga requerida")},
    {QStringLiteral("Download required."), QStringLiteral("Descarga requerida.")},
    {QStringLiteral("Image analysis requires visual LLM files. Download them now?"), QStringLiteral("El análisis de imágenes requiere archivos LLM visuales. ¿Descargarlos ahora?")},
    {QStringLiteral("Image analysis models (LLaVA)"), QStringLiteral("Modelos de análisis de imágenes (LLaVA)")},
    {QStringLiteral("Download the visual LLM files required for image analysis."), QStringLiteral("Descarga los archivos LLM visuales necesarios para el análisis de imágenes.")},
    {QStringLiteral("LLaVA 1.6 Mistral 7B (text model)"), QStringLiteral("LLaVA 1.6 Mistral 7B (modelo de texto)")},
    {QStringLiteral("LLaVA mmproj (vision encoder)"), QStringLiteral("LLaVA mmproj (codificador de visión)")},
    {QStringLiteral("Remote URL"), QStringLiteral("URL remota")},
    {QStringLiteral("Local path"), QStringLiteral("Ruta local")},
    {QStringLiteral("File size"), QStringLiteral("Tamaño del archivo")},
    {QStringLiteral("File size: unknown"), QStringLiteral("Tamaño del archivo: desconocido")},
    {QStringLiteral("Model ready."), QStringLiteral("Modelo listo.")},
    {QStringLiteral("Partial download detected. You can resume."), QStringLiteral("Descarga parcial detectada. Puedes reanudar.")},
    {QStringLiteral("Resume download"), QStringLiteral("Reanudar descarga")},
    {QStringLiteral("Download"), QStringLiteral("Descargar")},
    {QStringLiteral("Downloading…"), QStringLiteral("Descargando…")},
    {QStringLiteral("Missing download URL environment variable (%1)."), QStringLiteral("Falta la variable de entorno de la URL de descarga (%1).")},
    {QStringLiteral("Missing download URL environment variable."), QStringLiteral("Falta la variable de entorno de la URL de descarga.")},
    {QStringLiteral("Download complete."), QStringLiteral("Descarga completa.")},
    {QStringLiteral("Download cancelled."), QStringLiteral("Descarga cancelada.")},
    {QStringLiteral("Download error: %1"), QStringLiteral("Error de descarga: %1")}
};

static const QHash<QString, QString> kTurkishTranslations = {
    {QStringLiteral("Analyze folder"), QStringLiteral("Klasörü analiz et")},
    {QStringLiteral("Stop analyzing"), QStringLiteral("Analizi durdur")},
    {QStringLiteral("Use subcategories"), QStringLiteral("Alt kategorileri kullan")},
    {QStringLiteral("Categorization type"), QStringLiteral("Kategorilendirme türü")},
    {QStringLiteral("More refined"), QStringLiteral("Daha ayrıntılı")},
    {QStringLiteral("More consistent"), QStringLiteral("Daha tutarlı")},
    {QStringLiteral("Use a whitelist"), QStringLiteral("Beyaz liste kullan")},
    {QStringLiteral("Recategorize folder?"), QStringLiteral("Klasör yeniden kategorilendirilsin mi?")},
    {QStringLiteral("This folder was categorized using the %1 mode. Do you want to recategorize it now using the %2 mode?"),
     QStringLiteral("Bu klasör %1 modunda kategorilendirildi. Şimdi %2 moduyla yeniden kategorilendirmek ister misiniz?")},
    {QStringLiteral("Recategorize"), QStringLiteral("Yeniden kategorilendir")},
    {QStringLiteral("Keep existing"), QStringLiteral("Mevcut kalsın")},
    {QStringLiteral("Failed to reset cached categorization for this folder."), QStringLiteral("Bu klasör için önbellekteki kategorilendirme sıfırlanamadı.")},
    {QStringLiteral("Categorize files"), QStringLiteral("Dosyaları kategorilendir")},
    {QStringLiteral("Categorize directories"), QStringLiteral("Dizinleri kategorilendir")},
    {QStringLiteral("Ready"), QStringLiteral("Hazır")},
    {QStringLiteral("Set folder to %1"), QStringLiteral("Klasör %1 olarak ayarlandı")},
    {QStringLiteral("Loaded folder %1"), QStringLiteral("%1 klasörü yüklendi")},
    {QStringLiteral("Analysis cancelled"), QStringLiteral("Analiz iptal edildi")},
    {QStringLiteral("Folder selected: %1"), QStringLiteral("Seçilen klasör: %1")},
    {QStringLiteral("Analyzing…"), QStringLiteral("Analiz ediliyor…")},
    {QStringLiteral("Cancelling analysis…"), QStringLiteral("Analiz iptal ediliyor…")},
    {QStringLiteral("Folder:"), QStringLiteral("Klasör:")},
    {QStringLiteral("Browse…"), QStringLiteral("Gözat…")},
    {QStringLiteral("File"), QStringLiteral("Dosya")},
    {QStringLiteral("Type"), QStringLiteral("Tür")},
    {QStringLiteral("Category"), QStringLiteral("Kategori")},
    {QStringLiteral("Subcategory"), QStringLiteral("Alt kategori")},
    {QStringLiteral("Status"), QStringLiteral("Durum")},
    {QStringLiteral("Select Directory"), QStringLiteral("Klasör seç")},
    {QStringLiteral("Directory"), QStringLiteral("Klasör")},
    {QStringLiteral("&File"), QStringLiteral("&Dosya")},
    {QStringLiteral("&Quit"), QStringLiteral("&Çıkış")},
    {QStringLiteral("&Edit"), QStringLiteral("&Düzenle")},
    {QStringLiteral("&Copy"), QStringLiteral("&Kopyala")},
    {QStringLiteral("Cu&t"), QStringLiteral("Ke&s")},
    {QStringLiteral("&Paste"), QStringLiteral("&Yapıştır")},
    {QStringLiteral("&Delete"), QStringLiteral("&Sil")},
    {QStringLiteral("&View"), QStringLiteral("&Görüntüle")},
    {QStringLiteral("File &Explorer"), QStringLiteral("Dosya gezgini")},
    {QStringLiteral("File Explorer"), QStringLiteral("Dosya gezgini")},
    {QStringLiteral("&Settings"), QStringLiteral("&Ayarlar")},
    {QStringLiteral("Select &LLM…"), QStringLiteral("&LLM seç…")},
    {QStringLiteral("Manage category whitelists…"), QStringLiteral("Kategori beyaz listelerini yönet…")},
    {QStringLiteral("&Development"), QStringLiteral("&Geliştirme")},
    {QStringLiteral("Log prompts and responses to stdout"), QStringLiteral("İstek ve yanıtları stdout'a kaydet")},
    {QStringLiteral("Run &consistency pass"), QStringLiteral("&Tutarlılık geçişi çalıştır")},
    {QStringLiteral("Interface &language"), QStringLiteral("Arayüz &dili")},
    {QStringLiteral("&English"), QStringLiteral("&İngilizce")},
    {QStringLiteral("&French"), QStringLiteral("&Fransızca")},
    {QStringLiteral("&German"), QStringLiteral("&Almanca")},
    {QStringLiteral("&Italian"), QStringLiteral("&İtalyanca")},
    {QStringLiteral("&Spanish"), QStringLiteral("&İspanyolca")},
    {QStringLiteral("&Turkish"), QStringLiteral("&Türkçe")},
    {QStringLiteral("&Help"), QStringLiteral("&Yardım")},
    {QStringLiteral("&About"), QStringLiteral("&Hakkında")},
    {QStringLiteral("&About AI File Sorter"), QStringLiteral("AI File Sorter hakkında")},
    {QStringLiteral("About &Qt"), QStringLiteral("&Qt hakkında")},
    {QStringLiteral("About &AGPL"), QStringLiteral("&AGPL hakkında")},
    {QStringLiteral("&Support Project"), QStringLiteral("Projeyi destekle")},
    {QStringLiteral("Support AI File Sorter"), QStringLiteral("AI File Sorter'ı Destekle")},
    {QStringLiteral("Thank you for using AI File Sorter! You have categorized %1 files thus far. I, the author, really hope this app was useful for you."),
     QStringLiteral("AI File Sorter'ı kullandığınız için teşekkürler! Şimdiye kadar %1 dosyayı kategorilendirdiniz. Geliştirici olarak umarım bu uygulama sizin için faydalı olmuştur.")},
    {QStringLiteral("AI File Sorter takes hundreds of hours of development, feature work, support replies, and ongoing costs such as servers and remote-model infrastructure. "
                    "If the app saves you time or brings value, please consider supporting it so it can keep improving."),
     QStringLiteral("AI File Sorter yüzlerce saatlik geliştirme, özellik çalışması, destek yanıtları ve sunucular ile uzaktaki modeller gibi devam eden maliyetler gerektirir. "
                    "Uygulama size zaman kazandırıyor ya da değer sağlıyorsa, gelişmeye devam edebilmesi için lütfen desteklemeyi düşünün.")},
    {QStringLiteral("Support"), QStringLiteral("Destekle")},
    {QStringLiteral("I'm not yet sure"), QStringLiteral("Henüz emin değilim")},
    {QStringLiteral("I cannot donate"), QStringLiteral("Bağış yapamıyorum")},
    {QStringLiteral("About the AGPL License"), QStringLiteral("AGPL lisansı hakkında")},
    {QStringLiteral("AI File Sorter is distributed under the GNU Affero General Public License v3.0."
                    "<br><br>"
                    "You can access the full source code at "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "A full copy of the license is provided with this application and available online at "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>."),
     QStringLiteral("AI File Sorter, GNU Affero General Public License v3.0 kapsamında dağıtılmaktadır."
                    "<br><br>"
                    "Tam kaynak koduna şu adresten erişebilirsiniz: "
                    "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
                    "<br><br>"
                    "Lisansın tam kopyası uygulama ile birlikte gelir ve çevrimiçi olarak "
                    "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a> adresinde bulunur.")},
    {QStringLiteral("Review Categorization"), QStringLiteral("Kategoriyi gözden geçir")},
    {QStringLiteral("Select all"), QStringLiteral("Tümünü seç")},
    {QStringLiteral("Move"), QStringLiteral("Taşı")},
    {QStringLiteral("Suggested filename"), QStringLiteral("Önerilen dosya adı")},
    {QStringLiteral("Confirm and Process"), QStringLiteral("Onayla ve işle")},
    {QStringLiteral("Continue Later"), QStringLiteral("Daha sonra devam et")},
    {QStringLiteral("Close"), QStringLiteral("Kapat")},
    {QStringLiteral("Not selected"), QStringLiteral("Seçilmedi")},
    {QStringLiteral("Moved"), QStringLiteral("Taşındı")},
    {QStringLiteral("Skipped"), QStringLiteral("Atlandı")},
    {QStringLiteral("Analyzing Files"), QStringLiteral("Dosyalar analiz ediliyor")},
    {QStringLiteral("Stop Analysis"), QStringLiteral("Analizi durdur")},
    {QStringLiteral("Analyze picture files by content (can be slow)"), QStringLiteral("Resim dosyalarını içeriğe göre analiz et (yavaş olabilir)")},
    {QStringLiteral("Process image files only (ignore any other files)"), QStringLiteral("Yalnızca resim dosyalarını işle (diğer tüm dosyaları yok say)")},
    {QStringLiteral("Offer to rename image files"), QStringLiteral("Resim dosyalarını yeniden adlandırmayı öner")},
    {QStringLiteral("Do not categorize image files (only rename)"), QStringLiteral("Resim dosyalarını kategorize etme (yalnızca yeniden adlandır)")},
    {QStringLiteral("Download required"), QStringLiteral("İndirme gerekli")},
    {QStringLiteral("Download required."), QStringLiteral("İndirme gerekli.")},
    {QStringLiteral("Image analysis requires visual LLM files. Download them now?"), QStringLiteral("Görüntü analizi için görsel LLM dosyaları gerekir. Şimdi indirilsin mi?")},
    {QStringLiteral("Image analysis models (LLaVA)"), QStringLiteral("Görüntü analizi modelleri (LLaVA)")},
    {QStringLiteral("Download the visual LLM files required for image analysis."), QStringLiteral("Görüntü analizi için gerekli görsel LLM dosyalarını indirin.")},
    {QStringLiteral("LLaVA 1.6 Mistral 7B (text model)"), QStringLiteral("LLaVA 1.6 Mistral 7B (metin modeli)")},
    {QStringLiteral("LLaVA mmproj (vision encoder)"), QStringLiteral("LLaVA mmproj (görüntü kodlayıcı)")},
    {QStringLiteral("Remote URL"), QStringLiteral("Uzak URL")},
    {QStringLiteral("Local path"), QStringLiteral("Yerel yol")},
    {QStringLiteral("File size"), QStringLiteral("Dosya boyutu")},
    {QStringLiteral("File size: unknown"), QStringLiteral("Dosya boyutu: bilinmiyor")},
    {QStringLiteral("Model ready."), QStringLiteral("Model hazır.")},
    {QStringLiteral("Partial download detected. You can resume."), QStringLiteral("Kısmi indirme tespit edildi. Devam edebilirsiniz.")},
    {QStringLiteral("Resume download"), QStringLiteral("İndirmeye devam et")},
    {QStringLiteral("Download"), QStringLiteral("İndir")},
    {QStringLiteral("Downloading…"), QStringLiteral("İndiriliyor…")},
    {QStringLiteral("Missing download URL environment variable (%1)."), QStringLiteral("İndirme URL'si ortam değişkeni eksik (%1).")},
    {QStringLiteral("Missing download URL environment variable."), QStringLiteral("İndirme URL'si ortam değişkeni eksik.")},
    {QStringLiteral("Download complete."), QStringLiteral("İndirme tamamlandı.")},
    {QStringLiteral("Download cancelled."), QStringLiteral("İndirme iptal edildi.")},
    {QStringLiteral("Download error: %1"), QStringLiteral("İndirme hatası: %1")},
    {QStringLiteral("Undo last run"), QStringLiteral("Son çalıştırmayı geri al")},
    {QStringLiteral("Plan file:"), QStringLiteral("Plan dosyası:")},
    {QStringLiteral("Dry run (preview only, do not move files)"), QStringLiteral("Deneme çalıştırma (yalnızca önizleme, dosyaları taşıma)")},
    {QStringLiteral("Dry run preview"), QStringLiteral("Deneme çalıştırma önizlemesi")},
    {QStringLiteral("From"), QStringLiteral("Kaynak")},
    {QStringLiteral("To"), QStringLiteral("Hedef")},
    {QStringLiteral("Planned destination"), QStringLiteral("Planlanan hedef")},
    {QStringLiteral("Preview"), QStringLiteral("Önizleme")}
};

const QHash<QString, QString>* translations_for(Language lang)
{
    switch (lang) {
    case Language::French: return &kFrenchTranslations;
    case Language::German: return &kGermanTranslations;
    case Language::Italian: return &kItalianTranslations;
    case Language::Spanish: return &kSpanishTranslations;
    case Language::Turkish: return &kTurkishTranslations;
    default: return nullptr;
    }
}

} // namespace

class TranslationManager::StaticTranslator : public QTranslator
{
public:
    explicit StaticTranslator(QObject* parent = nullptr)
        : QTranslator(parent)
    {}

    void set_language(Language language)
    {
        language_ = language;
    }

    QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)

        if (!sourceText) {
            return QString();
        }

        if (const auto* map = translations_for(language_)) {
            const QString key = QString::fromUtf8(sourceText);
            const auto it = map->constFind(key);
            if (it != map->constEnd()) {
                return it.value();
            }
        }
        return QString();
    }

private:
    Language language_{Language::English};
};

TranslationManager::TranslationManager() = default;

TranslationManager& TranslationManager::instance()
{
    static TranslationManager manager;
    return manager;
}

void TranslationManager::initialize(QApplication* app)
{
    app_ = app;
    if (!translator_) {
        translator_ = std::make_unique<StaticTranslator>();
    }
    if (languages_.empty()) {
        languages_.push_back(LanguageInfo{Language::English, QStringLiteral("en"), QStringLiteral("English"), QString()});
        languages_.push_back(LanguageInfo{Language::French, QStringLiteral("fr"), QStringLiteral("French"), QString()});
        languages_.push_back(LanguageInfo{Language::German, QStringLiteral("de"), QStringLiteral("German"), QString()});
        languages_.push_back(LanguageInfo{Language::Italian, QStringLiteral("it"), QStringLiteral("Italian"), QString()});
        languages_.push_back(LanguageInfo{Language::Spanish, QStringLiteral("es"), QStringLiteral("Spanish"), QString()});
        languages_.push_back(LanguageInfo{Language::Turkish, QStringLiteral("tr"), QStringLiteral("Turkish"), QString()});
    }
}

void TranslationManager::initialize_for_app(QApplication* app, Language language)
{
    initialize(app);
    set_language(language);
}

void TranslationManager::set_language(Language language)
{
    if (!app_) {
        current_language_ = language;
        return;
    }

    if (!languages_.empty()) {
        const bool supported = std::any_of(
            languages_.cbegin(), languages_.cend(),
            [language](const LanguageInfo& info) { return info.id == language; });
        if (!supported) {
            language = Language::English;
        }
    }

    app_->removeTranslator(translator_.get());

    if (translator_) {
        translator_->set_language(language);
        if (language != Language::English) {
            app_->installTranslator(translator_.get());
        }
    }

    current_language_ = language;
}

Language TranslationManager::current_language() const
{
    return current_language_;
}

const std::vector<TranslationManager::LanguageInfo>& TranslationManager::available_languages() const
{
    return languages_;
}
