#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "CategorizationService.hpp"

/**
 * @brief Test-only accessors for CategorizationService internals.
 */
class CategorizationServiceTestAccess {
public:
    /**
     * @brief Returns the whitelist context string used in prompts.
     * @param service CategorizationService instance under test.
     * @return Prompt snippet describing allowed categories/subcategories.
     */
    static std::string build_whitelist_context(const CategorizationService& service) {
        return service.build_whitelist_context();
    }

    /**
     * @brief Returns the category-language context string used in prompts.
     * @param service CategorizationService instance under test.
     * @return Prompt snippet describing the required category language.
     */
    static std::string build_category_language_context(const CategorizationService& service) {
        return service.build_category_language_context();
    }

    /**
     * @brief Returns the fully combined prompt context for a specific entry shape.
     * @param service CategorizationService instance under test.
     * @param hint_block Optional consistency-hint block.
     * @param prompt_name Prompt-facing file name.
     * @param prompt_path Prompt-facing path/context payload.
     * @param file_type File or directory.
     * @return Combined prompt context with language/whitelist/type-specific guidance.
     */
    static std::string build_combined_context(const CategorizationService& service,
                                              const std::string& hint_block,
                                              const std::string& prompt_name,
                                              const std::string& prompt_path,
                                              FileType file_type = FileType::File) {
        return service.build_combined_context(hint_block, prompt_name, prompt_path, file_type);
    }

    /**
     * @brief Returns the resolved remote request limit used for pacing.
     * @param service CategorizationService instance under test.
     * @return Requests per minute, or 0 when pacing is disabled.
     */
    static int resolve_remote_requests_per_minute(const CategorizationService& service) {
        return service.resolve_remote_requests_per_minute();
    }
};

#endif // AI_FILE_SORTER_TEST_BUILD
