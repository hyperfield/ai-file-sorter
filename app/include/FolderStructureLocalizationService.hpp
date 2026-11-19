#pragma once

#include "CategoryLanguage.hpp"
#include "FolderStructureTemplates.hpp"

#include <atomic>

class ILLMClient;

/** @brief Translates starter labels without allowing the model to change paths or numbering. */
class FolderStructureLocalizationService {
public:
    /**
     * @brief Translate a complete template, or throw without returning a partial structure.
     * @param source Original template; English returns it unchanged.
     * @param language Requested folder-name language.
     * @param llm Selected text model, borrowed for this operation.
     * @param stop Optional cooperative cancellation flag checked between bounded requests.
     * @return A descriptor whose paths preserve the source hierarchy and numeric prefixes.
     */
    static FolderStructureTemplates::Descriptor localize(
        const FolderStructureTemplates::Descriptor& source, CategoryLanguage language,
        ILLMClient& llm, const std::atomic<bool>* stop = nullptr);
};
