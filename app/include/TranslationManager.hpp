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
    void initialize_for_app(QApplication* app, Language language);
    void set_language(Language language);
    Language current_language() const;
    const std::vector<LanguageInfo>& available_languages() const;

private:
    TranslationManager();
    bool load_translation(const LanguageInfo& info);

    QApplication* app_{nullptr};
    std::unique_ptr<QTranslator> translator_;
    Language current_language_{Language::English};
    std::vector<LanguageInfo> languages_;
};

#endif // TRANSLATIONMANAGER_HPP
