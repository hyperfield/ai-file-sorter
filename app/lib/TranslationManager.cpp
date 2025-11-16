#include "TranslationManager.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QHash>
#include <QString>

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
    {QStringLiteral("&Language"), QStringLiteral("&Langue")},
    {QStringLiteral("&English"), QStringLiteral("&Anglais")},
    {QStringLiteral("&French"), QStringLiteral("&Français")},
    {QStringLiteral("&Help"), QStringLiteral("&Aide")},
    {QStringLiteral("&About"), QStringLiteral("À propos")},
    {QStringLiteral("&About AI File Sorter"), QStringLiteral("À propos d'AI File Sorter")},
    {QStringLiteral("About &Qt"), QStringLiteral("À propos de &Qt")},
    {QStringLiteral("About &AGPL"), QStringLiteral("À propos de l'&AGPL")},
    {QStringLiteral("&Support Project"), QStringLiteral("&Soutenir le projet")},
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
    {QStringLiteral("Confirm and Sort"), QStringLiteral("Confirmer et trier")},
    {QStringLiteral("Continue Later"), QStringLiteral("Continuer plus tard")},
    {QStringLiteral("Close"), QStringLiteral("Fermer")},
    {QStringLiteral("Not selected"), QStringLiteral("Non sélectionné")},
    {QStringLiteral("Moved"), QStringLiteral("Déplacé")},
    {QStringLiteral("Skipped"), QStringLiteral("Ignoré")},
    {QStringLiteral("Analyzing Files"), QStringLiteral("Analyse des fichiers")},
    {QStringLiteral("Stop Analysis"), QStringLiteral("Arrêter l'analyse")},
    {QStringLiteral("Download complete."), QStringLiteral("Téléchargement terminé.")},
    {QStringLiteral("Download cancelled."), QStringLiteral("Téléchargement annulé.")},
    {QStringLiteral("Download error: %1"), QStringLiteral("Erreur de téléchargement : %1")}
};

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

    Language language() const
    {
        return language_;
    }

    QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override
    {
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)

        if (!sourceText || language_ != Language::French) {
            return QString();
        }

        const QString key = QString::fromUtf8(sourceText);
        const auto it = kFrenchTranslations.constFind(key);
        if (it != kFrenchTranslations.constEnd()) {
            return it.value();
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
}

void TranslationManager::set_language(Language language)
{
    if (!app_ || current_language_ == language) {
        current_language_ = language;
        if (translator_) {
            translator_->set_language(language);
        }
        return;
    }

    if (!translator_) {
        translator_ = std::make_unique<StaticTranslator>();
    }

    app_->removeTranslator(translator_.get());
    translator_->set_language(language);
    current_language_ = language;

    if (language != Language::English) {
        app_->installTranslator(translator_.get());
    }
}

Language TranslationManager::current_language() const
{
    return current_language_;
}
