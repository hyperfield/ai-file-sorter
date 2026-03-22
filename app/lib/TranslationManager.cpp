#include "TranslationManager.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

using LanguageInfo = TranslationManager::LanguageInfo;

std::vector<LanguageInfo> build_languages()
{
    return {
        {Language::English, QStringLiteral("en"), QStringLiteral("English"), QString()},
        {Language::Dutch, QStringLiteral("nl"), QStringLiteral("Dutch"), QStringLiteral(":/i18n/aifilesorter_nl.qm")},
        {Language::French, QStringLiteral("fr"), QStringLiteral("French"), QStringLiteral(":/i18n/aifilesorter_fr.qm")},
        {Language::German, QStringLiteral("de"), QStringLiteral("German"), QStringLiteral(":/i18n/aifilesorter_de.qm")},
        {Language::Italian, QStringLiteral("it"), QStringLiteral("Italian"), QStringLiteral(":/i18n/aifilesorter_it.qm")},
        {Language::Spanish, QStringLiteral("es"), QStringLiteral("Spanish"), QStringLiteral(":/i18n/aifilesorter_es.qm")},
        {Language::Turkish, QStringLiteral("tr"), QStringLiteral("Turkish"), QStringLiteral(":/i18n/aifilesorter_tr.qm")},
        {Language::Korean, QStringLiteral("ko"), QStringLiteral("Korean"), QStringLiteral(":/i18n/aifilesorter_ko.qm")},
    };
}

const LanguageInfo* find_language_info(const std::vector<LanguageInfo>& languages, Language language)
{
    const auto it = std::find_if(
        languages.cbegin(), languages.cend(),
        [language](const LanguageInfo& info) { return info.id == language; });
    if (it == languages.cend()) {
        return nullptr;
    }
    return &(*it);
}

QStringList translation_search_paths()
{
    QStringList paths;
    if (QCoreApplication::instance()) {
        const QDir app_dir(QCoreApplication::applicationDirPath());
        paths << app_dir.filePath(QStringLiteral("i18n"));
        paths << app_dir.filePath(QStringLiteral("../i18n"));
#if defined(Q_OS_MACOS)
        paths << app_dir.filePath(QStringLiteral("../Resources/i18n"));
#endif
    }

    const QDir cwd(QDir::currentPath());
    paths << cwd.filePath(QStringLiteral("app/resources/i18n"));
    paths << cwd.filePath(QStringLiteral("resources/i18n"));
    paths.removeDuplicates();
    return paths;
}

} // namespace

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
        translator_ = std::make_unique<QTranslator>();
    }
    if (languages_.empty()) {
        languages_ = build_languages();
    }
}

void TranslationManager::initialize_for_app(QApplication* app, Language language)
{
    initialize(app);
    set_language(language);
}

void TranslationManager::set_language(Language language)
{
    if (languages_.empty()) {
        languages_ = build_languages();
    }

    const LanguageInfo* info = find_language_info(languages_, language);
    if (!info) {
        language = Language::English;
        info = find_language_info(languages_, language);
    }

    if (!app_) {
        current_language_ = language;
        return;
    }

    if (translator_) {
        app_->removeTranslator(translator_.get());
        translator_ = std::make_unique<QTranslator>();
    }

    if (info && language != Language::English) {
        if (load_translation(*info)) {
            app_->installTranslator(translator_.get());
        } else {
            language = Language::English;
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

bool TranslationManager::load_translation(const LanguageInfo& info)
{
    if (!translator_) {
        return false;
    }

    if (!info.resource_path.isEmpty() && translator_->load(info.resource_path)) {
        return true;
    }

    const QString file_name = QFileInfo(info.resource_path).fileName();
    if (file_name.isEmpty()) {
        return false;
    }

    for (const QString& dir : translation_search_paths()) {
        const QString candidate = QDir(dir).filePath(file_name);
        if (QFileInfo::exists(candidate) && translator_->load(candidate)) {
            return true;
        }
    }

    return false;
}
