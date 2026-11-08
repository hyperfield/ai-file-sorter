#include "ImageFilenamePolicy.hpp"
#include "Utils.hpp"

#include <QString>
#include <QStringList>
#include <unordered_set>
#include <vector>

namespace {
QString sanitize_utf8_text(const std::string& value) {
    QString cleaned = QString::fromUtf8(value.c_str());
    cleaned.remove(QChar::ReplacementCharacter);
    return cleaned.normalized(QString::NormalizationForm_C);
}

std::vector<std::string> split_words(const QString& value) {
    std::vector<std::string> words;
    QString current;
    for (const QChar ch : value) {
        if (ch.isLetterOrNumber()) {
            current.append(ch.toLower());
        } else if (!current.isEmpty()) {
            words.emplace_back(current.toUtf8().toStdString());
            current.clear();
        }
    }
    if (!current.isEmpty()) {
        words.emplace_back(current.toUtf8().toStdString());
    }
    return words;
}

const std::unordered_set<std::string> kStopwords = {
    "a", "an", "and", "are", "as", "at", "based", "be", "by", "category", "describes",
    "description", "depicts", "details", "document", "file", "filename", "for", "from",
    "gif", "has", "image", "in", "is", "it", "jpeg", "jpg", "of", "on", "only",
    "photo", "picture", "png", "shows", "the", "this", "to", "txt", "unknown", "webp", "with"
};

QString clean_model_output(const std::string& value) {
    QString cleaned = sanitize_utf8_text(value).trimmed();
    const QString prefix = QStringLiteral("filename:");
    if (cleaned.startsWith(prefix, Qt::CaseInsensitive)) {
        cleaned = cleaned.mid(prefix.size()).trimmed();
    }
    const int newline = cleaned.indexOf('\n');
    if (newline != -1) {
        cleaned = cleaned.left(newline);
    }
    const int carriage = cleaned.indexOf('\r');
    if (carriage != -1) {
        cleaned = cleaned.left(carriage);
    }
    if (cleaned.size() >= 2) {
        const QChar first = cleaned.front();
        const QChar last = cleaned.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            cleaned = cleaned.mid(1, cleaned.size() - 2);
        }
    }

    return cleaned;
}

QStringList meaningful_words(const QString& value, size_t max_words) {
    QStringList words;
    std::unordered_set<std::string> seen;
    for (const auto& word : split_words(value)) {
        if (kStopwords.count(word) || !seen.insert(word).second) {
            continue;
        }
        words.push_back(QString::fromStdString(word));
        if (static_cast<size_t>(words.size()) >= max_words) {
            break;
        }
    }
    return words;
}

} // namespace

std::string ImageFilenamePolicy::sanitize_filename(const std::string& value,
                                                  size_t max_words,
                                                  size_t max_length) {
    if (max_words == 0 || max_length == 0) {
        return {};
    }
    auto stem = meaningful_words(clean_model_output(value), max_words).join('_');
    if (static_cast<size_t>(stem.size()) > max_length) {
        stem = stem.left(static_cast<qsizetype>(max_length));
        if (!stem.isEmpty() && stem.back().isHighSurrogate()) {
            stem.chop(1);
        }
    }
    while (stem.endsWith('_')) {
        stem.chop(1);
    }
    return stem.toUtf8().toStdString();
}

std::string ImageFilenamePolicy::slugify(const std::string& value) {
    const QString input = sanitize_utf8_text(value);
    QString slug;
    slug.reserve(input.size());
    bool last_sep = false;
    for (const QChar ch : input) {
        if (ch.isLetterOrNumber()) {
            slug.append(ch.toLower());
            last_sep = false;
        } else if (!last_sep && !slug.isEmpty()) {
            slug.append('_');
            last_sep = true;
        }
    }
    if (!slug.isEmpty() && slug.endsWith('_')) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = QStringLiteral("item");
    }
    return slug.toUtf8().toStdString();
}

std::string ImageFilenamePolicy::normalize_filename(const std::string& base,
                                                   const std::filesystem::path& original_path) {
    const std::string ext = Utils::path_to_utf8(original_path.extension());
    if (base.empty()) {
        return Utils::path_to_utf8(original_path.filename());
    }
    return ext.empty() ? base : base + ext;
}
