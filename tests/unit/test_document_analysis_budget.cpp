#include "DocumentTextAnalyzer.hpp"
#include "ContentAnalysisUnavailable.hpp"
#include "ILLMClient.hpp"
#include "LLMErrors.hpp"
#include "TestHelpers.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fstream>
#include <functional>
#include <vector>

#if defined(AI_FILE_SORTER_USE_LIBZIP)
#include <zip.h>
#endif

namespace {
std::string input_evidence(const std::string& prompt)
{
    const auto marker = prompt.find("Document excerpt:\n");
    const auto notes = prompt.find("Part summaries:\n");
    const auto start = marker != std::string::npos ? marker + 18 : notes + 16;
    return prompt.substr(start, prompt.size() - start - 7); // Final \n\nJSON: footer.
}

class DocumentClient final : public ILLMClient {
public:
    std::size_t baseline = 2048, maximum = 8192, allocation_limit = 8192;
    bool expose_budget = true;
    int rejected = 0;
    std::vector<std::string> prompts;
    std::vector<std::size_t> contexts;
    std::function<void()> after_completion;
    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override { return {}; }
    void set_prompt_logging_enabled(bool) override {}
    std::optional<CompletionBudget> completion_budget(const std::string& prompt, int response) const override {
        if (!expose_budget) return std::nullopt;
        // Dense-token fixture includes framing outside the user message.
        return CompletionBudget{prompt.size() + 128, baseline, maximum, static_cast<std::size_t>(response)};
    }
    std::string complete_document_prompt(const std::string& prompt, int response) override {
        if (prompt.size() + 128 + response > allocation_limit) {
            ++rejected;
            maximum = allocation_limit;
            throw PromptBudgetError("Simulated context allocation fallback", allocation_limit > response ? allocation_limit - response : 0);
        }
        std::size_t context = baseline;
        while (context < prompt.size() + 128 + response) context *= 2;
        contexts.push_back(context);
        return complete_prompt(prompt, response);
    }
    std::string complete_prompt(const std::string& prompt, int) override {
        prompts.push_back(prompt);
        const auto evidence = input_evidence(prompt);
        CHECK_FALSE(QString::fromStdString(evidence).contains(QChar::ReplacementCharacter));
        std::string summary = "Document evidence";
        if (evidence.find("HEAD_EVIDENCE") != std::string::npos) summary += " HEAD_EVIDENCE";
        if (evidence.find("TAIL_EVIDENCE") != std::string::npos) summary += " TAIL_EVIDENCE";
        if (after_completion) after_completion();
        return QJsonDocument(QJsonObject{{"summary", QString::fromStdString(summary)}, {"filename", "privacy_policy"}})
            .toJson(QJsonDocument::Compact).toStdString();
    }
};

std::filesystem::path source_file(const TempDir& temp, const std::string& text)
{
    const auto path = temp.path() / "source.txt";
    std::ofstream(path, std::ios::binary) << text;
    return path;
}

#if defined(AI_FILE_SORTER_USE_LIBZIP)
void archive_file(const std::filesystem::path& path, const char* member, const std::string& xml)
{
    auto* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, nullptr);
    REQUIRE(archive);
    auto* source = zip_source_buffer(archive, xml.data(), xml.size(), 0);
    REQUIRE(source);
    REQUIRE(zip_file_add(archive, member, source, 0) >= 0);
    REQUIRE(zip_close(archive) == 0);
}
#endif
} // namespace

TEST_CASE("Document summaries request adaptive completion without changing ordinary context", "[document-budget]")
{
    TempDir temp;
    const std::string text = "HEAD_EVIDENCE " + std::string(2600, '7') + " TAIL_EVIDENCE";
    DocumentClient client;
    const auto result = DocumentTextAnalyzer().analyze(source_file(temp, text), client);
    REQUIRE(client.prompts.size() == 1);
    CHECK(client.contexts == std::vector<std::size_t>{4096});
    CHECK(client.baseline == 2048);
    CHECK(result.summary_requests == 1);
    CHECK(result.summarized_chunks == 1);
    CHECK_FALSE(result.used_extractive_fallback);
    CHECK(result.summary.find("TAIL_EVIDENCE") != std::string::npos);
}

TEST_CASE("Document summaries cover every UTF-8 chunk within full prompt budgets", "[document-budget]")
{
    const bool allocation_fallback = GENERATE(false, true);
    TempDir temp;
    std::string text = "HEAD_EVIDENCE ";
    for (int i = 0; i < 500; ++i) text += "日本語 123456 ";
    text += "TAIL_EVIDENCE";
    DocumentTextAnalyzer::Settings settings;
    settings.max_characters = text.size();
    DocumentClient client;
    client.maximum = allocation_fallback ? 16384 : 2048;
    client.allocation_limit = 2048;
    const auto result = DocumentTextAnalyzer(settings).analyze(source_file(temp, text), client);
    std::string reconstructed;
    std::size_t source_chunks = 0;
    for (const auto& prompt : client.prompts) {
        CHECK(prompt.size() + 128 + 256 <= 2048);
        CHECK(prompt.find(temp.path().string()) == std::string::npos);
        if (prompt.find("Document excerpt:\n") != std::string::npos) {
            reconstructed += input_evidence(prompt);
            ++source_chunks;
        }
    }
    CHECK(reconstructed == text);
    CHECK(source_chunks > 1);
    CHECK(result.summarized_chunks == source_chunks);
    CHECK(result.summary.find("HEAD_EVIDENCE") != std::string::npos);
    CHECK(result.summary.find("TAIL_EVIDENCE") != std::string::npos);
    CHECK(result.suggested_name == "privacy_policy.txt");
    CHECK_FALSE(result.used_extractive_fallback);
    CHECK(client.rejected == (allocation_fallback ? 1 : 0));
}

TEST_CASE("Document summaries recover from budget rejections without provider token metadata", "[document-budget]")
{
    TempDir temp;
    const std::string text = "HEAD_EVIDENCE " + std::string(3000, '7') + " TAIL_EVIDENCE";
    DocumentClient client;
    client.expose_budget = false;
    client.allocation_limit = 2048;
    const auto result = DocumentTextAnalyzer().analyze(source_file(temp, text), client);
    CHECK(client.rejected > 0);
    CHECK(result.summarized_chunks > 1);
    CHECK(result.summary.find("HEAD_EVIDENCE") != std::string::npos);
    CHECK(result.summary.find("TAIL_EVIDENCE") != std::string::npos);
    CHECK_FALSE(result.used_extractive_fallback);
}

TEST_CASE("Document evidence remains available when instructions alone exceed model capacity", "[document-budget]")
{
    TempDir temp;
    DocumentClient client;
    client.maximum = 256;
    const auto result = DocumentTextAnalyzer().analyze(source_file(temp, "HEAD_EVIDENCE important financial report"), client);
    CHECK(client.prompts.empty());
    CHECK(result.used_extractive_fallback);
    CHECK(result.summary.find("HEAD_EVIDENCE") != std::string::npos);
    CHECK_FALSE(result.suggested_name.empty());
}

TEST_CASE("Document chunk processing respects cancellation before merging", "[document-budget]")
{
    TempDir temp;
    bool stop = false;
    DocumentClient client;
    client.maximum = client.allocation_limit = 2048;
    client.after_completion = [&] { stop = true; };
    DocumentTextAnalyzer::Settings settings;
    settings.should_stop = [&] { return stop; };
    CHECK_THROWS(DocumentTextAnalyzer(settings).analyze(source_file(temp, std::string(5000, '8')), client));
    CHECK(client.prompts.size() == 1);
}

TEST_CASE("Document extraction limits never leave a partial UTF-8 code point", "[document-budget]")
{
    TempDir temp;
    DocumentTextAnalyzer::Settings settings;
    settings.max_characters = 5;
    DocumentClient client;
    DocumentTextAnalyzer(settings).analyze(source_file(temp, "日本語"), client);
    REQUIRE(client.prompts.size() == 1);
    CHECK(input_evidence(client.prompts[0]) == "日");
}

#if defined(AI_FILE_SORTER_USE_LIBZIP)
TEST_CASE("Office XML extraction reaches text beyond large formatting definitions", "[document-budget][document-xml]")
{
    const auto format = GENERATE(std::string("odp"), std::string("docx"), std::string("xlsx"), std::string("pptx"));
    TempDir temp;
    const auto path = temp.path() / ("styled." + format);
    const auto styles = std::string(600000, 'a');
    const std::string paragraph = "HEAD_EVIDENCE Revenue &amp; Budget TAIL_EVIDENCE";
    if (format == "odp") archive_file(path, "content.xml",
        "<office:document-content xmlns:office='urn:oasis:names:tc:opendocument:xmlns:office:1.0' "
        "xmlns:text='urn:oasis:names:tc:opendocument:xmlns:text:1.0'><office:automatic-styles name='" + styles +
        "'/><office:body><office:presentation><text:p>" + paragraph + "</text:p></office:presentation></office:body></office:document-content>");
    else if (format == "docx") archive_file(path, "word/document.xml",
        "<w:document xmlns:w='http://schemas.openxmlformats.org/wordprocessingml/2006/main'><w:background name='" + styles +
        "'/><w:body><w:p><w:r><w:t>" + paragraph + "</w:t></w:r></w:p></w:body></w:document>");
    else if (format == "xlsx") archive_file(path, "xl/sharedStrings.xml",
        "<sst xmlns='http://schemas.openxmlformats.org/spreadsheetml/2006/main'><ignored name='" + styles +
        "'/><si><t>" + paragraph + "</t></si></sst>");
    else archive_file(path, "ppt/slides/slide1.xml",
        "<p:sld xmlns:p='http://schemas.openxmlformats.org/presentationml/2006/main' "
        "xmlns:a='http://schemas.openxmlformats.org/drawingml/2006/main'><p:extLst name='" + styles +
        "'/><a:p><a:r><a:t>" + paragraph + "</a:t></a:r></a:p></p:sld>");
    DocumentClient client;
    const auto result = DocumentTextAnalyzer().analyze(path, client);
    REQUIRE(client.prompts.size() == 1);
    CHECK(input_evidence(client.prompts[0]) == "HEAD_EVIDENCE Revenue & Budget TAIL_EVIDENCE");
    CHECK(result.summary.find("TAIL_EVIDENCE") != std::string::npos);
}

TEST_CASE("Office XML limits apply to useful UTF-8 text rather than archive prefixes", "[document-budget][document-xml]")
{
    TempDir temp;
    const auto path = temp.path() / "large.odp";
    archive_file(path, "content.xml",
        "<office:document-content xmlns:office='urn:oasis:names:tc:opendocument:xmlns:office:1.0' "
        "xmlns:text='urn:oasis:names:tc:opendocument:xmlns:text:1.0'><office:automatic-styles name='" + std::string(300000, 'a') +
        "'/><office:body><text:p>HEAD_EVIDENCE " + std::string(10000, 'b') + " TAIL_EVIDENCE</text:p></office:body></office:document-content>");
    DocumentTextAnalyzer::Settings settings;
    settings.max_characters = 100;
    DocumentClient client;
    DocumentTextAnalyzer(settings).analyze(path, client);
    REQUIRE(client.prompts.size() == 1);
    CHECK(input_evidence(client.prompts[0]).size() == 100);
    CHECK(client.prompts[0].find("HEAD_EVIDENCE") != std::string::npos);
    CHECK(client.prompts[0].find("TAIL_EVIDENCE") == std::string::npos);
}
#endif
