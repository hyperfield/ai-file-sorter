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
    {QStringLiteral("&Language"), QStringLiteral("&Langue")},
    {QStringLiteral("&English"), QStringLiteral("&Anglais")},
    {QStringLiteral("&French"), QStringLiteral("&Français")},
    {QStringLiteral("&Help"), QStringLiteral("&Aide")},
    {QStringLiteral("&About"), QStringLiteral("À propos")},
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
    {QStringLiteral("Stop Analysis"), QStringLiteral("Arrêter l'analyse")}
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
