#pragma once

#include "CategoryLanguage.hpp"

#include <memory>
#include <string>

namespace spdlog { class logger; }

class ILLMClient;

/**
 * @brief Localizes suggested filenames into the selected category language.
 *
 * The service preserves the original file extension, keeps filenames
 * filesystem-friendly, and falls back to the original suggestion if the
 * translation response is empty or malformed.
 */
class FilenameLocalizationService {
public:
    /**
     * @brief Optional tuning knobs for localized filename generation.
     */
    struct Options {
        /**
         * @brief Maximum number of words allowed in the localized filename stem.
         *
         * A value of `0` derives the limit from the source stem automatically.
         */
        std::size_t max_words{0};
        /**
         * @brief Maximum number of UTF-16 code units allowed in the localized filename stem.
         *
         * A value of `0` derives a conservative limit from the source stem automatically.
         */
        std::size_t max_length{0};
    };

    /**
     * @brief Constructs the filename localizer.
     * @param logger Optional logger used for translation diagnostics.
     */
    explicit FilenameLocalizationService(std::shared_ptr<spdlog::logger> logger = nullptr);

    /**
     * @brief Localizes a suggested filename into the selected language.
     * @param suggested_name Canonical suggested filename, including extension when present.
     * @param language Target language for localization.
     * @param llm LLM client used for the translation request.
     * @return Localized filename, or the original suggestion on failure.
     */
    std::string localize_filename(const std::string& suggested_name,
                                  CategoryLanguage language,
                                  ILLMClient& llm) const;

    /**
     * @brief Localizes a suggested filename into the selected language.
     * @param suggested_name Canonical suggested filename, including extension when present.
     * @param language Target language for localization.
     * @param llm LLM client used for the translation request.
     * @param options Optional filename-length constraints.
     * @return Localized filename, or the original suggestion on failure.
     */
    std::string localize_filename(const std::string& suggested_name,
                                  CategoryLanguage language,
                                  ILLMClient& llm,
                                  Options options) const;

private:
    /**
     * @brief Builds the translation prompt for a filename stem.
     * @param stem Source filename stem without its extension.
     * @param language Target language for translation.
     * @param max_words Maximum number of words allowed in the result.
     * @return Prompt text for the LLM.
     */
    std::string build_prompt(const std::string& stem,
                             CategoryLanguage language,
                             std::size_t max_words) const;
    /**
     * @brief Normalizes a translated filename stem into lowercase underscore-separated text.
     * @param value Raw LLM response.
     * @param max_words Maximum number of words allowed in the result.
     * @param max_length Maximum stem length allowed in the result.
     * @return Sanitized filename stem, or an empty string when normalization fails.
     */
    static std::string sanitize_stem(const std::string& value,
                                     std::size_t max_words,
                                     std::size_t max_length);

    std::shared_ptr<spdlog::logger> logger_;
};
