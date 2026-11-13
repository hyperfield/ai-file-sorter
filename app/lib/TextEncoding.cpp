#include "TextEncoding.hpp"

#include <QChar>
#include <QList>
#include <QString>
#include <QStringDecoder>
#include <QTextBoundaryFinder>
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

namespace {
std::optional<QString> decode(std::string_view text)
{
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    QString result = decoder(QByteArrayView(text.data(), static_cast<qsizetype>(text.size())));
    if (decoder.hasError()) return std::nullopt;
    return result;
}

bool is_mark(char32_t ch)
{
    const auto category = QChar::category(ch);
    return category == QChar::Mark_NonSpacing || category == QChar::Mark_SpacingCombining ||
           category == QChar::Mark_Enclosing;
}
}

std::optional<double> TextEncoding::parse_number(std::string_view text)
{
    std::istringstream stream{std::string(text)};
    stream.imbue(std::locale::classic());
    double number = 0;
    if (!(stream >> number) || !std::isfinite(number)) return std::nullopt;
    stream >> std::ws;
    if (!stream.eof()) return std::nullopt;
    return number;
}

bool TextEncoding::contains_letter(std::string_view text)
{
    const auto decoded = decode(text);
    if (!decoded) return false;
    const auto scalars = decoded->toUcs4();
    return std::any_of(scalars.begin(), scalars.end(), [](char32_t ch) { return QChar::isLetter(ch); });
}

bool TextEncoding::label_within_limits(std::string_view text)
{
    constexpr qsizetype kMaxLabelGraphemes = 80;
    constexpr qsizetype kMaxComponentUnits = 255;
    const auto decoded = decode(text);
    if (!decoded) return false;
#ifdef _WIN32
    if (decoded->size() > kMaxComponentUnits) return false;
#else
    if (text.size() > static_cast<std::size_t>(kMaxComponentUnits)) return false;
#endif
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, *decoded);
    qsizetype count = 0;
    while (finder.toNextBoundary() != -1) {
        if (++count > kMaxLabelGraphemes) return false;
    }
    return true;
}

std::string TextEncoding::place_slug(std::string_view text)
{
    const auto decoded = decode(text);
    if (!decoded) return {};
    QString output;
    bool separator = true;
    for (char32_t ch : decoded->normalized(QString::NormalizationForm_C).toLower().toUcs4()) {
        if (QChar::isLetterOrNumber(ch) || (!separator && is_mark(ch))) {
            output.append(QString::fromUcs4(&ch, 1));
            separator = false;
        } else if (!separator) {
            output.append(QLatin1Char('_'));
            separator = true;
        }
    }
    if (output.endsWith(QLatin1Char('_'))) output.chop(1);
    return output.toUtf8().toStdString();
}
