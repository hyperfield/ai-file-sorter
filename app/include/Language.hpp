#ifndef LANGUAGE_HPP
#define LANGUAGE_HPP

#include <QString>

enum class Language {
    English,
    French,
    German,
    Italian,
    Spanish,
    Turkish
};

inline QString languageToString(Language language)
{
    switch (language) {
    case Language::German:
        return QStringLiteral("German");
    case Language::Italian:
        return QStringLiteral("Italian");
    case Language::Spanish:
        return QStringLiteral("Spanish");
    case Language::Turkish:
        return QStringLiteral("Turkish");
    case Language::French:
        return QStringLiteral("French");
    case Language::English:
    default:
        return QStringLiteral("English");
    }
}

inline Language languageFromString(const QString& value)
{
    const QString lowered = value.toLower();
    if (lowered == QStringLiteral("french") || lowered == QStringLiteral("fr")) {
        return Language::French;
    }
    if (lowered == QStringLiteral("german") || lowered == QStringLiteral("de")) {
        return Language::German;
    }
    if (lowered == QStringLiteral("italian") || lowered == QStringLiteral("it")) {
        return Language::Italian;
    }
    if (lowered == QStringLiteral("spanish") || lowered == QStringLiteral("es")) {
        return Language::Spanish;
    }
    if (lowered == QStringLiteral("turkish") || lowered == QStringLiteral("tr")) {
        return Language::Turkish;
    }
    return Language::English;
}

#endif // LANGUAGE_HPP
