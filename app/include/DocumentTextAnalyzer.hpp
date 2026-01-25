#pragma once

#include <filesystem>
#include <optional>
#include <string>

class ILLMClient;

struct DocumentAnalysisResult {
    /**
     * @brief Short summary extracted from the document text.
     */
    std::string summary;
    /**
     * @brief Suggested filename based on the analyzed content (including extension).
     */
    std::string suggested_name;
};

/**
 * @brief Extracts document text and produces LLM-based summaries and rename suggestions.
 */
class DocumentTextAnalyzer {
public:
    /**
     * @brief Tuning knobs for document excerpting and filename cleanup.
     */
    struct Settings {
        /**
         * @brief Maximum characters to include in the excerpt sent to the LLM.
         */
        size_t max_characters = 8000;
        /**
         * @brief Maximum number of words to keep in the suggested filename.
         */
        size_t max_filename_words = 3;
        /**
         * @brief Maximum length of the suggested filename stem.
         */
        size_t max_filename_length = 50;
        /**
         * @brief Maximum number of tokens to generate for the response.
         */
        int max_tokens = 256;
    };

    /**
     * @brief Construct with default settings.
     */
    DocumentTextAnalyzer();
    /**
     * @brief Construct with custom settings.
     */
    explicit DocumentTextAnalyzer(Settings settings);

    /**
     * @brief Analyze the document and return a summary + suggested filename.
     */
    DocumentAnalysisResult analyze(const std::filesystem::path& document_path,
                                   ILLMClient& llm) const;

    /**
     * @brief Returns true if the file extension is supported for document analysis.
     */
    static bool is_supported_document(const std::filesystem::path& path);
    /**
     * @brief Attempts to read a creation date from supported document metadata.
     */
    static std::optional<std::string> extract_creation_date(const std::filesystem::path& path);

private:
    std::string extract_text(const std::filesystem::path& path) const;
    std::string build_prompt(const std::string& excerpt,
                             const std::string& file_name) const;
    std::string sanitize_filename(const std::string& value,
                                  size_t max_words,
                                  size_t max_length) const;

    static std::string normalize_filename(const std::string& base,
                                          const std::filesystem::path& original_path);
    static std::string trim(std::string value);
    static std::string slugify(const std::string& value);

    Settings settings_;
};
