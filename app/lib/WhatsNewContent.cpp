#include "WhatsNewContent.hpp"

#include <QFile>
#include <QIODevice>

#include <array>

namespace {

struct WhatsNewLanguageSuffix {
    Language language;
    const char* suffix;
};

constexpr std::array<WhatsNewLanguageSuffix, 14> kWhatsNewLanguageSuffixes{{
    {Language::Dutch, "_nl"},
    {Language::Swedish, "_sv"},
    {Language::Icelandic, "_is"},
    {Language::Norwegian, "_nb"},
    {Language::Finnish, "_fi"},
    {Language::Danish, "_da"},
    {Language::French, "_fr"},
    {Language::German, "_de"},
    {Language::Hindi, "_hi"},
    {Language::Italian, "_it"},
    {Language::Spanish, "_es"},
    {Language::Turkish, "_tr"},
    {Language::Korean, "_ko"},
    {Language::SimplifiedChinese, "_zh_cn"},
}};

QString sanitized_version(const QString& version)
{
    const QString trimmed = version.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    for (const QChar ch : trimmed) {
        if (!ch.isDigit() && ch != QChar('.')) {
            return {};
        }
    }
    return trimmed;
}

QString language_suffix(Language language)
{
    for (const auto& entry : kWhatsNewLanguageSuffixes) {
        if (entry.language == language) {
            return QString::fromLatin1(entry.suffix);
        }
    }
    return {};
}

QString resource_path_for_version(const QString& version, const QString& suffix)
{
    const QString clean_version = sanitized_version(version);
    if (clean_version.isEmpty()) {
        return {};
    }
    return QStringLiteral(":/dev/hfstudio/AIFileSorter/whats_new/%1%2.md")
        .arg(clean_version, suffix);
}

QString load_markdown_resource(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

} // namespace

QString WhatsNewContent::markdown_for_version(const QString& version)
{
    return load_markdown_resource(resource_path_for_version(version, QString()));
}

QString WhatsNewContent::markdown_for_version(const QString& version, Language language)
{
    if (language != Language::English) {
        const QString localized =
            load_markdown_resource(resource_path_for_version(version, language_suffix(language)));
        if (!localized.isEmpty()) {
            return localized;
        }
    }
    return markdown_for_version(version);
}
