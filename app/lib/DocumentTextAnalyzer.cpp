#include "DocumentTextAnalyzer.hpp"

#include "ILLMClient.hpp"

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#else
#error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {
constexpr size_t kDefaultMaxChars = 8000;
constexpr int kDefaultMaxTokens = 256;
constexpr size_t kMaxProcessOutput = 200000;

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

std::vector<std::string> split_words(const std::string& value) {
    std::vector<std::string> words;
    std::string current;
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            current.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!current.empty()) {
            words.emplace_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        words.emplace_back(std::move(current));
    }
    return words;
}

std::string strip_xml_tags(const std::string& xml) {
    std::string output;
    output.reserve(xml.size());
    bool in_tag = false;
    for (char ch : xml) {
        if (ch == '<') {
            in_tag = true;
            continue;
        }
        if (ch == '>') {
            in_tag = false;
            continue;
        }
        if (!in_tag) {
            output.push_back(ch);
        }
    }
    return output;
}

void replace_all(std::string& value, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return;
    }
    size_t pos = 0;
    while ((pos = value.find(from.data(), pos, from.size())) != std::string::npos) {
        value.replace(pos, from.size(), to.data(), to.size());
        pos += to.size();
    }
}

std::string decode_basic_entities(std::string value) {
    replace_all(value, "&amp;", "&");
    replace_all(value, "&lt;", "<");
    replace_all(value, "&gt;", ">");
    replace_all(value, "&quot;", "\"");
    replace_all(value, "&apos;", "'");
    return value;
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

std::string truncate_excerpt(const std::string& text, size_t max_chars) {
    if (text.size() <= max_chars) {
        return text;
    }
    const size_t head = max_chars * 3 / 4;
    const size_t tail = max_chars - head;
    return text.substr(0, head) + "\n...\n" + text.substr(text.size() - tail);
}

std::optional<std::string> normalize_date(const std::string& input) {
    std::smatch match;
    static const std::regex kIsoDate(R"((\d{4})-(\d{2})-(\d{2}))");
    if (std::regex_search(input, match, kIsoDate)) {
        return match.str(1) + "-" + match.str(2) + "-" + match.str(3);
    }
    static const std::regex kPdfDate(R"(D:(\d{4})(\d{2})(\d{2}))");
    if (std::regex_search(input, match, kPdfDate)) {
        return match.str(1) + "-" + match.str(2) + "-" + match.str(3);
    }
    static const std::regex kCompact(R"(\b(\d{4})(\d{2})(\d{2})\b)");
    if (std::regex_search(input, match, kCompact)) {
        return match.str(1) + "-" + match.str(2) + "-" + match.str(3);
    }
    return std::nullopt;
}

std::optional<std::string> extract_docx_date(const std::filesystem::path& path) {
    const auto unzip = find_executable(QStringLiteral("unzip"));
    if (!unzip) {
        return std::nullopt;
    }
    const QString file_path = QString::fromStdString(path.string());
    auto xml = run_process(*unzip,
                           {QStringLiteral("-p"), file_path, QStringLiteral("docProps/core.xml")},
                           5000);
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
    const QString file_path = QString::fromStdString(path.string());
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

const std::unordered_set<std::string> kDocumentExtensions = {
    ".txt", ".md", ".markdown", ".rtf", ".csv", ".tsv", ".log", ".json", ".xml", ".yml", ".yaml",
    ".ini", ".cfg", ".conf", ".html", ".htm", ".tex", ".rst", ".pdf", ".docx"
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
    "overview", "information", "data", "untitled"
};

struct ParsedAnalysis {
    std::string summary;
    std::string filename;
};

std::optional<ParsedAnalysis> parse_analysis_json(const std::string& response) {
    Json::CharReaderBuilder reader;
    Json::Value root;
    std::string errors;

    auto try_parse = [&](const std::string& payload) -> bool {
        std::istringstream stream(payload);
        return Json::parseFromStream(reader, stream, &root, &errors);
    };

    if (!try_parse(response)) {
        const auto first = response.find('{');
        const auto last = response.rfind('}');
        if (first == std::string::npos || last == std::string::npos || last <= first) {
            return std::nullopt;
        }
        if (!try_parse(response.substr(first, last - first + 1))) {
            return std::nullopt;
        }
    }

    if (!root.isObject()) {
        return std::nullopt;
    }

    ParsedAnalysis parsed;
    if (root.isMember("summary")) {
        parsed.summary = root["summary"].asString();
    } else if (root.isMember("description")) {
        parsed.summary = root["description"].asString();
    }

    if (root.isMember("filename")) {
        parsed.filename = root["filename"].asString();
    } else if (root.isMember("name")) {
        parsed.filename = root["name"].asString();
    } else if (root.isMember("suggested_name")) {
        parsed.filename = root["suggested_name"].asString();
    }

    return parsed;
}

std::string first_words(const std::string& text, size_t max_words) {
    std::istringstream iss(text);
    std::string word;
    std::string result;
    size_t count = 0;
    while (iss >> word) {
        if (!result.empty()) {
            result.push_back(' ');
        }
        result += word;
        if (++count >= max_words) {
            break;
        }
    }
    return result;
}

} // namespace

DocumentTextAnalyzer::DocumentTextAnalyzer()
    : DocumentTextAnalyzer(Settings{})
{}

DocumentTextAnalyzer::DocumentTextAnalyzer(Settings settings)
    : settings_(settings) {
    if (settings_.max_characters == 0) {
        settings_.max_characters = kDefaultMaxChars;
    }
    if (settings_.max_tokens <= 0) {
        settings_.max_tokens = kDefaultMaxTokens;
    }
    if (settings_.max_filename_words == 0) {
        settings_.max_filename_words = 3;
    }
    if (settings_.max_filename_length == 0) {
        settings_.max_filename_length = 50;
    }
}

DocumentAnalysisResult DocumentTextAnalyzer::analyze(const std::filesystem::path& document_path,
                                                     ILLMClient& llm) const
{
    DocumentAnalysisResult result;
    const std::string raw_text = extract_text(document_path);
    if (raw_text.empty()) {
        throw std::runtime_error("No extractable text");
    }

    const std::string excerpt = truncate_excerpt(raw_text, settings_.max_characters);
    const std::string prompt = build_prompt(excerpt, document_path.filename().string());
    const std::string response = llm.complete_prompt(prompt, settings_.max_tokens);

    std::string summary;
    std::string filename;
    if (auto parsed = parse_analysis_json(response)) {
        summary = trim(parsed->summary);
        filename = trim(parsed->filename);
    }

    if (summary.empty()) {
        summary = trim(first_words(excerpt, 120));
    }

    std::string sanitized = sanitize_filename(filename, settings_.max_filename_words, settings_.max_filename_length);
    if (sanitized.empty()) {
        sanitized = sanitize_filename(summary, settings_.max_filename_words, settings_.max_filename_length);
    }
    if (sanitized.empty()) {
        sanitized = "document_" + slugify(document_path.stem().string());
    }

    result.summary = summary;
    result.suggested_name = normalize_filename(sanitized, document_path);
    return result;
}

bool DocumentTextAnalyzer::is_supported_document(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return false;
    }
    const std::string ext = to_lower_copy(path.extension().string());
    return kDocumentExtensions.find(ext) != kDocumentExtensions.end();
}

std::optional<std::string> DocumentTextAnalyzer::extract_creation_date(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return std::nullopt;
    }
    const std::string ext = to_lower_copy(path.extension().string());
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
    const std::string ext = to_lower_copy(path.extension().string());

    if (kTextExtensions.find(ext) != kTextExtensions.end()) {
        std::string text = read_file_prefix(path, settings_.max_characters);
        return collapse_whitespace(text);
    }

    if (ext == ".pdf") {
        const auto pdftotext = find_executable(QStringLiteral("pdftotext"));
        if (!pdftotext) {
            return {};
        }
        const QString file_path = QString::fromStdString(path.string());
        auto output = run_process(*pdftotext,
                                  {QStringLiteral("-layout"), QStringLiteral("-q"), file_path, QStringLiteral("-")},
                                  15000);
        if (!output) {
            return {};
        }
        if (output->size() > settings_.max_characters) {
            output->resize(settings_.max_characters);
        }
        return collapse_whitespace(*output);
    }

    if (ext == ".docx") {
        const auto unzip = find_executable(QStringLiteral("unzip"));
        if (!unzip) {
            return {};
        }
        const QString file_path = QString::fromStdString(path.string());
        auto xml = run_process(*unzip,
                               {QStringLiteral("-p"), file_path, QStringLiteral("word/document.xml")},
                               7000);
        if (!xml) {
            return {};
        }
        std::string text = strip_xml_tags(*xml);
        text = decode_basic_entities(text);
        if (text.size() > settings_.max_characters) {
            text.resize(settings_.max_characters);
        }
        return collapse_whitespace(text);
    }

    return {};
}

std::string DocumentTextAnalyzer::build_prompt(const std::string& excerpt,
                                               const std::string& file_name) const {
    std::ostringstream oss;
    oss << "Summarize the document excerpt below in at most 120 words. "
        << "Then propose a short descriptive filename (max 3 words, nouns only). "
        << "Use underscores between words, avoid generic words like 'document', 'file', or extensions. "
        << "Return JSON only in the format: {\"summary\":\"...\",\"filename\":\"...\"}.\n\n";
    oss << "Document filename: " << file_name << "\n";
    oss << "Document excerpt:\n" << excerpt << "\n\n";
    oss << "JSON:";
    return oss.str();
}

std::string DocumentTextAnalyzer::sanitize_filename(const std::string& value,
                                                    size_t max_words,
                                                    size_t max_length) const {
    std::string cleaned = trim_copy(value);
    const std::string lower = to_lower_copy(cleaned);
    const std::string prefix = "filename:";
    if (lower.rfind(prefix, 0) == 0) {
        cleaned = trim_copy(cleaned.substr(prefix.size()));
    }
    const auto newline = cleaned.find('\n');
    if (newline != std::string::npos) {
        cleaned = cleaned.substr(0, newline);
    }
    if (cleaned.size() >= 2 && ((cleaned.front() == '"' && cleaned.back() == '"') ||
                                (cleaned.front() == '\'' && cleaned.back() == '\''))) {
        cleaned = cleaned.substr(1, cleaned.size() - 2);
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

    std::string joined;
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (i > 0) {
            joined.push_back('_');
        }
        joined += filtered[i];
    }

    if (joined.size() > max_length) {
        joined.resize(max_length);
    }
    while (!joined.empty() && joined.back() == '_') {
        joined.pop_back();
    }

    return joined;
}

std::string DocumentTextAnalyzer::trim(std::string value) {
    return trim_copy(value);
}

std::string DocumentTextAnalyzer::slugify(const std::string& value) {
    std::string slug;
    slug.reserve(value.size());
    bool last_sep = false;
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            last_sep = false;
        } else if (!last_sep && !slug.empty()) {
            slug.push_back('_');
            last_sep = true;
        }
    }
    if (!slug.empty() && slug.back() == '_') {
        slug.pop_back();
    }
    if (slug.empty()) {
        slug = "document";
    }
    return slug;
}

std::string DocumentTextAnalyzer::normalize_filename(const std::string& base,
                                                     const std::filesystem::path& original_path) {
    const std::string ext = original_path.extension().string();
    if (base.empty()) {
        return original_path.filename().string();
    }
    return ext.empty() ? base : base + ext;
}
