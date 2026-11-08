#include "DocumentTextAnalyzer.hpp"
#include "ContentAnalysisUnavailable.hpp"
#include "DocumentXmlTextExtractor.hpp"
#include "DocumentSummaryService.hpp"
#include "Logger.hpp"

#include "ILLMClient.hpp"
#include "Utils.hpp"

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#if defined(AI_FILE_SORTER_USE_PDFIUM)
#include "fpdf_doc.h"
#include "fpdf_text.h"
#include "fpdfview.h"
#endif
#if defined(AI_FILE_SORTER_USE_LIBZIP)
#include <zip.h>
#endif


#include <array>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {
constexpr size_t kMaxProcessOutput = 200000;
constexpr std::size_t kZipMemberReadBufferBytes = 4096;
constexpr int kPdfiumTextChunkChars = 4096;

QString path_to_qstring(const std::filesystem::path& path) {
    const std::string utf8 = Utils::path_to_utf8(path);
    return QString::fromUtf8(utf8.c_str(), static_cast<qsizetype>(utf8.size()));
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim_copy(const std::string& value) {
    auto result = value;
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), not_space));
    result.erase(std::find_if(result.rbegin(), result.rend(), not_space).base(), result.end());
    return result;
}

std::string collapse_whitespace(const std::string& value) {
    std::string collapsed;
    collapsed.reserve(value.size());
    bool last_space = false;
    for (unsigned char ch : value) {
        if (std::isspace(ch)) {
            if (!last_space) {
                collapsed.push_back(' ');
                last_space = true;
            }
        } else {
            collapsed.push_back(static_cast<char>(ch));
            last_space = false;
        }
    }
    return trim_copy(collapsed);
}

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

std::optional<std::string> run_process(const QString& program,
                                       const QStringList& args,
                                       int timeout_ms) {
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted()) {
        return std::nullopt;
    }
    if (!process.waitForFinished(timeout_ms)) {
        process.kill();
        process.waitForFinished();
        return std::nullopt;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return std::nullopt;
    }
    const QByteArray output = process.readAllStandardOutput();
    std::string result(output.constData(), static_cast<size_t>(output.size()));
    if (result.size() > kMaxProcessOutput) {
        result.resize(kMaxProcessOutput);
    }
    return result;
}

std::optional<QString> find_executable(const QString& name) {
    const QString exe = QStandardPaths::findExecutable(name);
    if (exe.isEmpty()) {
        return std::nullopt;
    }
    return exe;
}

#if defined(AI_FILE_SORTER_USE_LIBZIP)
std::optional<std::string> extract_zip_member_libzip(const std::filesystem::path& path,
                                                     std::initializer_list<QString> members) {
    int error_code = 0;
    const std::string archive_path = Utils::path_to_utf8(path);
    zip_t* archive = zip_open(archive_path.c_str(), ZIP_RDONLY, &error_code);
    if (!archive) {
        return std::nullopt;
    }
    for (const auto& member : members) {
        const QByteArray member_name = member.toUtf8();
        zip_file_t* file = zip_fopen(archive, member_name.constData(), 0);
        if (!file) {
            continue;
        }
        std::string output;
        output.reserve(kMaxProcessOutput);
        std::array<char, kZipMemberReadBufferBytes> buffer{};
        zip_int64_t read = 0;
        while ((read = zip_fread(file, buffer.data(), buffer.size())) > 0) {
            output.append(buffer.data(), static_cast<size_t>(read));
            if (output.size() >= kMaxProcessOutput) {
                output.resize(kMaxProcessOutput);
                break;
            }
        }
        zip_fclose(file);
        if (!output.empty()) {
            zip_close(archive);
            return output;
        }
    }
    zip_close(archive);
    return std::nullopt;
}
#endif

std::optional<std::string> extract_zip_member(const std::filesystem::path& path,
                                              std::initializer_list<QString> members,
                                              int timeout_ms)
{
#if defined(AI_FILE_SORTER_USE_LIBZIP)
    if (auto output = extract_zip_member_libzip(path, members)) {
        return output;
    }
#endif
    const auto unzip = find_executable(QStringLiteral("unzip"));
    if (!unzip) {
        return std::nullopt;
    }
    const QString file_path = path_to_qstring(path);
    for (const auto& member : members) {
        if (auto output = run_process(*unzip, {QStringLiteral("-p"), file_path, member}, timeout_ms)) {
            if (!output->empty()) {
                return output;
            }
        }
    }
    return std::nullopt;
}

std::string read_file_prefix(const std::filesystem::path& path, size_t max_chars) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::string buffer;
    buffer.resize(max_chars);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    buffer.resize(static_cast<size_t>(file.gcount()));
    buffer.erase(std::remove(buffer.begin(), buffer.end(), '\0'), buffer.end());
    return buffer;
}

std::size_t utf8_prefix_boundary(const std::string& text, std::size_t end)
{
    while (end && end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    return end;
}

std::size_t complete_utf8_size(const std::string& text)
{
    if (text.empty()) return 0;
    auto start = text.size() - 1;
    while (start && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) --start;
    const auto lead = static_cast<unsigned char>(text[start]);
    const std::size_t width = (lead & 0xf8) == 0xf0 ? 4 : (lead & 0xf0) == 0xe0 ? 3 : (lead & 0xe0) == 0xc0 ? 2 : 1;
    return text.size() - start < width ? start : text.size();
}

std::string truncate_excerpt(const std::string& text, size_t max_chars)
{
    constexpr std::string_view kExcerptGap = "\n...\n";
    constexpr std::size_t kHeadNumerator = 3;
    constexpr std::size_t kHeadDenominator = 4;
    // Readers may stop inside a UTF-8 code point; omit that incomplete point.
    const auto size = complete_utf8_size(text);
    if (size <= max_chars) return text.substr(0, size);
    if (max_chars <= kExcerptGap.size()) return text.substr(0, utf8_prefix_boundary(text, max_chars));
    const auto available = max_chars - kExcerptGap.size();
    const auto head = utf8_prefix_boundary(text, available * kHeadNumerator / kHeadDenominator);
    auto tail = size - (available - head);
    while (tail < size && (static_cast<unsigned char>(text[tail]) & 0xc0) == 0x80) ++tail;
    return text.substr(0, head) + std::string(kExcerptGap) + text.substr(tail, size - tail);
}

std::optional<std::string> normalize_date(const std::string& input) {
    std::smatch match;
    static const std::regex kIsoDate(R"((\d{4})-(\d{2})-(\d{2}))");
    if (std::regex_search(input, match, kIsoDate)) {
        return match.str(1) + "-" + match.str(2);
    }
    static const std::regex kPdfDate(R"(D:(\d{4})(\d{2})(\d{2}))");
    if (std::regex_search(input, match, kPdfDate)) {
        return match.str(1) + "-" + match.str(2);
    }
    static const std::regex kCompact(R"(\b(\d{4})(\d{2})(\d{2})\b)");
    if (std::regex_search(input, match, kCompact)) {
        return match.str(1) + "-" + match.str(2);
    }
    return std::nullopt;
}

std::optional<std::string> extract_docx_date(const std::filesystem::path& path) {
    auto xml = extract_zip_member(path, {QStringLiteral("docProps/core.xml")}, 5000);
    if (!xml) {
        return std::nullopt;
    }
    static const std::regex kCreatedTag(R"(<dcterms:created[^>]*>([^<]+)</dcterms:created>)");
    std::smatch match;
    if (std::regex_search(*xml, match, kCreatedTag)) {
        return normalize_date(match.str(1));
    }
    static const std::regex kAltCreatedTag(R"(<cp:created[^>]*>([^<]+)</cp:created>)");
    if (std::regex_search(*xml, match, kAltCreatedTag)) {
        return normalize_date(match.str(1));
    }
    return std::nullopt;
}

std::optional<std::string> extract_pdf_date(const std::filesystem::path& path) {
    const QString file_path = path_to_qstring(path);
    if (const auto pdfinfo = find_executable(QStringLiteral("pdfinfo"))) {
        if (auto output = run_process(*pdfinfo, {file_path}, 4000)) {
            std::istringstream iss(*output);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.rfind("CreationDate", 0) == 0 || line.rfind("Creation Date", 0) == 0) {
                    if (auto parsed = normalize_date(line)) {
                        return parsed;
                    }
                }
            }
        }
    }

    std::string raw = read_file_prefix(path, 200000);
    if (raw.empty()) {
        return std::nullopt;
    }
    static const std::regex kCreation(R"(/CreationDate\s*\(([^\)]*)\))");
    std::smatch match;
    if (std::regex_search(raw, match, kCreation)) {
        return normalize_date(match.str(1));
    }
    return std::nullopt;
}

#if defined(AI_FILE_SORTER_USE_PDFIUM)
class PdfiumLibraryGuard {
public:
    PdfiumLibraryGuard() { FPDF_InitLibrary(); }
    ~PdfiumLibraryGuard() { FPDF_DestroyLibrary(); }
};

PdfiumLibraryGuard& pdfium_library() {
    static PdfiumLibraryGuard guard;
    return guard;
}

std::string extract_pdf_text_pdfium(const std::filesystem::path& path, size_t max_chars) {
    pdfium_library();
    const std::string pdf_path = Utils::path_to_utf8(path);
    FPDF_DOCUMENT doc = FPDF_LoadDocument(pdf_path.c_str(), nullptr);
    if (!doc) {
        return {};
    }
    const int page_count = FPDF_GetPageCount(doc);
    std::string result;
    for (int i = 0; i < page_count && result.size() < max_chars; ++i) {
        FPDF_PAGE page = FPDF_LoadPage(doc, i);
        if (!page) {
            continue;
        }
        FPDF_TEXTPAGE text_page = FPDFText_LoadPage(page);
        if (!text_page) {
            FPDF_ClosePage(page);
            continue;
        }
        const int total_chars = FPDFText_CountChars(text_page);
        const int chunk = kPdfiumTextChunkChars;
        int offset = 0;
        while (offset < total_chars && result.size() < max_chars) {
            const int take = std::min(chunk, total_chars - offset);
            std::vector<unsigned short> buffer(static_cast<size_t>(take + 1));
            const int extracted = FPDFText_GetText(text_page, offset, take, buffer.data());
            if (extracted <= 0) {
                break;
            }
            const int count = std::max(0, extracted - 1);
            QString text = QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()), count);
            const QByteArray bytes = text.toUtf8();
            result.append(bytes.constData(), static_cast<size_t>(bytes.size()));
            if (result.size() >= max_chars) {
                result.resize(max_chars);
                break;
            }
            offset += take;
        }
        FPDFText_ClosePage(text_page);
        FPDF_ClosePage(page);
    }
    FPDF_CloseDocument(doc);
    return result;
}
#endif

std::string extract_pdf_text(const std::filesystem::path& path, std::size_t max_bytes)
{
#if defined(AI_FILE_SORTER_USE_PDFIUM)
    if (auto text = extract_pdf_text_pdfium(path, max_bytes); !text.empty()) return collapse_whitespace(text);
#endif
    const auto pdftotext = find_executable(QStringLiteral("pdftotext"));
    if (!pdftotext) return {};
    constexpr int kPdfTextTimeoutMs = 15000;
    auto text = run_process(*pdftotext,
        {QStringLiteral("-layout"), QStringLiteral("-q"), path_to_qstring(path), QStringLiteral("-")}, kPdfTextTimeoutMs);
    if (!text) return {};
    if (text->size() > max_bytes) text->resize(max_bytes);
    return collapse_whitespace(*text);
}

const std::unordered_set<std::string> kDocumentExtensions = {
    ".txt", ".md", ".markdown", ".rtf", ".csv", ".tsv", ".log", ".json", ".xml", ".yml", ".yaml",
    ".ini", ".cfg", ".conf", ".html", ".htm", ".tex", ".rst", ".pdf", ".docx", ".xlsx", ".pptx",
    ".odt", ".ods", ".odp"
};

const std::unordered_set<std::string> kTextExtensions = {
    ".txt", ".md", ".markdown", ".rtf", ".csv", ".tsv", ".log", ".json", ".xml", ".yml", ".yaml",
    ".ini", ".cfg", ".conf", ".html", ".htm", ".tex", ".rst"
};

const std::unordered_set<std::string> kStopwords = {
    "a", "an", "and", "are", "as", "at", "based", "be", "by", "category", "chapter",
    "description", "details", "document", "documents", "file", "files", "filename", "for",
    "from", "has", "in", "is", "it", "of", "on", "only", "page", "pages", "pdf", "doc",
    "docx", "rtf", "text", "this", "to", "txt", "with", "report", "notes", "note", "summary",
    "overview", "information", "data", "untitled", "ppt", "pptx", "xls", "xlsx", "odt", "ods",
    "odp", "presentation", "slides", "spreadsheet"
};

} // namespace

DocumentTextAnalyzer::DocumentTextAnalyzer()
    : DocumentTextAnalyzer(Settings{})
{}

DocumentTextAnalyzer::DocumentTextAnalyzer(Settings settings)
    : settings_(settings) {
    if (settings_.max_characters == 0) {
        settings_.max_characters = kDefaultDocumentAnalyzerMaxCharacters;
    }
    if (settings_.max_tokens <= 0) {
        settings_.max_tokens = kDefaultDocumentAnalyzerMaxTokens;
    }
    if (settings_.max_filename_words == 0) {
        settings_.max_filename_words = kDefaultDocumentAnalyzerMaxFilenameWords;
    }
    if (settings_.max_filename_length == 0) {
        settings_.max_filename_length = kDefaultDocumentAnalyzerMaxFilenameLength;
    }
}

DocumentAnalysisResult DocumentTextAnalyzer::analyze(const std::filesystem::path& document_path,
                                                     ILLMClient& llm) const
{
    DocumentAnalysisResult result;
    const auto check_stopped = [&] {
        if (settings_.should_stop && settings_.should_stop()) throw std::runtime_error("Document analysis cancelled.");
    };
    check_stopped();
    const std::string raw_text = extract_text(document_path);
    if (raw_text.empty()) {
        throw ContentAnalysisUnavailable("No extractable text");
    }

    const std::string excerpt = truncate_excerpt(raw_text, settings_.max_characters);
    const auto file_name = Utils::path_to_utf8(document_path.filename());
    const auto analysis = DocumentSummaryService::summarize(excerpt, llm,
        [&](const std::string& text, bool merging) { return build_prompt(text, file_name, merging); },
        settings_.max_tokens, settings_.should_stop);
    const auto& summary = analysis.summary;
    const auto& filename = analysis.filename;
    result.summary_requests = analysis.requests;
    result.summarized_chunks = analysis.source_chunks;
    result.used_extractive_fallback = analysis.used_extractive_fallback;
    check_stopped();

    std::string sanitized = sanitize_filename(filename, settings_.max_filename_words, settings_.max_filename_length);
    if (sanitized.empty()) {
        sanitized = sanitize_filename(summary, settings_.max_filename_words, settings_.max_filename_length);
    }
    if (sanitized.empty()) {
        sanitized = "document_" + slugify(Utils::path_to_utf8(document_path.stem()));
    }

    result.summary = summary;
    result.suggested_name = normalize_filename(sanitized, document_path);
    if (result.used_extractive_fallback) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Using bounded extracted text as document evidence for '{}' after {} summary attempts",
                         file_name, result.summary_requests);
        }
    }
    return result;
}

bool DocumentTextAnalyzer::is_supported_document(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return false;
    }
    const std::string ext = to_lower_copy(Utils::path_to_utf8(path.extension()));
    return kDocumentExtensions.find(ext) != kDocumentExtensions.end();
}

std::optional<std::string> DocumentTextAnalyzer::extract_creation_date(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return std::nullopt;
    }
    const std::string ext = to_lower_copy(Utils::path_to_utf8(path.extension()));
    if (ext == ".pdf") {
        return extract_pdf_date(path);
    }
    if (ext == ".docx") {
        return extract_docx_date(path);
    }
    return std::nullopt;
}

std::string DocumentTextAnalyzer::extract_text(const std::filesystem::path& path) const {
    if (!path.has_extension()) {
        return {};
    }
    const std::string ext = to_lower_copy(Utils::path_to_utf8(path.extension()));

    if (kTextExtensions.find(ext) != kTextExtensions.end()) {
        std::string text = read_file_prefix(path, settings_.max_characters);
        return collapse_whitespace(text);
    }

    if (ext == ".pdf") {
        return extract_pdf_text(path, settings_.max_characters);
    }

    if (ext == ".docx") {
        return DocumentXmlTextExtractor::extract(path, {"word/document.xml"}, settings_.max_characters, settings_.should_stop);
    }
    if (ext == ".xlsx") {
        return DocumentXmlTextExtractor::extract(path,
            {"xl/sharedStrings.xml", "xl/worksheets/sheet1.xml", "xl/worksheets/sheet2.xml"},
            settings_.max_characters, settings_.should_stop);
    }
    if (ext == ".pptx") {
        return DocumentXmlTextExtractor::extract(path, {"ppt/slides/slide1.xml", "ppt/slides/slide2.xml"},
            settings_.max_characters, settings_.should_stop);
    }
    if (ext == ".odt" || ext == ".ods" || ext == ".odp") {
        return DocumentXmlTextExtractor::extract(path, {"content.xml"}, settings_.max_characters, settings_.should_stop);
    }

    return {};
}

std::string DocumentTextAnalyzer::build_prompt(const std::string& excerpt,
                                               const std::string& file_name, bool merging) const {
    std::ostringstream oss;
    oss << (merging ? "Combine these summaries of parts of one document into a summary of at most 120 words. "
                    : "Summarize the document excerpt below in at most 120 words. ");
    oss << "Then propose a short descriptive filename (max " << settings_.max_filename_words
        << " words). "
        << "Use concise descriptive words that identify the document's subject. "
        << "Use English descriptive words regardless of the document's language. "
        << "Preserve proper nouns and identifiers without translating them. "
        << "Use underscores between words, avoid generic words like 'document', 'file', or extensions. "
        << "Return JSON only in the format: {\"summary\":\"...\",\"filename\":\"...\"}.\n\n";
    oss << "Document filename: " << file_name << "\n";
    oss << (merging ? "Part summaries:\n" : "Document excerpt:\n") << excerpt << "\n\n";
    oss << "JSON:";
    return oss.str();
}

std::string DocumentTextAnalyzer::sanitize_filename(const std::string& value,
                                                    size_t max_words,
                                                    size_t max_length) const {
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
    std::vector<std::string> filtered;
    filtered.reserve(words.size());
    std::unordered_set<std::string> seen;
    for (const auto& word : words) {
        if (word.empty()) {
            continue;
        }
        if (kStopwords.find(word) != kStopwords.end()) {
            continue;
        }
        if (seen.insert(word).second) {
            filtered.push_back(word);
        }
        if (filtered.size() >= max_words) {
            break;
        }
    }

    if (filtered.empty()) {
        return std::string();
    }

    QString joined;
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (i > 0) {
            joined.append('_');
        }
        joined.append(QString::fromUtf8(filtered[i].c_str()));
    }

    if (joined.size() > static_cast<int>(max_length)) {
        joined = joined.left(static_cast<int>(max_length));
    }
    while (!joined.isEmpty() && joined.endsWith('_')) {
        joined.chop(1);
    }

    return joined.toUtf8().toStdString();
}

std::string DocumentTextAnalyzer::trim(std::string value) {
    return trim_copy(value);
}

std::string DocumentTextAnalyzer::slugify(const std::string& value) {
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
        slug = QStringLiteral("document");
    }
    return slug.toUtf8().toStdString();
}

std::string DocumentTextAnalyzer::normalize_filename(const std::string& base,
                                                     const std::filesystem::path& original_path) {
    const std::string ext = Utils::path_to_utf8(original_path.extension());
    if (base.empty()) {
        return Utils::path_to_utf8(original_path.filename());
    }
    return ext.empty() ? base : base + ext;
}
