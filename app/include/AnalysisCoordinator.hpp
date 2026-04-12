#pragma once

#include <string>

class MainApp;

/**
 * @brief Owns the end-to-end folder analysis workflow previously hosted in MainApp.
 */
class AnalysisCoordinator {
public:
    explicit AnalysisCoordinator(MainApp& app);

    void execute();

    /**
     * @brief Resolve the prompt filename used for document categorization.
     * @param original_name Original file name.
     * @param suggested_name Suggested file name, when available.
     * @return Suggested name when present; otherwise the original name.
     */
    static std::string resolve_document_prompt_name(const std::string& original_name,
                                                    const std::string& suggested_name);
    /**
     * @brief Build the image prompt path used for categorization.
     * @param full_path Original full path to the image.
     * @param prompt_name File name to use in the categorization prompt.
     * @param description Optional visual description appended for the LLM prompt.
     * @return Prompt path string used for categorization.
     */
    static std::string build_image_prompt_path(const std::string& full_path,
                                               const std::string& prompt_name,
                                               const std::string& description);
    /**
     * @brief Build the document prompt path used for categorization.
     * @param full_path Original full path to the document.
     * @param prompt_name File name to use in the categorization prompt.
     * @param summary Optional summary appended for the LLM prompt.
     * @return Prompt path string used for categorization.
     */
    static std::string build_document_prompt_path(const std::string& full_path,
                                                  const std::string& prompt_name,
                                                  const std::string& summary);

private:
    MainApp& app_;
};
