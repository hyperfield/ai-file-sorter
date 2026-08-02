#ifndef LANGUAGE_HPP
#define LANGUAGE_HPP

#include <QString>

#include <array>
#include <cstddef>

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
    Ukrainian
};

inline QString languageToString(Language language)
{
    // Indexed by declaration order in the Language enum above.
    static constexpr std::array<const char*, 16> kLanguageNames = {
        "English", "French", "German", "Hindi", "Italian", "Spanish",
        "Turkish", "Korean", "Simplified Chinese", "Dutch", "Swedish",
        "Icelandic", "Norwegian", "Finnish", "Danish", "Ukrainian",
    };

    const auto index = static_cast<std::size_t>(language);
    if (index < kLanguageNames.size()) {
        return QString::fromLatin1(kLanguageNames[index]);
    }
    return QStringLiteral("English");
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
    if (lowered == QStringLiteral("hindi") || lowered == QStringLiteral("hi")) {
        return Language::Hindi;
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
    if (lowered == QStringLiteral("korean") || lowered == QStringLiteral("ko")) {
        return Language::Korean;
    }
    if (lowered == QStringLiteral("simplified chinese")
        || lowered == QStringLiteral("simplified_chinese")
        || lowered == QStringLiteral("zh")
        || lowered == QStringLiteral("zh-cn")
        || lowered == QStringLiteral("zh_cn")
        || lowered == QStringLiteral("zh-hans")
        || lowered == QStringLiteral("zh_hans")) {
        return Language::SimplifiedChinese;
    }
    if (lowered == QStringLiteral("dutch") || lowered == QStringLiteral("nl")) {
        return Language::Dutch;
    }
    if (lowered == QStringLiteral("swedish") || lowered == QStringLiteral("sv")) {
        return Language::Swedish;
    }
    if (lowered == QStringLiteral("icelandic") || lowered == QStringLiteral("is")) {
        return Language::Icelandic;
    }
    if (lowered == QStringLiteral("norwegian") || lowered == QStringLiteral("nb")
        || lowered == QStringLiteral("no")) {
        return Language::Norwegian;
    }
    if (lowered == QStringLiteral("finnish") || lowered == QStringLiteral("fi")) {
        return Language::Finnish;
    }
    if (lowered == QStringLiteral("danish") || lowered == QStringLiteral("da")) {
        return Language::Danish;
    }
    if (lowered == QStringLiteral("ukrainian") || lowered == QStringLiteral("uk")) {
        return Language::Ukrainian;
    }
    return Language::English;
}

#endif // LANGUAGE_HPP
