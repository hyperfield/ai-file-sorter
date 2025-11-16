#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "CategorizationService.hpp"

class CategorizationServiceTestAccess {
public:
    static std::string build_whitelist_context(const CategorizationService& service) {
        return service.build_whitelist_context();
    }

    static std::string build_category_language_context(const CategorizationService& service) {
        return service.build_category_language_context();
    }
};

#endif // AI_FILE_SORTER_TEST_BUILD

