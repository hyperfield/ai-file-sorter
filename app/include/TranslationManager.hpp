#ifndef TRANSLATIONMANAGER_HPP
#define TRANSLATIONMANAGER_HPP

#include "Language.hpp"

#include <QObject>
#include <QTranslator>
#include <memory>
#include <vector>

class QApplication;

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

    void initialize(QApplication* app);
    void set_language(Language language);
    Language current_language() const;
    const std::vector<LanguageInfo>& available_languages() const;

private:
    class StaticTranslator;

    TranslationManager();

    QApplication* app_{nullptr};
    std::unique_ptr<QTranslator> file_translator_;
    std::unique_ptr<StaticTranslator> fallback_translator_;
    Language current_language_{Language::English};
    std::vector<LanguageInfo> languages_;
};

#endif // TRANSLATIONMANAGER_HPP
