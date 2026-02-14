#ifndef CATEGORYLANGUAGE_HPP
#define CATEGORYLANGUAGE_HPP

#include <QString>
#include <string>
#include <array>

enum class CategoryLanguage {
    Dutch,
    English,
    French,
    German,
    Italian,
    Polish,
    Portuguese,
    Spanish,
    Turkish
};

inline QString categoryLanguageToString(CategoryLanguage language)
{
    static const std::array<const char*, 9> names = {
        "Dutch",
        "English",
        "French",
        "German",
        "Italian",
        "Polish",
        "Portuguese",
        "Spanish",
        "Turkish"
    };

    const auto idx = static_cast<std::size_t>(language);
    if (idx < names.size()) {
        return QString::fromUtf8(names[idx]);
    }
    return QStringLiteral("English");
}

inline CategoryLanguage categoryLanguageFromString(const QString& value)
{
    const QString lowered = value.toLower();
    static const std::array<std::pair<QString, CategoryLanguage>, 9> mapping = {{
        {QStringLiteral("dutch"), CategoryLanguage::Dutch},
        {QStringLiteral("english"), CategoryLanguage::English},
        {QStringLiteral("french"), CategoryLanguage::French},
        {QStringLiteral("german"), CategoryLanguage::German},
        {QStringLiteral("italian"), CategoryLanguage::Italian},
        {QStringLiteral("polish"), CategoryLanguage::Polish},
        {QStringLiteral("portuguese"), CategoryLanguage::Portuguese},
        {QStringLiteral("spanish"), CategoryLanguage::Spanish},
        {QStringLiteral("turkish"), CategoryLanguage::Turkish},
    }};
    for (const auto& entry : mapping) {
        if (lowered == entry.first) {
            return entry.second;
        }
    }
    return CategoryLanguage::English;
}

inline std::string categoryLanguageDisplay(CategoryLanguage lang) {
    return categoryLanguageToString(lang).toStdString();
}

#endif // CATEGORYLANGUAGE_HPP
