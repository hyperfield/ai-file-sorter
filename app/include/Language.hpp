#ifndef LANGUAGE_HPP
#define LANGUAGE_HPP

#include <QString>
#include <array>

/** @brief Supported interface languages; existing ordinal values remain stable. */
enum class Language {
    English,
    French,
    German,
    Hindi,
    Italian,
    Spanish,
    Turkish,
    Korean,
    SimplifiedChinese,
    Dutch,
    Swedish,
    Icelandic,
    Norwegian,
    Finnish,
    Danish,
    Portuguese,
    Polish,
    Russian,
    Ukrainian
};

/** @brief Stable settings name and resource locale for an interface language. */
struct InterfaceLanguageDescriptor {
    Language id;
    const char* name;
    const char* code;
};

inline constexpr std::array<InterfaceLanguageDescriptor, 19> kInterfaceLanguages{{
    {Language::English, "English", "en"},
    {Language::French, "French", "fr"},
    {Language::German, "German", "de"},
    {Language::Hindi, "Hindi", "hi"},
    {Language::Italian, "Italian", "it"},
    {Language::Spanish, "Spanish", "es"},
    {Language::Turkish, "Turkish", "tr"},
    {Language::Korean, "Korean", "ko"},
    {Language::SimplifiedChinese, "Simplified Chinese", "zh_cn"},
    {Language::Dutch, "Dutch", "nl"},
    {Language::Swedish, "Swedish", "sv"},
    {Language::Icelandic, "Icelandic", "is"},
    {Language::Norwegian, "Norwegian", "nb"},
    {Language::Finnish, "Finnish", "fi"},
    {Language::Danish, "Danish", "da"},
    {Language::Portuguese, "Portuguese", "pt_pt"},
    {Language::Polish, "Polish", "pl"},
    {Language::Russian, "Russian", "ru"},
    {Language::Ukrainian, "Ukrainian", "uk"},
}};

/** @brief Return the stable settings name.
 * @param language Interface language to encode.
 * @return English settings name, or English for an unknown enum value.
 */
inline QString languageToString(Language language)
{
    for (const auto& entry : kInterfaceLanguages) {
        if (entry.id == language) return QString::fromLatin1(entry.name);
    }
    return QStringLiteral("English");
}

/** @brief Parse a stored name or locale tag without changing existing aliases.
 * @param value English language name or locale such as pt-PT, pl, ru, or uk-UA.
 * @return Matching interface language, or English for an unknown value.
 */
inline Language languageFromString(const QString& value)
{
    QString normalized = value.trimmed().toLower();
    normalized.replace(QLatin1Char('-'), QLatin1Char('_'));
    for (const auto& entry : kInterfaceLanguages) {
        if (normalized == QString::fromLatin1(entry.name).toLower()
            || normalized == QLatin1String(entry.code)) return entry.id;
    }
    struct Alias { const char* name; Language language; };
    constexpr Alias aliases[] = {
        {"simplified_chinese", Language::SimplifiedChinese},
        {"zh", Language::SimplifiedChinese},
        {"zh_hans", Language::SimplifiedChinese},
        {"no", Language::Norwegian},
        {"pt", Language::Portuguese},
        {"portuguese (portugal)", Language::Portuguese},
        {"pl_pl", Language::Polish},
        {"ru_ru", Language::Russian},
        {"uk_ua", Language::Ukrainian},
    };
    for (const auto& alias : aliases) {
        if (normalized == QLatin1String(alias.name)) return alias.language;
    }
    return Language::English;
}

#endif // LANGUAGE_HPP
