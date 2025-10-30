#ifndef TRANSLATIONMANAGER_HPP
#define TRANSLATIONMANAGER_HPP

#include "Language.hpp"

#include <QObject>
#include <QTranslator>
#include <memory>

class QApplication;

class TranslationManager : public QObject
{
public:
    static TranslationManager& instance();

    void initialize(QApplication* app);
    void set_language(Language language);
    Language current_language() const;

private:
    class StaticTranslator;

    TranslationManager();

    QApplication* app_{nullptr};
    std::unique_ptr<StaticTranslator> translator_;
    Language current_language_{Language::English};
};

#endif // TRANSLATIONMANAGER_HPP
