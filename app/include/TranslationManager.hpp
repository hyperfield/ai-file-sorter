#ifndef TRANSLATIONMANAGER_HPP
#define TRANSLATIONMANAGER_HPP

#include "Language.hpp"

#include <QObject>
#include <QCoreApplication>
#include <QPointer>
#include <QTranslator>
#include <memory>
#include <vector>

/** @brief Own application and bundled Qt translators for GUI or QCoreApplication hosts. */
class TranslationManager : public QObject
{
public:
    struct LanguageInfo {
        Language id;
        QString code;
        QString name;
        QString resource_path;
    };

    static TranslationManager& instance();

    /** @brief Attach to an application idempotently. @param app GUI or headless application. */
    void initialize(QCoreApplication* app);
    /** @brief Install saved/default translations before startup dialogs. @param app Application. @param language UI language. */
    void initialize_for_app(QCoreApplication* app, Language language);
    void set_language(Language language);
    Language current_language() const;
    const std::vector<LanguageInfo>& available_languages() const;

private:
    TranslationManager();
    bool load_translation(const LanguageInfo& info);

    QPointer<QCoreApplication> app_;
    std::unique_ptr<QTranslator> translator_;
    std::unique_ptr<QTranslator> qt_translator_;
    bool language_installed_{false};
    Language current_language_{Language::English};
    std::vector<LanguageInfo> languages_;
};

#endif // TRANSLATIONMANAGER_HPP
