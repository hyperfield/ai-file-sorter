#ifndef LANGUAGE_HPP
#define LANGUAGE_HPP

#include <QString>

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
    TraditionalChinese,
    Dutch,
    Swedish,
    Icelandic,
    Norwegian,
    Finnish,
    Danish
};

inline QString languageToString(Language language)
{
    switch (language) {
    case Language::German:
        return QStringLiteral("German");
    case Language::Hindi:
        return QStringLiteral("Hindi");
    case Language::Italian:
        return QStringLiteral("Italian");
    case Language::Spanish:
        return QStringLiteral("Spanish");
    case Language::Turkish:
        return QStringLiteral("Turkish");
    case Language::Korean:
        return QStringLiteral("Korean");
    case Language::SimplifiedChinese:
        return QStringLiteral("Simplified Chinese");
    case Language::TraditionalChinese:
        return QStringLiteral("Traditional Chinese");
    case Language::Dutch:
        return QStringLiteral("Dutch");
    case Language::Swedish:
        return QStringLiteral("Swedish");
    case Language::Icelandic:
        return QStringLiteral("Icelandic");
    case Language::Norwegian:
        return QStringLiteral("Norwegian");
    case Language::Finnish:
        return QStringLiteral("Finnish");
    case Language::Danish:
        return QStringLiteral("Danish");
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
    if (lowered == QStringLiteral("traditional chinese")
        || lowered == QStringLiteral("traditional_chinese")
        || lowered == QStringLiteral("zh-tw")
        || lowered == QStringLiteral("zh_tw")
        || lowered == QStringLiteral("zh-hant")
        || lowered == QStringLiteral("zh_hant")
        || lowered == QStringLiteral("zh-hk")
        || lowered == QStringLiteral("zh_hk")) {
        return Language::TraditionalChinese;
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
    return Language::English;
}

#endif // LANGUAGE_HPP
