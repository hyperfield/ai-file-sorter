#ifndef LANGUAGE_HPP
#define LANGUAGE_HPP

#include <QString>

enum class Language {
    English,
    French
};

inline QString languageToString(Language language)
{
    switch (language) {
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
    return Language::English;
}

#endif // LANGUAGE_HPP

