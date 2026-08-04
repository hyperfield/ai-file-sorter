#include "FilenameLocalizationService.hpp"

#include "ILLMClient.hpp"
#include "Utils.hpp"

#include <QString>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <spdlog/logger.h>

namespace {

constexpr int kFilenameLocalizationCompletionTokens = 96;
constexpr std::size_t kMinimumLocalizedFilenameWords = 1;
constexpr std::size_t kMaximumLocalizedFilenameWords = 12;
constexpr std::size_t kMinimumLocalizedFilenameLength = 24;
constexpr std::size_t kMaximumLocalizedFilenameLength = 96;
constexpr std::size_t kLocalizedFilenameLengthSlack = 24;

QString sanitize_utf8_text(const std::string& value)
{
    QString cleaned = QString::fromUtf8(value.c_str());
    cleaned.remove(QChar::ReplacementCharacter);
    return cleaned.normalized(QString::NormalizationForm_C);
}

std::vector<std::string> split_words(const QString& value)
{
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

std::string trim_copy(const std::string& value)
{
    auto result = value;
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), not_space));
    result.erase(std::find_if(result.rbegin(), result.rend(), not_space).base(), result.end());
    return result;
}

std::size_t derived_max_words(const std::string& stem, std::size_t configured_max_words)
{
    if (configured_max_words > 0) {
        return configured_max_words;
    }

    const auto words = split_words(sanitize_utf8_text(stem));
    const std::size_t source_word_count = std::max<std::size_t>(words.size(),
                                                                kMinimumLocalizedFilenameWords);
    return std::clamp(source_word_count,
                      kMinimumLocalizedFilenameWords,
                      kMaximumLocalizedFilenameWords);
}

std::size_t derived_max_length(const QString& stem, std::size_t configured_max_length)
{
    if (configured_max_length > 0) {
        return configured_max_length;
    }

    const std::size_t source_length = static_cast<std::size_t>(stem.size());
    return std::clamp(source_length + kLocalizedFilenameLengthSlack,
                      kMinimumLocalizedFilenameLength,
                      kMaximumLocalizedFilenameLength);
}

} // namespace

FilenameLocalizationService::FilenameLocalizationService(std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger))
{
}

std::string FilenameLocalizationService::localize_filename(const std::string& suggested_name,
                                                           CategoryLanguage language,
                                                           ILLMClient& llm) const
{
    return localize_filename(suggested_name, language, llm, Options{});
}

std::string FilenameLocalizationService::localize_filename(const std::string& suggested_name,
                                                           CategoryLanguage language,
                                                           ILLMClient& llm,
                                                           Options options) const
{
    if (suggested_name.empty() || language == CategoryLanguage::English) {
        return suggested_name;
    }

    const auto source_path = Utils::utf8_to_path(suggested_name);
    const std::string extension = Utils::path_to_utf8(source_path.extension());
    const std::string stem = source_path.has_stem()
        ? Utils::path_to_utf8(source_path.stem())
        : trim_copy(suggested_name);
    if (stem.empty()) {
        return suggested_name;
    }

    const QString stem_text = sanitize_utf8_text(stem);
    const std::size_t max_words = derived_max_words(stem, options.max_words);
    const std::size_t max_length = derived_max_length(stem_text, options.max_length);
    const std::string prompt = build_prompt(stem, language, max_words);

    try {
        const std::string response =
            llm.complete_prompt(prompt, kFilenameLocalizationCompletionTokens);
        const std::string localized_stem = sanitize_stem(response, max_words, max_length);
        if (localized_stem.empty()) {
            return suggested_name;
        }
        return extension.empty() ? localized_stem : localized_stem + extension;
    } catch (const std::exception& ex) {
        if (logger_) {
            logger_->warn("Filename localization failed for '{}' to {}: {}",
                          suggested_name,
                          categoryLanguageDisplay(language),
                          ex.what());
        }
        return suggested_name;
    }
}

std::string FilenameLocalizationService::build_prompt(const std::string& stem,
                                                      CategoryLanguage language,
                                                      std::size_t max_words) const
{
    return fmt::format(
        "Translate the following filename stem into {}.\n"
        "Return only the translated filename stem.\n"
        "Rules:\n"
        "- Preserve the original meaning.\n"
        "- Keep proper nouns unchanged when appropriate.\n"
        "- Use lowercase words separated by underscores.\n"
        "- Do not include a file extension.\n"
        "- Do not include quotes, JSON, or explanations.\n"
        "- If the stem is already natural in {}, keep it unchanged.\n"
        "- Use no more than {} words.\n\n"
        "filename: {}",
        categoryLanguageDisplay(language),
        categoryLanguageDisplay(language),
        max_words,
        stem);
}

std::string FilenameLocalizationService::sanitize_stem(const std::string& value,
                                                       std::size_t max_words,
                                                       std::size_t max_length)
{
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

    auto words = split_words(cleaned);
    if (words.empty()) {
        return std::string();
    }
    if (max_words > 0 && words.size() > max_words) {
        words.resize(max_words);
    }

    QString joined;
    for (std::size_t index = 0; index < words.size(); ++index) {
        if (index > 0) {
            joined.append('_');
        }
        joined.append(QString::fromUtf8(words[index].c_str()));
    }

    if (joined.size() > static_cast<qsizetype>(max_length)) {
        joined = joined.left(static_cast<qsizetype>(max_length));
    }
    while (!joined.isEmpty() && joined.endsWith('_')) {
        joined.chop(1);
    }

    return joined.toUtf8().toStdString();
}
