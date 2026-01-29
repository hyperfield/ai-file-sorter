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
};

#endif // AI_FILE_SORTER_TEST_BUILD
